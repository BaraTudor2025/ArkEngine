#pragma once

#include <unordered_map>
#include <fstream>
#include <typeindex>
#include <optional>
#include <bitset>
#include <tuple>
#include <set>
#include <array>
#include <span>
#include <ranges>

#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/core/Signal.hpp"

namespace ark {

	class ProxyEntitiesView;
	class ProxyRuntimeComponentView;

	template <typename...> class View;

	using EntityId = int;

	namespace detail
	{ 
		inline auto& s_counter() {
			static std::size_t c_counter;
			return c_counter;
		}
		inline auto& s_ids() {
			static std::vector<std::type_index> c_ids;
			return c_ids;
		}
	}

	class EntityManager final {
		template<typename T> static inline const std::size_t s_compId = []() { 
			detail::s_ids().push_back(typeid(T)); 
			ark::meta::type<T>();
			return detail::s_counter()++;
		}();
	public:
		EntityManager(			
			std::pmr::memory_resource* upstreamComponent = std::pmr::new_delete_resource())
			: m_componentPool(std::make_unique<std::pmr::unsynchronized_pool_resource>(
				std::pmr::pool_options{.max_blocks_per_chunk = 100, .largest_required_pool_block = 1024},
				upstreamComponent))
		{
			m_componentsNum = detail::s_counter();
			int i = 0;
			for (auto& type : componentIds()) {
				if (i < detail::s_counter())
					type = detail::s_ids()[i];
				else
					type = typeid(void);
				i++;
			}
		}

		EntityManager(EntityManager&&) noexcept = default;

		EntityManager(const EntityManager&) = delete;

		auto createEntity() -> Entity
		{
			EntityId id;
			if (m_freeEntities.size() > 0) {
				id = m_freeEntities.back();
				m_freeEntities.pop_back();
				m_isFree[id] = false;
			} else {
				id = m_entities.size();
				m_entities.emplace_back();
				m_isFree.push_back(false);
				m_masks.emplace_back();
			}
			auto e = Entity(id, this);
			auto& entity = getEntity(e);
			entity.id = id;
			m_signalCreate.publish(*this, e);
			return e;
		}

		/* First, construct each component with default or copy constructor
		 * Then, call the clone signals for each component that has them
		*/
		auto clone(ark::EntityId toClone) -> ark::Entity {
			auto clone = createEntity();
			eachComponent(toClone, [&](RuntimeComponent comp) {
				add(clone, comp.type, toClone);
			});
			eachComponent(toClone, [&](RuntimeComponent comp) {
				signalTable(m_tableClone, comp.type, clone, ark::Entity{ toClone, this });
			});
			return clone;
		}

		bool isValid(EntityId entity) const {
			return entity != ArkInvalidID && entity >= 0 && !m_isFree[entity];
		}

		void reserveEntities(int num) {
			m_entities.reserve(num);
			m_isFree.reserve(num);
			m_masks.reserve(num);
		}

		/* function type should be void(EntityManager&, Entity/EntityId)
		*/
		auto onCreate() {
			return Sink{ m_signalCreate };
		}

		auto onDestroy() {
			return Sink{ m_signalDestroy };
		}

		template <ConceptComponent T>
		auto onAdd() {
			return Sink{ m_tableAdd[typeid(T)] };
		}

		template <ConceptComponent T>
		auto onClone() {
			return Sink{ m_tableClone[typeid(T)] };
		}

		template <ConceptComponent T>
		auto onRemove() {
			return Sink{ m_tableRemove[typeid(T)] };
		}

		/* for .patch<T>(auto& comp) { comp.mem = nush; }
		 * or for .get<non-const Component>()
		 * ???
		*/
		//template <ConceptComponent T>
		//auto onUpdate() {
		//	return Sink{ m_tableUpdate[typeid(T)]; };
		//}

		/* function type should be void(EntityManager&, EntityId, std::type_index componenetType) 
		*/
		auto onAdd(){
			return Sink{ m_signalAdd };
		}
		auto onRemove(){
			return Sink{ m_signalRemove };
		}

		void destroyEntity(EntityId entityId)
		{
			m_signalDestroy.publish(*this, Entity{ entityId, this });
			this->eachComponent(entityId, [&, this](RuntimeComponent comp) {
				this->remove(entityId, comp.type);
			});
			m_isFree[entityId] = true;
			m_freeEntities.push_back(entityId);
		}

		template <ConceptComponent... Ts>
		auto view() noexcept -> ark::View<Ts...>;

		template <ConceptComponent... Ts>
		operator ark::View<Ts...>() noexcept {
			return this->view<Ts...>();
		}

		// if component already exists, then the existing component is returned
		// if no ctor-args are provided then it's default constructed
		template <ConceptComponent T, typename... Args>
		T& add(EntityId entityId, Args&&... args) {
			return implStaticAdd<T>(entityId, std::forward<Args>(args)...);
		}

		/* default-constructs the component or copy-constructs it if an aditional entity is provided to copy from
		*/
		void* add(EntityId entityId, std::type_index type, EntityId toCopy = ArkInvalidID) {
			addType(type);
			return implRuntimeAdd(entityId, type, toCopy);
		}

		/* copy-constructs from 'toClone' and calls the clone signal
		*/ 
		void* clone(EntityId entityId, std::type_index type, Entity toClone) {
			void* ptr = add(entityId, type, toClone);
			signalTable(m_tableClone, type, Entity{ entityId, this }, Entity{ toClone, this });
			return ptr;
		}

		void* get(EntityId entityId, std::type_index type) const
		{
			auto& entity = getEntity(entityId);
			int compId = idFromType(type);
#if !NDEBUG
			if (compId == ArkInvalidID || !entity.mask.test(compId)) {
				EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%d), doesn't have component (%s)", entityId, type.name());
				return nullptr;
			}
#endif
			return entity.components[compId].ptr;
		}

		template <typename T>
		T& get(EntityId entityId) const noexcept {
			return *tryGet<T>(entityId);
		}

		template <typename T>
		T* tryGet(EntityId entityId) const noexcept {
			return static_cast<T*>(m_entities[entityId].components[idFromType<T>()].ptr);
		}


		template <typename... Ts>
		bool has(EntityId entity) const noexcept {
			return ((this->tryGet<Ts>(entity) != nullptr) && ...);
		}

		bool has(EntityId entity, std::type_index type) const {
			return this->get(entity, type) != nullptr;
		}

		bool has(EntityId entity, std::span<std::type_index> types) const {
			return std::all_of(types.begin(), types.end(), [&](auto type) { return has(entity, type); });
		}

		bool has(EntityId entity, ComponentMask compIds) const {
			return (mask(entity) & compIds) == compIds;
		}

		template <typename T>
		void remove(EntityId entityId) {
			remove(entityId, typeid(T));
		}

		void remove(EntityId entityId, std::type_index type)
		{
			auto& entity = getEntity(entityId);
			auto compId = idFromType(type);
			if (entity.mask.test(compId)) {
				signalTable(m_tableRemove, type, *this, Entity{ entityId, this });
				m_signalRemove.publish(*this, Entity{ entityId, this }, type);
				entity.mask.set(compId, false);
				m_masks[entityId].set(compId, false);
				destroyComponent(type, entity.components[compId].ptr);
				entity.components[compId] = {};
			}
		}

		auto mask(EntityId entityId) const -> ComponentMask
		{
			return getEntity(entityId).mask;
		}

		auto each() -> ProxyEntitiesView;
		auto eachComponent(EntityId entity) -> ProxyRuntimeComponentView;

		template <std::invocable<EntityId> F>
		void each(F&& fun) {
			for (int i = 0; i < m_entities.size(); i++) {
				if (this->isValid(i))
					fun(i);
			}
		}

		template <std::invocable<RuntimeComponent> F>
		void eachComponent(EntityId entityId, F&& fun)
		{
			auto& entity = getEntity(entityId);
			for (int i = 0; i < entity.mask.size(); ++i)
				if (entity.mask.test(i))
					fun(entity.components[i]);
		}

		//auto eachComponent(EntityId entity) {
		//	return this->m_entities[entity].components
		//		| std::views::filter([](const auto& compData) { return compData.ptr != nullptr; })
		//		| std::views::transform([](auto& compData) {return RuntimeComponent(compData.type, compData.ptr); }); // poate nu am nevoie de asta
		//}

		//auto each() {
		//	return this->m_entities
		//		| std::views::filter([this](const auto& entityData) { return !this->m_isFree[entityData.id]; })
		//		| std::views::transform([this](auto& data) { return ark::Entity(data.id, this); });
		//}

		template <typename... Ts>
		void idFromType(ComponentMask& mask) const
		{
			static_assert(sizeof...(Ts) != 0);
			(mask.set(this->idFromType<Ts>()), ...);
		}

		template <ConceptComponent T>
		int idFromType() const {
			return s_compId<std::remove_const_t<T>>;
		}

		int idFromType(std::type_index type) const
		{
			auto ids = getTypes();
			auto pos = std::find(ids.begin(), ids.end(), type);
			if (pos == ids.end()) {
				EngineLog(LogSource::ComponentM, LogLevel::Warning, "type not found (%s) ", type.name());
				return ArkInvalidIndex;
			}
			else 
				return pos - ids.begin();
		}

		auto typeFromId(int id) const -> std::type_index {
			return componentIds()[id];
		}

		void addType(std::type_index type)
		{
			if (idFromType(type) != ArkInvalidIndex)
				return;
			EngineLog(LogSource::ComponentM, LogLevel::Info, "adding type (%s)", type.name());
			if (m_componentsNum == MaxComponentTypes) {
				EngineLog(LogSource::ComponentM, LogLevel::Error, 
					"aborting... nr max of components is &d, trying to add type (%s), no more space", MaxComponentTypes, type.name());
				// TODO: add abort with grace
				std::abort();
			}
			this->componentIds()[m_componentsNum++] = type;
		}

		auto getTypes() const -> std::span<const std::type_index>
		{
			return { componentIds().data(), static_cast<std::size_t>(m_componentsNum) };
		}

#if 0 // disable entity children
		void addChildTo(Entity p, Entity c)
		{
			if (!isValidEntity(p))
				return;
			if (!isValidEntity(c)) {
				std::cerr << "above entity is an invalid child";
				return;
			}

			getEntity(c).parentIndex = p.id;
			auto parent = getEntity(p);
			if (p.hasChildren()) {
				childrenTree.at(parent.childrenIndex).push_back(c);
			} else {
				childrenTree.emplace_back();
				parent.childrenIndex = childrenTree.m_size() - 1;
				childrenTree.back().push_back(c);
			}
		}

		std::optional<Entity> getParentOfEntity(Entity e)
		{
			auto entity = getEntity(e);
			if (entity.parentIndex == ArkInvalidIndex)
				return {};
			else {
				Entity parent(*this);
				parent.id = entity.parentIndex;
				return parent;
			}
		}

		std::vector<Entity> getChildrenOfEntity(Entity e, int depth)
		{
			if (!isValidEntity(e))
				return {};

			auto entity = getEntity(e);
			if (entity.childrenIndex == ArkInvalidIndex || depth == 0)
				return {};

			auto cs = childrenTree.at(entity.childrenIndex);

			if (depth == 1)
				return cs;

			std::vector<Entity> children(cs);
			for (auto child : cs) {
				if (depth == Entity::AllChilren) {
					auto grandsons = getChildrenOfEntity(child, depth); // maintain depth
					children.insert(children.end(), grandsons.begin(), grandsons.end());
				} else {
					auto grandsons = getChildrenOfEntity(child, depth - 1); // 
					children.insert(children.end(), grandsons.begin(), grandsons.end());
				}
			}
			return children;

		}

		bool entityHasChildren(Entity e)
		{
			return getEntity(e).childrenIndex == ArkInvalidIndex;
		}
#endif // disable entity children

		~EntityManager()
		{
			this->each([this](EntityId id){
				this->destroyEntity(id);
			});
		}

	private:

		void* allocateComponent(EntityId entityId, int compId, std::size_t size, std::size_t align, std::type_index type) {
			auto& entity = getEntity(entityId);
			entity.mask.set(compId);
			m_masks[entityId].set(compId);
			void* newComponent = m_componentPool->allocate(size, align);
			entity.components[compId] = { type, newComponent };
			return newComponent;
		}

		template <typename T, typename... Args>
		T& implStaticAdd(EntityId entityId, Args&&... args) {
			int compId = idFromType<T>();
			auto& entity = getEntity(entityId);
			if (entity.mask[compId])
				return *static_cast<T*>(entity.components[compId].ptr);

			void* newComponent = allocateComponent(entityId, compId, sizeof(T), alignof(T), typeid(T));
			std::construct_at<T>((T*)newComponent, std::forward<Args>(args)...);

			signalTable(m_tableAdd, typeid(T), *this, Entity{ entityId, this });
			m_signalAdd.publish(*this, Entity{ entityId, this }, typeid(T));
			return *static_cast<T*>(newComponent);
		}

		void* implRuntimeAdd(EntityId entityId, std::type_index type, EntityId toClone)
		{
			int compId = idFromType(type);
			auto& entity = getEntity(entityId);
			if (entity.mask.test(compId))
				return entity.components[compId].ptr;

			auto metadata = meta::resolve(type);
			void* newComponent = allocateComponent(entityId, compId, metadata->size, metadata->align, type);

			const void* compToClone = isValid(toClone) ? get(toClone, type) : nullptr;
			if(compToClone && metadata->copy_constructor)
				metadata->copy_constructor(newComponent, compToClone);
			else
				metadata->default_constructor(newComponent);
			signalTable(m_tableAdd, type, *this, Entity{ entityId, this });
			m_signalAdd.publish(*this, Entity{ entityId, this }, type);
			return newComponent;
		}

		using compIds_t = std::array<std::type_index, MaxComponentTypes>;

		const compIds_t& componentIds() const {
			return *reinterpret_cast<const compIds_t*>(&m_storageCompIDs);
		}

		compIds_t& componentIds() {
			return *reinterpret_cast<compIds_t*>(&m_storageCompIDs);
		}

		void destroyComponent(std::type_index type, void* component)
		{
			auto metadata = meta::resolve(type);
			if(metadata->destructor)
				metadata->destructor(component);
			m_componentPool->deallocate(component, metadata->size, metadata->align);
		}

		template <typename Table, typename... Args>
		void signalTable(Table& table, std::type_index type, Args&&... args) {
			if (auto it = table.find(type); it != table.end()) {
				auto& func = it->second;
				func.publish(std::forward<Args>(args)...);
			}
		}

		struct InternalEntityData {
			ComponentMask mask;
			int id = ArkInvalidID;
			std::array<RuntimeComponent, MaxComponentTypes> components;
		};

		auto getEntity(EntityId id) -> InternalEntityData&
		{
			return m_entities.at(id);
		}
		auto getEntity(EntityId id) const -> const InternalEntityData&
		{
			return m_entities.at(id);
		}

		using ComponentVector = decltype(InternalEntityData::components);

	private:
		using storageCompIds_t = std::aligned_storage_t<sizeof(compIds_t), alignof(compIds_t)>;
		int m_componentsNum = 0;
		storageCompIds_t m_storageCompIDs;
		std::unique_ptr<std::pmr::unsynchronized_pool_resource> m_componentPool;
		std::vector<InternalEntityData> m_entities;
		std::vector<ComponentMask> m_masks; // used by View
		std::vector<Entity::ID> m_freeEntities; // as putea folosi un implicit list (int m_nextFree;) vezi entt: you dont have to store free entitites sau ceva de genu
		int m_nextFree = ArkInvalidIndex;
		std::vector<bool> m_isFree; // pentru verificate rapida

		Signal<void(EntityManager&, Entity)> m_signalCreate;
		Signal<void(EntityManager&, Entity)> m_signalDestroy;
		Signal<void(EntityManager&, Entity, std::type_index)> m_signalAdd; // any comp. add, type_index is type of component added
		Signal<void(EntityManager&, Entity, std::type_index)> m_signalRemove; // analog

		template <typename F>
		using SignalTable = std::unordered_map<std::type_index, Signal<F>>;

		SignalTable<void(EntityManager&, Entity)> m_tableAdd;
		SignalTable<void(EntityManager&, Entity)> m_tableRemove;
		SignalTable<void(Entity, Entity)> m_tableClone;

		friend struct ProxyRuntimeComponentIterator;
		friend struct ProxyEntityIterator;
		friend class ProxyRuntimeComponentView;
		friend class ProxyEntitiesView;
		friend class Entity;
		template <typename...> friend class View;
		template <bool, typename...> friend class IteratorView;
		template <bool, typename...> friend class ProxyView;
		template <typename...> friend struct IdTable;
	};


	template <bool bRetEnt=false, typename... Cs>
	class IteratorView {
		using Iter = decltype(EntityManager::m_entities)::iterator;
		using Self = IteratorView;
		EntityManager* m_manager;
		ComponentMask m_mask;
		Iter m_iter;
		decltype(EntityManager::m_masks)::iterator m_iterMask;
	public:

		IteratorView(Iter iter, ComponentMask mask, EntityManager* man)
			: m_iter(iter), m_mask(mask), m_manager(man)
		{
			m_iterMask = m_manager->m_masks.begin();
			if (m_iter != m_manager->m_entities.end() && (*m_iterMask & m_mask) != m_mask)
				this->operator++();
		}

		auto operator++() noexcept {
			++m_iter;
			while (m_iter < m_manager->m_entities.end() && (*(++m_iterMask) & m_mask) != m_mask) {
				++m_iter;
			}
			return *this;
		}

		[[nodiscard]]
		decltype(auto) operator*() noexcept {

			if constexpr (sizeof...(Cs) == 0) {
				return ark::Entity{ m_iter->id, m_manager };
			}
			else if constexpr (bRetEnt) {
				auto entity = ark::Entity{ m_iter->id, m_manager };
				return std::tuple<ark::Entity, Cs&...>(entity, m_manager->get<Cs>(m_iter->id)...);
			}
			else {
				if constexpr (sizeof...(Cs) == 1)
					return (m_manager->get<Cs>(m_iter->id), ...);
				else
					return std::tuple<Cs&...>(m_manager->get<Cs>(m_iter->id)...);
			}
		}

		friend bool operator==(const Self& a, const Self& b) noexcept
		{
			return a.m_iter == b.m_iter;
		}

		friend bool operator!=(const Self& a, const Self& b) noexcept
		{
			return a.m_iter != b.m_iter;
		}
	};

	template <bool bRetEnt, typename... Cs>
	class ProxyView {
		EntityManager* m_manager;
		ComponentMask m_mask;

	public:
		ProxyView(ComponentMask mask, EntityManager* man) 
			: m_mask(mask), m_manager(man) { }

		auto begin() {
			return IteratorView<bRetEnt, Cs...>(m_manager->m_entities.begin(), m_mask, m_manager);
		}
		auto end() {
			return IteratorView<bRetEnt, Cs...>(m_manager->m_entities.end(), m_mask, m_manager);
		}
	};

	/*
	* Folosire:
	* auto view = manager.view<Comp1, ...>();
	* 
	* for(auto [c1, ...] : view)
	* for(auto [ent, c1, ...] : view.each())
	*
	* custom args:
	* for(auto [c1, ...] : view.each<C1...>()
	* for(auto [ent, c1, ...] : view.each<Entity, C1...>())
	* for(auto ent : view.each<Entity>())
	*/
	template <typename... Cs>
	class View {
		ComponentMask m_mask;
		EntityManager* m_manager = nullptr;
	public:

		View() = default;

		View(EntityManager& man) : m_manager(&man) { 
			m_manager->idFromType<Cs...>(m_mask);
		}

		auto begin() noexcept {
			return IteratorView<false, Cs...>(m_manager->m_entities.begin(), m_mask, m_manager);
		}
		auto end() noexcept {
			return IteratorView<false, Cs...>(m_manager->m_entities.end(), m_mask, m_manager);
		}

		auto each() {
			return ProxyView<true, Cs...>(m_mask, m_manager);
		}

		//auto filter_view() {
		//	return m_manager->entities 
		//		| std::views::filter([this](const auto& entity) { return (entity.mask & this->m_mask) == this->m_mask; });
		//}

		//auto each() {
		//	return filer_view()| std::views::transform([this](auto& entity) {
		//		return std::tuple<ark::Entity, Cs&...>(ark::Entity(entity.id, this->m_manager), *m_ids.get<Cs>()...);
		//	});
		//}

		template <std::same_as<ark::Entity> TEntity, ConceptComponent...  Ts>
		auto each() {
			if constexpr (sizeof...(Ts) == 0)
				return ProxyView<true>(m_mask, m_manager);
			else
				return ProxyView<true, Ts...>(m_mask, m_manager);
		}

		template <ConceptComponent T, ConceptComponent... Ts>
		requires (!std::same_as<ark::Entity, T>)
		auto each() {
			return ProxyView<false, T, Ts...>(m_mask, m_manager);
		}

		template <ConceptComponent... Ts>
		decltype(auto) get(ark::EntityId entity) {
			if constexpr (sizeof...(Ts) == 1)
				return ((m_manager->get<Ts>(entity)), ...);
			else if constexpr (sizeof...(Ts) > 1)
				return std::tuple<Ts&...>(m_manager->get<Ts>(entity)...);
			else if constexpr (sizeof...(Cs) == 1)
				return ((m_manager->get<Cs>(entity)), ...);
			else
				return std::tuple<Cs&...>(m_manager->get<Cs>(entity)...);
		}

		// daca 'fun' returneaza un bool atunci: true-continue/ false-break
		template <typename F>
		void each(F&& fun) noexcept {
			int index = 0;
			for (auto& data : m_manager->m_entities) {
				if (!m_manager->m_isFree[index] && (data.mask & m_mask) == m_mask) {
					auto entity = ark::Entity{ data.id, m_manager };

					if constexpr (std::invocable<F, ark::Entity>) {
						if constexpr (not std::convertible_to<decltype(fun(entity)), bool>)
							fun(entity);
						else
							if (!fun(entity))
								break;
					}
					else if constexpr (std::invocable<F, ark::Entity, Cs&...>) {
						if constexpr (not std::convertible_to<decltype(fun(entity, m_manager->get<Cs>(entity)...)), bool>)
							fun(entity, m_manager->get<Cs>(entity)...);
						else
							if (!fun(entity, m_manager->get<Cs>(entity)...))
								break;
					}
					else if constexpr (std::invocable<F, Cs&...>) {
						if constexpr (not std::convertible_to<decltype(fun(m_manager->get<Cs>(entity)...)), bool>)
							fun(m_manager->get<Cs>(entity)...);
						else
							if (!fun(m_manager->get<Cs>(entity)...))
								break;
					}
					else
						static_assert(std::invocable<F, ark::Entity>, "View.each error: callback-ul are argumnete gresite");
				}
				index++;
			}
		}
	};

	template <ConceptComponent... Ts>
	inline auto EntityManager::view() noexcept -> ark::View<Ts...> {
		return View<Ts...>(*this);
	}

	struct ProxyRuntimeComponentIterator {
		using InternalIterator = decltype(EntityManager::InternalEntityData::components)::iterator;
		InternalIterator m_iter;
		InternalIterator m_end;
	public:
		auto& operator++()
		{
			++m_iter;
			while (m_iter < m_end && !m_iter->ptr)
				++m_iter;
			return *this;
		}

		ProxyRuntimeComponentIterator(decltype(m_iter) iter, decltype(m_end) mEnd) :m_iter(iter), m_end(mEnd) { 
			if (m_iter != m_end && !m_iter->ptr)
				++(*this);
		}


		RuntimeComponent operator*()
		{
			return *m_iter;
		}

		friend bool operator==(const ProxyRuntimeComponentIterator& a, const ProxyRuntimeComponentIterator& b) noexcept
		{
			return a.m_iter == b.m_iter;
		}

		friend bool operator!=(const ProxyRuntimeComponentIterator& a, const ProxyRuntimeComponentIterator& b) noexcept
		{
			return a.m_iter != b.m_iter;
		}
	};

	class ProxyRuntimeComponentView {
		EntityManager::InternalEntityData& m_data;
	public:
		ProxyRuntimeComponentView(decltype(m_data) data) : m_data(data) {}
		auto begin() -> ProxyRuntimeComponentIterator { return { m_data.components.begin(), m_data.components.end() }; }
		auto end() -> ProxyRuntimeComponentIterator { return { m_data.components.end(), m_data.components.end() }; }
	};

	struct ProxyEntityIterator {
		using InternalIter = decltype(EntityManager::m_entities)::iterator;
		mutable InternalIter m_iter;
		mutable InternalIter m_end;
		EntityManager& m_manager;
	public:

		auto& operator++() noexcept
		{
			++m_iter;
			while(m_iter < m_end && m_manager.m_isFree[m_iter->id])
				++m_iter;
			return *this;
		}

		ProxyEntityIterator(InternalIter iter, InternalIter end, EntityManager& manager) 
			: m_iter(iter), m_end(end), m_manager(manager) {
			if(m_iter < m_end && m_manager.m_isFree[m_iter->id])
				++(*this);
		}

		const auto& operator++() const noexcept
		{
			++m_iter;
			while(m_iter < m_end && m_manager.m_isFree[m_iter->id])
				++m_iter;
			return *this;
		}

		Entity operator*() noexcept
		{
			return { m_iter->id, &m_manager };
		}

		const Entity operator*() const noexcept
		{
			return { m_iter->id, &m_manager };
		}

		friend bool operator==(const ProxyEntityIterator& a, const ProxyEntityIterator& b) noexcept
		{
			return a.m_iter == b.m_iter;
		}

		friend bool operator!=(const ProxyEntityIterator& a, const ProxyEntityIterator& b) noexcept
		{
			return a.m_iter != b.m_iter;
		}
	};

	// live view
	class ProxyEntitiesView {
		EntityManager& m_manager;
	public:
		ProxyEntitiesView(EntityManager& m) : m_manager(m) {}

		auto begin() -> ProxyEntityIterator { return {m_manager.m_entities.begin(), m_manager.m_entities.end(), m_manager}; }
		auto end() -> ProxyEntityIterator { return {m_manager.m_entities.end(), m_manager.m_entities.end(), m_manager}; }
		auto begin() const -> const ProxyEntityIterator { return {m_manager.m_entities.begin(), m_manager.m_entities.end(), m_manager}; }
		auto end() const -> const ProxyEntityIterator { return {m_manager.m_entities.end(), m_manager.m_entities.end(), m_manager}; }
	};

	inline ProxyRuntimeComponentView EntityManager::eachComponent(EntityId entityId){
		return {getEntity(entityId)};
	}

	inline ProxyEntitiesView EntityManager::each()
	{
		return ProxyEntitiesView{*this};
	}

	/* Entity handle definitions */

	inline bool Entity::isValid() const {
		return manager != nullptr && manager->isValid(*this);
	}

	template <ConceptComponent T>
	requires std::is_aggregate_v<T>
	inline T& Entity::add(T&& comp)
	{
		return manager->add<T>(*this, std::forward<T>(comp));
	}

	template<ConceptComponent T, typename ...Args>
	inline T& Entity::add(Args&& ...args)
	{
		return manager->add<T>(*this, std::forward<Args>(args)...);
	} 

	inline void Entity::add(std::type_index type, Entity entity)
	{
		manager->add(*this, type, entity);
	}

	inline void* Entity::get(std::type_index type)
	{
		auto comp = manager->get(*this, type);
		if(!comp)
			EngineLog(LogSource::EntityM, LogLevel::Error, ">:( \n going to crash..."); // going to crash...
		return comp;
	}

	template <typename F>
	requires std::invocable<F, RuntimeComponent>
	inline void Entity::eachComponent(F&& f)
	{
		manager->eachComponent(*this, std::forward<F>(f));
	}

	template<ConceptComponent T>
	[[nodiscard]]
	inline T* Entity::tryGet()
	{
		return manager->tryGet<T>(*this);
	}

	template<ConceptComponent T>
	[[nodiscard]]
	inline const T* Entity::tryGet() const
	{
		return manager->tryGet<T>(*this);
	}

	template <ConceptComponent T>
	inline void Entity::remove()
	{
		this->remove(typeid(T));
	}

	inline void Entity::remove(std::type_index type)
	{
		manager->remove(*this, type);
	}

	inline auto Entity::eachComponent() //-> ProxyRuntimeComponentView
	{
		return manager->eachComponent(*this);
	}

	inline auto Entity::getMask() const -> ComponentMask { return manager->mask(*this); }
}
