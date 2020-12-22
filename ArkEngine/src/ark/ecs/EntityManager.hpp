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


	class EntityManager final : public NonCopyable, public NonMovable {

	public:
		EntityManager(ComponentManager& cm, std::pmr::memory_resource* upstreamComponent)
			: componentManager(cm), 
			componentPool(
				std::pmr::pool_options{.max_blocks_per_chunk = 30, .largest_required_pool_block = 1024},
				upstreamComponent) { }


		auto createEntity() -> Entity
		{
			Entity e;
			e.manager = this;
			if (!freeEntities.empty()) {
				e.id = freeEntities.back();
				freeEntities.pop_back();
			} else {
				e.id = entities.size();
				entities.emplace_back().mask.reset();
			}
			auto& entity = getEntity(e);
			entity.id = e.id;
			entity.isFree = false;

			EngineLog(LogSource::EntityM, LogLevel::Info, "created entity with id(%d)", entity.id);
			return e;
		}

		auto cloneEntity(Entity e) -> Entity
		{
			Entity hClone = createEntity();
			auto& entity = getEntity(e);
			auto& clone = getEntity(hClone);

			clone.mask = entity.mask;
			for (const auto& compData : entity.components) {
				auto metadata = meta::getMetadata(compData.type);
				void* newComponent = componentPool.allocate(metadata->size, metadata->align);
				if (metadata->copy_constructor) {
					metadata->copy_constructor(newComponent, compData.pComponent);
				}
				else
					metadata->default_constructor(newComponent);

				clone.components.push_back({compData.type, newComponent});
				onConstruction(compData.type, newComponent, hClone);
			}
			return hClone;
		}

		void destroyEntity(Entity e)
		{
			freeEntities.push_back(e.id);
			auto& entity = getEntity(e);

			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity with id(%d)", entity.id);

			for (const auto& compData : entity.components)
				destroyComponent(compData.type, compData.pComponent);

			entity.mask.reset();
			entity.components.clear();
			entity.isFree = true;
			dirtyEntities.erase(e);
		}

		//template <typename T>
		//using hasSetEntity = decltype(std::declval<T>()._setEntity(std::declval<Entity>()));

		template <typename T, typename F>
		void addOnConstruction(F&& f)
		{
			onConstructionTable[typeid(T)] = [f = std::forward<F>(f)](Entity e, void* ptr) {
				using func_traits = meta::detail::function_traits<F>;
				static_assert(std::is_same_v<func_traits::template arg<0>, ark::Entity>, "primu arg tre sa fie entity");
				static_assert(std::is_same_v<std::decay_t<std::remove_pointer_t<func_traits::template arg<1>>>, T>, "al doilea arg tre sa fie componenta");
				if constexpr (std::is_pointer_v<func_traits::template arg<1>>)
					f(e, static_cast<T*>(ptr));
				else if constexpr (std::is_reference_v<func_traits::template arg<1>>)
					f(e, *static_cast<T*>(ptr));
				else
					static_assert(false, "argumetul componenta tre sa fie ori pointer ori referinta");
			};
		}

		// if component already exists, then the existing component is returned
		// if no ctor-args are provided then it's default constructed
		template <typename T, typename... Args>
		T& addComponentOnEntity(Entity e, Args&&... args)
		{
			static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
			componentManager.addComponentType(typeid(T));
			bool bDefaultConstruct = sizeof...(args) == 0 ? true : false;
			void* comp = implAddComponentOnEntity(e, typeid(T), bDefaultConstruct);
			if (not bDefaultConstruct)
				new(comp)T(std::forward<Args>(args)...);
			onConstruction(typeid(T), comp, e);
			return *static_cast<T*>(comp);
		}

		void* addComponentOnEntity(Entity e, std::type_index type)
		{
			componentManager.addComponentType(type);
			void* comp = implAddComponentOnEntity(e, type, true);
			onConstruction(type, comp, e);
			return comp;
		}

		void* implAddComponentOnEntity(Entity e, std::type_index type, bool defaultConstruct)
		{
			int compId = componentManager.idFromType(type);
			auto& entity = getEntity(e);
			if (entity.mask.test(compId))
				return getComponentOfEntity(e, type);
			entity.mask.set(compId);
			markAsModified(e);

			auto metadata = meta::getMetadata(type);
			void* newComponent = componentPool.allocate(metadata->size, metadata->align);
			if(defaultConstruct)
				metadata->default_constructor(newComponent);

			entity.components.push_back({ type, newComponent });
			return newComponent;
		}

		void* getComponentOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			int compId = componentManager.idFromType(type);
			if (!entity.mask.test(compId)) {
				EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%d), doesn't have component (%s)", entity.id, meta::getMetadata(type)->name);
				return nullptr;
			}
			return entity.getComponentData(type).pComponent;
		}

		template <typename T>
		T* getComponentOfEntity(Entity e)
		{
			return reinterpret_cast<T*>(getComponentOfEntity(e, typeid(T)));
		}

		void removeComponentOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			auto compId = componentManager.idFromType(type);
			if (entity.mask.test(compId)) {
				entity.mask.set(compId, false);
				markAsModified(e);
				destroyComponent(type, entity.getComponentData(type).pComponent);
				Util::erase_if(entity.components, [&](const auto& compData) { return compData.type == type; });
			}
		}

		auto getComponentMaskOfEntity(Entity e) -> const ComponentManager::ComponentMask&
		{
			auto& entity = getEntity(e);
			return entity.mask;
		}

		void markAsModified(Entity e)
		{
			dirtyEntities.insert(e);
		}

		auto getModifiedEntities() -> std::optional<std::set<Entity>>
		{
			if (dirtyEntities.empty())
				return std::nullopt;
			else
				return std::move(dirtyEntities);
		}

		template <typename F>
		void forEachComponentOfEntity(Entity e, F&& f)
		{
			auto& entity = getEntity(e);
			RuntimeComponent comp;
			for (auto& compData : entity.components) {
				comp.type = compData.type;
				comp.ptr = compData.pComponent;
				f(comp);
			}
		}

		auto makeComponentView(Entity e) -> RuntimeComponentView;

		auto entitiesView() -> EntitiesView;

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
			for (const auto& entity : entities) {
				auto it = std::find(freeEntities.begin(), freeEntities.end(), id);
				// destroy components only for live entities
				if (it == freeEntities.end()) {
					for (const auto& compData : entity.components) {
						auto metadata = meta::getMetadata(compData.type);
						if(metadata->destructor)
							metadata->destructor(compData.pComponent);
					}
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
			componentPool.deallocate(component, metadata->size, metadata->align);
		}

		void onConstruction(std::type_index type, void* ptr, Entity e)
		{
			if (auto it = onConstructionTable.find(type); it != onConstructionTable.end()) {
				auto& onCtor = it->second;
				if (onCtor)
					onCtor(e, ptr);
			}
		}

		struct InternalEntityData {
			struct ComponentData {
				std::type_index type = typeid(void);
				void* pComponent; // reference
			};
			std::vector<ComponentData> components;
			ComponentManager::ComponentMask mask;
			int16_t id = ArkInvalidID;
			bool isFree = false;

			ComponentData& getComponentData(std::type_index type)
			{
				return *std::find_if(std::begin(components), std::end(components), [&](const auto& compData) { return compData.type == type; });
			}
		};

		auto getEntity(Entity e) -> InternalEntityData&
		{
			return entities.at(e.id);
		}

		using ComponentVector = decltype(InternalEntityData::components);

	private:
		std::pmr::unsynchronized_pool_resource componentPool;		
		std::vector<InternalEntityData> entities;
		std::set<Entity> dirtyEntities;
		std::vector<int> freeEntities;
		std::unordered_map<std::type_index, std::function<void(Entity, void*)>> onConstructionTable;
		ComponentManager& componentManager;
		friend struct RuntimeComponentIterator;
		friend struct EntityIterator;
		friend class RuntimeComponentView;
		friend class EntitiesView;
		friend class Registry;
		friend class Entity;
	};

	struct RuntimeComponentIterator {
		using InternalIterator = decltype(EntityManager::InternalEntityData::components)::iterator;
		InternalIterator iter;
	public:
		RuntimeComponentIterator(InternalIterator iter) :iter(iter) {};

		RuntimeComponentIterator& operator++()
		{
			++iter;
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

	// live view
	class RuntimeComponentView {
		EntityManager::ComponentVector& mComps;
	public:
		RuntimeComponentView(EntityManager::ComponentVector& comps) : mComps(comps) {}
		auto begin() -> RuntimeComponentIterator{ return mComps.begin(); }
		auto end() -> RuntimeComponentIterator { return mComps.end(); }
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
			while(mIter < mEnd && mIter->isFree)
				++mIter;
			return *this;
		}

		const EntityIterator& operator++() const noexcept
		{
			++mIter;
			while(mIter < mEnd && mIter->isFree)
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

	class EntitiesView {
		EntityManager& mManager;
	public:
		EntitiesView(EntityManager& m) : mManager(m) {}

		auto begin() -> EntityIterator { return {mManager.entities.begin(), mManager.entities.end(), mManager}; }
		auto end() -> EntityIterator { return {mManager.entities.end(), mManager.entities.end(), mManager}; }
		auto begin() const -> const EntityIterator { return {mManager.entities.begin(), mManager.entities.end(), mManager}; }
		auto end() const -> const EntityIterator { return {mManager.entities.end(), mManager.entities.end(), mManager}; }
	};

	inline auto EntityManager::makeComponentView(Entity e) -> RuntimeComponentView
	{
		return RuntimeComponentView{getEntity(e).components};
	}

	inline auto EntityManager::entitiesView() -> EntitiesView
	{
		return EntitiesView{*this};
	}


	/* Entity handle definitions */

	inline bool Entity::isValid() const {
		if (manager == nullptr)
			return false;
		for (int id : manager->freeEntities)
			if (id == this->id)
				return false;
		return /*probably*/ true; 
	}

	template<typename T, typename ...Args>
	inline T& Entity::addComponent(Args&& ...args)
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
		return manager->addComponentOnEntity<T>(*this, std::forward<Args>(args)...);
	}

	template<typename T>
	inline T& Entity::getComponent()
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
		auto comp = manager->getComponentOfEntity<T>(*this);
		if (!comp)
			EngineLog(LogSource::EntityM, LogLevel::Error, ">:( \n going to crash..."); // going to crash...
		return *comp;
	}

	template<typename T>
	inline const T& Entity::getComponent() const
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
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
	inline void Entity::forEachComponent(F&& f)
	{
		manager->forEachComponentOfEntity(*this, std::forward<F>(f));
	}

	template<typename T>
	inline T* Entity::tryGetComponent()
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
		return manager->getComponentOfEntity<T>(*this);
	}

	template<typename T>
	inline const T* Entity::tryGetComponent() const
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
		return &this->getComponent<T>();
	}

	template <typename T>
	inline void Entity::removeComponent()
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
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
}
