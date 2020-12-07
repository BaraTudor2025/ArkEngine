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

	class Scene;

	class EntityManager final : public NonCopyable, public NonMovable {

	public:
		EntityManager(ComponentManager& cm) : componentManager(cm) {}

	public:

		Entity createEntity()
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

		Entity cloneEntity(Entity e)
		{
			Entity hClone = createEntity();
			auto& entity = getEntity(e);
			auto& clone = getEntity(hClone);

			clone.mask = entity.mask;
			for (auto compData : entity.components) {
				auto [newComponent, newIndex] = componentManager.copyComponent(compData.type, compData.index);
				auto& cloneCompData = clone.components.emplace_back();
				cloneCompData.pComponent = newComponent;
				cloneCompData.index = newIndex;
				cloneCompData.type = compData.type;
				onConstruction(cloneCompData.type, cloneCompData.pComponent, hClone);
			}
			return hClone;
		}

		void destroyEntity(Entity e)
		{
			freeEntities.push_back(e.id);
			auto& entity = getEntity(e);

			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity with id(%d)", entity.id);

			for (auto compData : entity.components)
				componentManager.removeComponent(compData.type, compData.index);

			entity.mask.reset();
			entity.components.clear();
			entity.isFree = true;
			dirtyEntities.erase(e);
		}


		template <typename T>
		using hasSetEntity = decltype(std::declval<T>()._setEntity(std::declval<Entity>()));

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
		template <typename T, typename... Args>
		T& addComponentOnEntity(Entity e, Args&&... args) {
			static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
			componentManager.addComponentType(typeid(T));
			bool bDefaultConstruct = sizeof...(args) == 0 ? true : false;
			void* comp = implAddComponentOnEntity(e, typeid(T), bDefaultConstruct);
			if (not bDefaultConstruct)
				new(comp)T(std::forward<Args>(args)...);
			onConstruction(typeid(T), comp, e);
			return *static_cast<T*>(comp);
		}

		void* addComponentOnEntity(Entity e, std::type_index type) {
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
			markAsModified(e);
			entity.mask.set(compId);

			auto [comp, compIndex] = componentManager.addComponent(type, defaultConstruct);
			auto& compData = entity.components.emplace_back();
			compData.pComponent = comp;
			compData.index = compIndex;
			compData.type = type;
			return comp;
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
				componentManager.removeComponent(type, entity.getComponentData(type).index);
				Util::erase_if(entity.components, [&](const auto& compData) { return compData.type == type; });
			}
		}

		const ComponentManager::ComponentMask& getComponentMaskOfEntity(Entity e)
		{
			auto& entity = getEntity(e);
			return entity.mask;
		}

		void markAsModified(Entity e)
		{
			dirtyEntities.insert(e);
		}

		std::optional<std::set<Entity>> getModifiedEntities()
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

		auto makeComponentView(Entity e);

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
				// components of entity need to be destroyed
				if (it == freeEntities.end()) {
					for (const auto& compData : entity.components)
						componentManager.removeComponent(compData.type, compData.index);
				}
				id += 1;
			}
		}

	private:

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
				int16_t index;
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

		InternalEntityData& getEntity(Entity e)
		{
			return entities.at(e.id);
		}

		struct RuntimeComponentIterator {
			using InternalIterator = std::vector<InternalEntityData::ComponentData>::iterator;
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

	private:
		std::vector<InternalEntityData> entities;
		std::set<Entity> dirtyEntities;
		std::vector<int> freeEntities;
		std::unordered_map<std::type_index, std::function<void(Entity, void*)>> onConstructionTable;
		ComponentManager& componentManager;
		friend class RuntimeComponentView;
		friend class Scene;
	};

	class RuntimeComponentView {
		friend class EntityManager;
		using Iter = EntityManager::RuntimeComponentIterator;
		Iter iterBegin;
		Iter iterEnd;
	public:
		RuntimeComponentView(Iter b, Iter e) : iterBegin(b), iterEnd(e) { }
		auto begin() { return iterBegin; }
		auto end() { return iterEnd; }
	};

	inline auto EntityManager::makeComponentView(Entity e)
	{
		auto& entity = getEntity(e);
		return RuntimeComponentView{ entity.components.begin(), entity.components.end() };
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
