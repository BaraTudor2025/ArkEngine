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
#include "ark/util/ResourceManager.hpp"

namespace ark {

	class Scene;

	class EntityManager final : public NonCopyable, public NonMovable {

	public:
		EntityManager(ComponentManager& cm) : componentManager(cm) {}

	public:

		Entity createEntity(std::string name)
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

			if (name.empty())
				entity.name = std::string("entity_") + std::to_string(e.id);
			else
				entity.name = name;

			EngineLog(LogSource::EntityM, LogLevel::Info, "created (%s)", entity.name.c_str());
			return e;
		}

		Entity cloneEntity(Entity e, std::string name)
		{
			Entity hClone = createEntity(name);
			auto& entity = getEntity(e);
			auto& clone = getEntity(hClone);
			if (name.empty()) {
				clone.name.append("_cloneof_");
				clone.name.append(entity.name);
			} 

			for (auto compData : entity.components) {
				auto [newComponent, newIndex] = componentManager.copyComponent(compData.type, compData.index);
				clone.mask.set(componentManager.idFromType(compData.type));
				auto& cloneCompData = clone.components.emplace_back();
				cloneCompData.pComponent = newComponent;
				cloneCompData.index = newIndex;
				cloneCompData.type = compData.type;
			}
			return hClone;
		}

		const std::string& getNameOfEntity(Entity e)
		{
			auto& entity = getEntity(e);
			return entity.name;
		}

		void setNameOfEntity(Entity e, std::string name)
		{
			auto& entity = getEntity(e);
			entity.name = name;
		}

		Entity getEntityByName(std::string name)
		{
			for (int i = 0; i < entities.size(); i++) {
				if (entities[i].name == name) {
					Entity e;
					e.id = i;
					e.manager = this;
					return e;
				}
			}
			EngineLog(LogSource::EntityM, LogLevel::Warning, "getByName(): didn't find entity (%s)", name.c_str());
			return {};
		}

		void destroyEntity(Entity e)
		{
			freeEntities.push_back(e.id);
			auto& entity = getEntity(e);

			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity (%s)", entity.name.c_str());

			for (auto compData : entity.components)
				componentManager.removeComponent(compData.type, compData.index);

			entity.name.clear();
			entity.mask.reset();
			entity.components.clear();
			entity.isFree = true;
		}

		// if component already exists, then the existing component is returned
		template <typename T, typename... Args>
		T& addComponentOnEntity(Entity e, Args&&... args) {
			bool def = sizeof...(args) == 0 ? true : false;
			void* comp = implAddComponentOnEntity(e, typeid(T), def);
			if(not def)
				Util::construct_in_place<T>(comp, std::forward<Args>(args)...);
			return *static_cast<T*>(comp);
		}

		void* addComponentOnEntity(Entity e, std::type_index type) {
			return implAddComponentOnEntity(e, type, true);
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
				EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%s), doesn't have component (%s)", entity.name.c_str(), Util::getNameOfType(type));
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
				markAsModified(e);
				auto& compData = entity.getComponentData(type);
				componentManager.removeComponent(type, compData.index);
				entity.mask.set(compId, false);
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
				auto it = std::find(freeEntities.begin(), freeEntities.end(), id);
				// component is not destroyed
				if (it == freeEntities.end()) {
					for (auto compData : entity.components)
						componentManager.removeComponent(compData.type, compData.index);
				}
				id += 1;
			}
		}

	private:

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
			std::string name = "";

			ComponentData& getComponentData(std::type_index type)
			{
				return *std::find_if(std::begin(components), std::end(components), [&](const auto& compData) { return compData.type == type; });
			}
		};

		InternalEntityData& getEntity(Entity e)
		{
			return entities.at(e.id);
		}

	private:
		std::vector<InternalEntityData> entities;
		std::set<Entity> dirtyEntities;
		std::vector<int> freeEntities;
		ComponentManager& componentManager;
		friend class Director;
	};

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


	inline void Entity::addComponent(std::type_index type)
	{
		manager->addComponentOnEntity(*this, type);
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

}
