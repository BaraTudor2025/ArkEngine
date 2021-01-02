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
		EntityManager(ComponentManager& cm, 
			std::vector<std::pair<Entity::ID, ComponentManager::ComponentMask>>& mComponentsAdded, 
			std::pmr::memory_resource* upstreamComponent)
			: componentManager(cm),
			mComponentsAdded(mComponentsAdded),
			componentPool(
				std::pmr::pool_options{.max_blocks_per_chunk = 30, .largest_required_pool_block = 1024},
				upstreamComponent) { }

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

		auto cloneEntity(Entity e) -> Entity
		{
			Entity hClone = createEntity();
			auto& entity = getEntity(e);
			auto& clone = getEntity(hClone);
			clone.mask = entity.mask;
			entity.forComponents([&, this](auto& compData) {
				auto metadata = meta::getMetadata(compData.type);
				void* newComponent = componentPool.allocate(metadata->size, metadata->align);
				if (metadata->copy_constructor) {
					metadata->copy_constructor(newComponent, compData.pComponent);
				}
				else
					metadata->default_constructor(newComponent);

				clone.components[componentManager.idFromType(compData.type)] = { compData.type, newComponent };
				onConstruction(compData.type, newComponent, hClone);
			});
			mComponentsAdded.push_back({clone.id, clone.mask});
			return hClone;
		}

		void destroyEntity(Entity e)
		{
			auto& entity = getEntity(e);
			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity with id(%d)", entity.id);
			entity.forComponents([this](auto& compData) {
				if (compData.pComponent) {
					destroyComponent(compData.type, compData.pComponent);
					compData = {};
				}
			});
			entity.mask.reset();
			isFree[entity.id] = true;
		}

		template <typename T, typename F>
		void addOnConstruction(F&& f) {
			onConstructionTable[typeid(T)] = [f = std::forward<F>(f)](Entity e, void* ptr) {
				if constexpr (std::invocable<F, ark::Entity, T&>)
					f(e, *static_cast<T*>(ptr));
				else if constexpr (std::invocable<F, ark::Entity, T*>)
					f(e, static_cast<T*>(ptr));
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
			componentManager.addComponentType(type);
			auto [comp, isAlready] = implAddComponentOnEntity(e, type, true);
			if(!isAlready)
				onConstruction(type, comp, e);
			return comp;
		}

		// returns: component pointer, exists?
		auto implAddComponentOnEntity(Entity e, std::type_index type, bool defaultConstruct) -> std::pair<void*, bool>
		{
			int compId = componentManager.idFromType(type);
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
			void* newComponent = componentPool.allocate(metadata->size, metadata->align);
			if(defaultConstruct)
				metadata->default_constructor(newComponent);

			entity.components[compId] = { type, newComponent };
			return { newComponent, false };
		}

		void* getComponentOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			int compId = componentManager.idFromType(type);
			if (!entity.mask.test(compId)) {
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
			auto compId = componentManager.idFromType(type);
			if (entity.mask.test(compId)) {
				entity.mask.set(compId, false);
				destroyComponent(type, entity.components[compId].pComponent);
				entity.components[compId] = {};
			}
		}

		auto getComponentMaskOfEntity(Entity e) -> const ComponentManager::ComponentMask&
		{
			auto& entity = getEntity(e);
			return entity.mask;
		}

		template <typename F>
		void forEachComponentOfEntity(Entity e, F&& f)
		{
			auto& entity = getEntity(e);
			entity.forComponents([&](auto& compData) {
				RuntimeComponent comp;
				comp.type = compData.type;
				comp.ptr = compData.pComponent;
				f(comp);
			});
		}

		auto makeComponentView(Entity e) -> RuntimeComponentView;
		auto makeComponentViewLive(Entity e)->RuntimeComponentViewLive;
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
			for (auto& entity : entities) {
				//auto it = std::find(freeEntities.begin(), freeEntities.end(), id);
				// destroy components only for alive entities
				if (isFree[entity.id]) {
					entity.forComponents([this](auto& compData) {
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

	public:
		struct InternalEntityData {
			struct ComponentData {
				std::type_index type = typeid(void);
				void* pComponent = nullptr;
			};
			ComponentManager::ComponentMask mask;
			int16_t id = ArkInvalidID;
			std::array<ComponentData, ComponentManager::MaxComponentTypes> components;

			using BitsType = std::conditional_t<ComponentManager::MaxComponentTypes <= 32, std::uint32_t, std::uint64_t>;
			auto getBits() -> BitsType {
				if constexpr (ComponentManager::MaxComponentTypes <= 32)
					return mask.to_ulong();
				else
					return mask.to_ullong();
			}

			template <typename F>
			void forComponents(F&& f) {
				int i = 0;
				for (auto bits = getBits(); bits; bits >>= 1, ++i)
					if (bits & 1)
						f(components[i]);
			}
		};
	private:

		auto getEntity(Entity e) -> InternalEntityData&
		{
			return entities.at(e.id);
		}

		using ComponentVector = decltype(InternalEntityData::components);

	private:
		std::pmr::unsynchronized_pool_resource componentPool;
		std::vector<InternalEntityData> entities;
		std::vector<std::pair<Entity::ID, ComponentManager::ComponentMask>>& mComponentsAdded;
		std::vector<bool> isFree;
		std::unordered_map<std::type_index, std::function<void(Entity, void*)>> onConstructionTable;
		ComponentManager& componentManager;
		template <bool> friend struct RuntimeComponentIterator;
		friend struct EntityIterator;
		friend class RuntimeComponentView;
		friend class EntitiesView;
		friend class Registry;
		friend class Entity;
	};

	template <bool isLive = false>
	struct RuntimeComponentIterator {
		using InternalIterator = decltype(EntityManager::InternalEntityData::components)::iterator;
		InternalIterator iter;
		InternalIterator mEnd;
		EntityManager::InternalEntityData::BitsType mask;
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
		auto begin() -> RuntimeComponentIterator<false> { return { mData.components.begin(), mData.components.end(), mData.getBits() }; }
		auto end() -> RuntimeComponentIterator<false> { return { mData.components.end(), mData.components.end(), mData.getBits() }; }
	};

	class RuntimeComponentViewLive {
		EntityManager::InternalEntityData& mData;
	public:
		RuntimeComponentViewLive(decltype(mData) mData) : mData(mData) {}
		auto begin() -> RuntimeComponentIterator<true> { return { mData.components.begin(), mData.components.end(), mData.getBits() }; }
		auto end() -> RuntimeComponentIterator<true> { return { mData.components.end(), mData.components.end(), mData.getBits() }; }
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


	/* Entity handle definitions */

	inline bool Entity::isValid() const {
		return manager != nullptr && manager->isFree[this->id];
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
	requires std::invocable<F, RuntimeComponent>
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
	inline auto Entity::runtimeComponentViewLive() -> RuntimeComponentViewLive
	{
		return manager->makeComponentViewLive(*this);
	}

	inline auto Entity::getComponentMask() const -> const ComponentManager::ComponentMask& { return manager->getComponentMaskOfEntity(*this); }
}
