#pragma once

#include <unordered_map>
#include <fstream>
#include <typeindex>
#include <optional>
#include <bitset>
#include <tuple>
#include <set>

#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"

namespace ark {

	class Registry;
	class EntitiesView;
	class RuntimeComponentView;

	class EntityManager final {
	public:
		EntityManager(			
			std::vector<std::pair<Entity::ID, ComponentMask>>& mComponentsAdded, 
			std::pmr::memory_resource* upstreamComponent)
			: mComponentsAdded(mComponentsAdded),
			resComponentIds(std::make_unique<std::pmr::monotonic_buffer_resource>(buffComponentIds.data(), sizeof(buffComponentIds))),
			componentIds(resComponentIds.get()),
			componentPool(std::make_unique<std::pmr::unsynchronized_pool_resource>(
				std::pmr::pool_options{.max_blocks_per_chunk = 30, .largest_required_pool_block = 1024},
				upstreamComponent))
		{
			componentIds.reserve(MaxComponentTypes);
		}

		EntityManager(EntityManager&&) noexcept = default;

		/// 
		/// use cloneManager instead
		EntityManager(const EntityManager&) = delete;

		auto createEntity() -> Entity
		{
			Entity e;
			e.manager = this;
			if (auto it = std::find(isFree.begin(), isFree.end(), true); it != isFree.end()) {
				e.id = it - isFree.begin();
				*it = false;
			} else {
				e.id = entities.size();
				entities.emplace_back();
				isFree.push_back(false);
			}
			auto& entity = getEntity(e);
			entity.id = e.id;
			EngineLog(LogSource::EntityM, LogLevel::Info, "created entity with id(%d)", entity.id);
			return e;
		}

		//EntityManager cloneThisManager(
		//	std::vector<std::pair<Entity::ID, ComponentMask>>& mComponentsAdded,
		//	std::pmr::memory_resource* upstreamComponent);

		auto cloneEntity(Entity e) -> Entity
		{
			Entity hClone = createEntity();
			auto& entity = getEntity(e);
			auto& clone = getEntity(hClone);
			clone.mask = entity.mask;
			forComponents(entity, [&, this](auto& compData) {
				auto metadata = meta::getMetadata(compData.type);
				void* newComponent = componentPool->allocate(metadata->size, metadata->align);
				if (metadata->copy_constructor) {
					metadata->copy_constructor(newComponent, compData.pComponent);
				}
				else
					metadata->default_constructor(newComponent);

				clone.components[idFromType(compData.type)] = { compData.type, newComponent };
				onConstruction(compData.type, newComponent, hClone);
				onCopy(compData.type, newComponent, compData.pComponent);
			});
			mComponentsAdded.push_back({clone.id, clone.mask});
			return hClone;
		}

		void destroyEntity(Entity e)
		{
			auto& entity = getEntity(e);
			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity with id(%d)", entity.id);
			forComponents(entity, [this](auto& compData) {
				if (compData.pComponent) {
					destroyComponent(compData.type, compData.pComponent);
					compData = {};
				}
			});
			entity.mask.reset();
			isFree[entity.id] = true;
		}

		template <typename T, typename F>
		void addOnConstruction(F&& f, Registry* reg) {
			onConstructionTable[typeid(T)] = [f = std::forward<F>(f), reg](void* ptr, ark::Entity e) {
				if constexpr (std::invocable<F, T&>)
					f(*static_cast<T*>(ptr));
				else if constexpr (std::invocable<F, T&, ark::Entity>)
					f(*static_cast<T*>(ptr), e);
				else
					f(*static_cast<T*>(ptr), e, *reg);
			};
		}

		template <typename T, typename F>
		void addOnCopy(F&& f) {
			onCopyTable[typeid(T)] = [f = std::forward<F>(f)](void* This, const void* That) {
				f(*static_cast<T*>(This), *static_cast<const T*>(That));
			};
		}

		// if component already exists, then the existing component is returned
		// if no ctor-args are provided then it's default constructed
		template <typename T, typename... Args>
		T& addComponentOnEntity(Entity e, Args&&... args)
		{
			//static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
			addComponentType(typeid(T));
			bool bDefaultConstruct = sizeof...(args) == 0 ? true : false;
			auto [comp, isAlready] = implAddComponentOnEntity(e, typeid(T), bDefaultConstruct);
			if (!isAlready) {
				if (not bDefaultConstruct)
					new(comp)T(std::forward<Args>(args)...);
				onConstruction(typeid(T), comp, e);
			}
			return *static_cast<T*>(comp);
		}

		void* addComponentOnEntity(Entity e, std::type_index type)
		{
			addComponentType(type);
			auto [comp, isAlready] = implAddComponentOnEntity(e, type, true);
			if(!isAlready)
				onConstruction(type, comp, e);
			return comp;
		}

		// returns: component pointer, exists?
		auto implAddComponentOnEntity(Entity e, std::type_index type, bool defaultConstruct, const void* otherComponent = nullptr) -> std::pair<void*, bool>
		{
			int compId = idFromType(type);
			auto& entity = getEntity(e);
			if (entity.mask.test(compId))
				return { getComponentOfEntity(e, type), true };
			entity.mask.set(compId);

			// for querries
			auto index = Util::get_index_if(mComponentsAdded, [id = e.getID()](const auto& pair) { return pair.first == id; });
			if (index == ArkInvalidIndex) {
				auto& [id, mask] = mComponentsAdded.emplace_back();
				id = entity.id;
				mask.set(compId);
			}
			else
				mComponentsAdded[index].second.set(compId);

			auto metadata = meta::getMetadata(type);
			void* newComponent = componentPool->allocate(metadata->size, metadata->align);
			if (otherComponent && metadata->copy_constructor)
				metadata->copy_constructor(newComponent, otherComponent);
			else if(defaultConstruct)
				metadata->default_constructor(newComponent);

			entity.components[compId] = { type, newComponent };
			return { newComponent, false };
		}

		void* getComponentOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			int compId = idFromType(type);
			if (compId == ArkInvalidID || !entity.mask.test(compId)) {
				EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%d), doesn't have component (%s)", entity.id, meta::getMetadata(type)->name);
				return nullptr;
			}
			return entity.components[compId].pComponent;
		}

		template <typename T>
		T* getComponentOfEntity(Entity e)
		{
			return reinterpret_cast<T*>(getComponentOfEntity(e, typeid(T)));
		}

		void removeComponentOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			auto compId = idFromType(type);
			if (entity.mask.test(compId)) {
				entity.mask.set(compId, false);
				destroyComponent(type, entity.components[compId].pComponent);
				entity.components[compId] = {};
			}
		}

		auto getComponentMaskOfEntity(Entity e) -> const ComponentMask&
		{
			auto& entity = getEntity(e);
			return entity.mask;
		}

		template <typename F>
		void forEachComponentOfEntity(Entity e, F&& f)
		{
			auto& entity = getEntity(e);
			forComponents(entity, [&](auto& compData) {
				RuntimeComponent comp;
				comp.type = compData.type;
				comp.ptr = compData.pComponent;
				f(comp);
			});
		}

		auto makeComponentView(Entity e) -> RuntimeComponentView;
		auto makeComponentViewLive(Entity e)->RuntimeComponentViewLive;
		auto entitiesView() -> EntitiesView;

		int idFromType(std::type_index type) const
		{
			const auto end = this->componentIds.end();
			auto pos = std::find(this->componentIds.begin(), end, type);
			if (pos == end) {
				EngineLog(LogSource::ComponentM, LogLevel::Warning, "type not found (%s) ", type.name());
				return ArkInvalidIndex;
			}
			else 
				return pos - this->componentIds.begin();
		}

		auto typeFromId(int id) const -> std::type_index {
			return this->componentIds[id];
		}

		void addComponentType(std::type_index type)
		{
			if (idFromType(type) != ArkInvalidIndex)
				return;
			EngineLog(LogSource::ComponentM, LogLevel::Info, "adding type (%s)", type.name());
			if (componentIds.size() == MaxComponentTypes) {
				EngineLog(LogSource::ComponentM, LogLevel::Error, 
					"aborting... nr max of components is &d, trying to add type (%s), no more space", MaxComponentTypes, type.name());
				// TODO: add abort with grace
				std::abort();
			}
			this->componentIds.emplace_back(type);
		}

		const auto& getTypes() const
		{
			return this->componentIds;
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
				parent.childrenIndex = childrenTree.size() - 1;
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
			int id = 0;
			for (auto& entity : entities) {
				// destroy components only for alive entities
				if (isFree[entity.id]) {
					forComponents(entity, [this](auto& compData) {
						auto metadata = meta::getMetadata(compData.type);
						if (metadata->destructor)
							metadata->destructor(compData.pComponent);
					});
				}
				id += 1;
			}
		}

	private:
		void destroyComponent(std::type_index type, void* component)
		{
			auto metadata = meta::getMetadata(type);
			if(metadata->destructor)
				metadata->destructor(component);
			componentPool->deallocate(component, metadata->size, metadata->align);
		}

		void onConstruction(std::type_index type, void* ptr, Entity e)
		{
			if (auto it = onConstructionTable.find(type); it != onConstructionTable.end()) {
				if (auto& func = it->second; func)
					func(ptr, e);
			}
		}
		void onCopy(std::type_index type, void* ptr, const void* that)
		{
			if (auto it = onCopyTable.find(type); it != onCopyTable.end()) {
				if (auto& func = it->second; func)
					func(ptr, that);
			}
		}

	private:
		struct InternalEntityData {
			struct ComponentData {
				std::type_index type = typeid(void);
				void* pComponent = nullptr;
			};
			ComponentMask mask;
			int16_t id = ArkInvalidID;
			std::array<ComponentData, MaxComponentTypes> components;
		};

		template <typename F>
		void forComponents(InternalEntityData& data, F&& f) {
			int i = 0;
			for (auto bits = bitsComponentFromMask(data.mask); bits; bits >>= 1, ++i)
				if (bits & 1)
					f(data.components[i]);
		}

		auto getEntity(Entity e) -> InternalEntityData&
		{
			return entities.at(e.id);
		}

		using ComponentVector = decltype(InternalEntityData::components);

	private:
		struct CompTypeWrapper {
			std::type_index type = typeid(void);
			inline operator std::type_index() const { return type; }
		};
#ifdef NDEBUG
		std::array<CompTypeWrapper, MaxComponentTypes> buffComponentIds;
#else
		// msvc mai aloca 16 bytes in debug wtf???
		std::array<CompTypeWrapper, MaxComponentTypes + 2> buffComponentIds;
#endif
		// component ID is the index
		// wrapper pentru std::type_index deoarece el nu are default ctor, asa ca i dam typeid(void)
		std::unique_ptr<std::pmr::monotonic_buffer_resource> resComponentIds;
		std::pmr::vector<CompTypeWrapper> componentIds;
		std::unique_ptr<std::pmr::unsynchronized_pool_resource> componentPool;
		std::vector<InternalEntityData> entities;
		std::vector<std::pair<Entity::ID, ComponentMask>>& mComponentsAdded;
		std::vector<bool> isFree;
		std::unordered_map<std::type_index, std::function<void(void*, Entity)>> onConstructionTable;
		std::unordered_map<std::type_index, std::function<void(void*, const void*)>> onCopyTable;

		template <bool> friend struct RuntimeComponentIterator;
		friend struct EntityIterator;
		friend class RuntimeComponentView;
		friend class RuntimeComponentViewLive;
		friend class EntitiesView;
		friend class Registry;
		friend class Entity;
	};

	template <bool isLive = false>
	struct RuntimeComponentIterator {
		using InternalIterator = decltype(EntityManager::InternalEntityData::components)::iterator;
		InternalIterator iter;
		InternalIterator mEnd;
		BitsComponentType mask;
	public:
		RuntimeComponentIterator(decltype(iter) iter, decltype(mEnd) mEnd, decltype(mask) mask) :iter(iter), mEnd(mEnd), mask(mask) {}

		RuntimeComponentIterator& operator++()
		{
			if constexpr (isLive) {
				++iter;
				while (iter < mEnd && !iter->pComponent)
					++iter;
			}
			else {
				// iterate over bits, nu e live pentru ca mask-ul este o copie 
				++iter;
				mask >>= 1;
				if (!mask)
					iter = mEnd;
				while (iter < mEnd && !(mask & 1)) {
					++iter;
					mask >>= 1;
				}
			}
			return *this;
		}

		RuntimeComponent operator*()
		{
			return { iter->type, iter->pComponent };
		}

		friend bool operator==(const RuntimeComponentIterator& a, const RuntimeComponentIterator& b) noexcept
		{
			return a.iter == b.iter;
		}

		friend bool operator!=(const RuntimeComponentIterator& a, const RuntimeComponentIterator& b) noexcept
		{
			return a.iter != b.iter;
		}
	};

	class RuntimeComponentView {
		EntityManager::InternalEntityData& mData;
	public:
		RuntimeComponentView(decltype(mData) mData) : mData(mData) {}
		auto begin() -> RuntimeComponentIterator<false> { return { mData.components.begin(), mData.components.end(), bitsComponentFromMask(mData.mask) }; }
		auto end() -> RuntimeComponentIterator<false> { return { mData.components.end(), mData.components.end(), bitsComponentFromMask(mData.mask) }; }
	};

	class RuntimeComponentViewLive {
		EntityManager::InternalEntityData& mData;
	public:
		RuntimeComponentViewLive(decltype(mData) mData) : mData(mData) {}
		auto begin() -> RuntimeComponentIterator<true> { return { mData.components.begin(), mData.components.end(), bitsComponentFromMask(mData.mask) }; }
		auto end() -> RuntimeComponentIterator<true> { return { mData.components.end(), mData.components.end(), bitsComponentFromMask(mData.mask) }; }
	};


	struct EntityIterator {
		using InternalIter = decltype(EntityManager::entities)::iterator;
		mutable InternalIter mIter;
		mutable InternalIter mEnd;
		EntityManager& mManager;
	public:
		EntityIterator(InternalIter iter, InternalIter end, EntityManager& manager) 
			:mIter(iter), mEnd(end), mManager(manager) {}

		EntityIterator& operator++() noexcept
		{
			++mIter;
			while(mIter < mEnd && mManager.isFree[mIter->id])
				++mIter;
			return *this;
		}

		const EntityIterator& operator++() const noexcept
		{
			++mIter;
			while(mIter < mEnd && mManager.isFree[mIter->id])
				++mIter;
			return *this;
		}

		Entity operator*() noexcept
		{
			return { mIter->id, &mManager };
		}

		const Entity operator*() const noexcept
		{
			return { mIter->id, &mManager };
		}

		friend bool operator==(const EntityIterator& a, const EntityIterator& b) noexcept
		{
			return a.mIter == b.mIter;
		}

		friend bool operator!=(const EntityIterator& a, const EntityIterator& b) noexcept
		{
			return a.mIter != b.mIter;
		}
	};

	// live view
	class EntitiesView {
		EntityManager& mManager;
	public:
		EntitiesView(EntityManager& m) : mManager(m) {}

		auto begin() -> EntityIterator { return {mManager.entities.begin(), mManager.entities.end(), mManager}; }
		auto end() -> EntityIterator { return {mManager.entities.end(), mManager.entities.end(), mManager}; }
		auto begin() const -> const EntityIterator { return {mManager.entities.begin(), mManager.entities.end(), mManager}; }
		auto end() const -> const EntityIterator { return {mManager.entities.end(), mManager.entities.end(), mManager}; }
	};

	inline auto EntityManager::makeComponentViewLive(Entity e) -> RuntimeComponentViewLive
	{
		return RuntimeComponentViewLive{getEntity(e)};
	}
	inline auto EntityManager::makeComponentView(Entity e) -> RuntimeComponentView
	{
		return RuntimeComponentView{getEntity(e)};
	}
	inline auto EntityManager::entitiesView() -> EntitiesView
	{
		return EntitiesView{*this};
	}

	//inline EntityManager EntityManager::cloneThisManager(
	//	std::vector<std::pair<Entity::ID, ComponentMask>>& mComponentsAdded,
	//	std::pmr::memory_resource* upstreamComponent)
	//{
	//	auto manager = EntityManager(mComponentsAdded, upstreamComponent);
	//	manager.componentIds = this->componentIds;
	//	// tre facut MANUAL?!
	//	//manager.onConstructionTable = this->onConstructionTable;
	//	for (Entity entity : this->entitiesView()) {
	//		Entity clone = manager.createEntity();
	//		for (auto [type, ptr] : entity.runtimeComponentView()) {
	//			auto [newComp, /*isAlready*/_] = manager.implAddComponentOnEntity(clone, type, false, ptr);
	//			manager.onConstruction(type, newComp, clone);
	//			// ar trebui si asta pe langa .onCtor()
	//			//manager.onCopy(type, newComp, clone, ptr);
	//		}
	//	}
	//	return manager;
	//}

	/* Entity handle definitions */

	inline bool Entity::isValid() const {
		return manager != nullptr && !manager->isFree[this->id];
	}

	template<ConceptComponent T, typename ...Args>
	inline T& Entity::addComponent(Args&& ...args)
	{
		return manager->addComponentOnEntity<T>(*this, std::forward<Args>(args)...);
	}

	template<ConceptComponent T>
	inline T& Entity::getComponent()
	{
		auto comp = manager->getComponentOfEntity<T>(*this);
		if (!comp)
			EngineLog(LogSource::EntityM, LogLevel::Error, ">:( \n going to crash..."); // going to crash...
		return *comp;
	}

	template<ConceptComponent T>
	inline const T& Entity::getComponent() const
	{
		auto comp = manager->getComponentOfEntity<T>(*this);
		if (!comp)
			EngineLog(LogSource::EntityM, LogLevel::Error, ">:( \n going to crash..."); // going to crash...
		return *comp;
	}

	inline void* Entity::getComponent(std::type_index type)
	{
		auto comp = manager->getComponentOfEntity(*this, type);
		if(!comp)
			EngineLog(LogSource::EntityM, LogLevel::Error, ">:( \n going to crash..."); // going to crash...
		return comp;
	}

	inline void Entity::addComponent(std::type_index type)
	{
		manager->addComponentOnEntity(*this, type);
	}

	template <typename F>
	requires std::invocable<F, RuntimeComponent>
	inline void Entity::forEachComponent(F&& f)
	{
		manager->forEachComponentOfEntity(*this, std::forward<F>(f));
	}

	template<ConceptComponent T>
	inline T* Entity::tryGetComponent()
	{
		return manager->getComponentOfEntity<T>(*this);
	}

	template<ConceptComponent T>
	inline const T* Entity::tryGetComponent() const
	{
		return &this->getComponent<T>();
	}

	template <ConceptComponent T>
	inline void Entity::removeComponent()
	{
		this->removeComponent(typeid(T));
	}

	inline void Entity::removeComponent(std::type_index type)
	{
		manager->removeComponentOfEntity(*this, type);
	}

	inline auto Entity::runtimeComponentView() -> RuntimeComponentView 
	{
		return manager->makeComponentView(*this);
	}
	inline auto Entity::runtimeComponentViewLive() -> RuntimeComponentViewLive
	{
		return manager->makeComponentViewLive(*this);
	}

	inline auto Entity::getComponentMask() const -> const ComponentMask& { return manager->getComponentMaskOfEntity(*this); }
}
