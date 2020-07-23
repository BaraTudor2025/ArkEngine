#pragma once

#include <unordered_map>
#include <fstream>
#include <typeindex>
#include <optional>
#include <bitset>
#include <tuple>
#include <set>

#include "ark/ecs/Component.hpp"
#include "ark/ecs/Script.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/util/ResourceManager.hpp"


namespace ark {

	class Scene;

	class EntityManager final : public NonCopyable, public NonMovable {

	public:
		EntityManager(ComponentManager& cm, ScriptManager& sm) : componentManager(cm), scriptManager(sm) {}

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
				auto [component, index] = componentManager.copyComponent(compData.id, compData.index);
				clone.mask.set(compData.id);
				auto& cloneCompData = clone.components.emplace_back();
				cloneCompData.component = component;
				cloneCompData.id = compData.id;
				cloneCompData.index = index;
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

		void serializeEntity(Entity e)
		{
			auto& entity = getEntity(e);
			json jsonEntity;

			auto& jsonComps = jsonEntity["components"];
			for (auto compData : entity.components) {
				auto compName = componentManager.nameFromId(compData.id);
				jsonComps[compName.data()] = componentManager.serializeComponent(compData.id, compData.component);
			}

			jsonEntity["scripts"] = scriptManager.serializeScripts(entity.scriptsIndex);

			std::ofstream of(getEntityFilePath(e));
			of << jsonEntity.dump(4, ' ', true);
		}

		void deserializeEntity(Entity e)
		{
			auto& entity = getEntity(e);
			json jsonEntity;
			std::ifstream fin(getEntityFilePath(e));
			fin >> jsonEntity;

			auto& jsonComps = jsonEntity.at("components");
			for (const auto& [key, _] : jsonComps.items()) {
				auto type = componentManager.typeFromName(key);
				e.addComponent(type);
			}
			for (auto compData : entity.components) {
				auto compName = componentManager.nameFromId(compData.id);
				componentManager.deserializeComponent(compData.id, compData.component, jsonComps.at(compName.data()));
			}

			if (!jsonEntity.contains("scripts"))
				return;
			const auto& jsonScripts = jsonEntity.at("scripts");
			for (const auto& [key, _] : jsonScripts.items()) {
				auto type = scriptManager.getTypeFromName(key);
				if (e.getScript(type) == nullptr)
					e.addScript(type);
			}
			scriptManager.deserializeScripts(entity.scriptsIndex, jsonScripts);
		}

		void destroyEntity(Entity e)
		{
			freeEntities.push_back(e.id);
			auto& entity = getEntity(e);

			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity (%s)", entity.name.c_str());

			for (auto compData : entity.components)
				componentManager.removeComponent(compData.id, compData.index);

			scriptManager.removeScripts(entity.scriptsIndex);

			entity.name.clear();
			entity.mask.reset();
			entity.scriptsIndex = ArkInvalidIndex;
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

			auto [comp, compIndex] = componentManager.addComponent(compId, defaultConstruct);
			auto& compData = entity.components.emplace_back();
			compData.component = comp;
			compData.id = compId;
			compData.index = compIndex;
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
			return entity.getComponent(compId);
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
				componentManager.removeComponent(compId, entity.getComponentIndex(compId));
				entity.mask.set(compId, false);
				Util::erase_if(entity.components, [compId](const auto& compData) { return compData.id == compId; });
			}
		}

		template <typename T, typename... Args>
		T* addScriptOnEntity(Entity e, Args&&... args)
		{
			auto& entity = getEntity(e);
			auto script = scriptManager.addScript<T>(entity.scriptsIndex, std::forward<Args>(args)...);
			script->m_entity = e;
			script->init();
			script->isInitialized = true;
			return script;
		}

		Script* addScriptOnEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			auto script = scriptManager.addScript(entity.scriptsIndex, type);
			script->m_entity = e;
			script->init();
			script->isInitialized = true;
			return script;
		}

		template <typename T>
		T* getScriptOfEntity(Entity e)
		{
			auto& entity = getEntity(e);
			auto script = scriptManager.getScript<T>(entity.scriptsIndex);
			if (script == nullptr)
				EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%s) doesn't have (%s) script attached", entity.name.c_str(), Util::getNameOfType<T>());
			return script;
		}

		Script* getScriptOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			auto script = scriptManager.getScript(entity.scriptsIndex, type);
			if (script == nullptr)
				EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%s) doesn't have (%s) script attached", entity.name.c_str(), Util::getNameOfType(type));
			return script;
		}

		bool hasScript(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			return scriptManager.hasScript(entity.scriptsIndex, type);
		}

		void removeScriptOfEntity(Entity e, std::type_index type)
		{
			auto& entity = getEntity(e);
			scriptManager.removeScript(entity.scriptsIndex, type);
		}

		void setScriptActive(Entity e, bool active, std::type_index type)
		{
			auto& entity = getEntity(e);
			scriptManager.setActive(entity.scriptsIndex, active, type);
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
						componentManager.removeComponent(compData.id, compData.index);
				}
				id += 1;
			}
		}

	private:

		static inline const std::string entityFolder = Resources::resourceFolder + "entities/";

		std::string getEntityFilePath(Entity e)
		{
			auto& entity = getEntity(e);
			return entityFolder + entity.name + ".json";
		}

		struct InternalEntityData {
			struct ComponentData {
				int16_t id;
				int16_t index;
				void* component; // reference
			};
			ComponentManager::ComponentMask mask;
			std::vector<ComponentData> components;
			int16_t scriptsIndex = ArkInvalidIndex;
			int16_t id = ArkInvalidID;
			bool isFree = false;
			std::string name = "";

			void* getComponent(int id)
			{
				auto compData = std::find_if(std::begin(components), std::end(components), [=](const auto& compData) { return compData.id == id; });
				return compData->component;
			}

			template <typename T>
			T* getComponent(int id)
			{
				return reinterpret_cast<T*>(getComponent(id));
			}

			int16_t getComponentIndex(int id)
			{
				auto compData = std::find_if(std::begin(components), std::end(components), [=](const auto& compData) { return compData.id == id; });
				return compData->index;
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
		ScriptManager& scriptManager;
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


	template<typename T, typename ...Args>
	inline T* Entity::addScript(Args&& ...args)
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		return manager->addScriptOnEntity<T>(*this, std::forward<Args>(args)...);
	}

	inline Script* Entity::addScript(std::type_index type)
	{
		return manager->addScriptOnEntity(*this, type);
	}

	template<typename T>
	inline T* Entity::getScript()
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		return manager->getScriptOfEntity<T>(*this);
	}

	inline Script* Entity::getScript(std::type_index type)
	{
		return manager->getScriptOfEntity(*this, type);
	}

	template <typename T>
	inline bool Entity::hasScript()
	{
		return this->hasScript(typeid(T));
	}

	inline bool Entity::hasScript(std::type_index type)
	{
		return manager->hasScript(*this, type);
	}

	template<typename T>
	inline void Entity::setScriptActive(bool active)
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		this->setScriptActive(typeid(T), active);
	}

	inline void Entity::setScriptActive(std::type_index type, bool active)
	{
		manager->setScriptActive(*this, active, type);
	}

	template<typename T>
	inline void Entity::removeScript()
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		this->removeScript(typeid(T));
	}

	inline void Entity::removeScript(std::type_index type)
	{
		manager->removeScriptOfEntity(*this, type);
	}

}
