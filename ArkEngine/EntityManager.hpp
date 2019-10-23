#pragma once

#include "Component.hpp"
#include "Script.hpp"
#include "Entity.hpp"

#include <libs/static_any.hpp>

#include <unordered_map>
#include <typeindex>
#include <optional>
#include <bitset>
#include <tuple>
#include <set>

class EntityManager final : public NonCopyable, public NonMovable {

public:
	EntityManager(ComponentManager& cm, ScriptManager& sm): componentManager(cm), scriptManager(sm) {}
	~EntityManager() = default;

public:

	Entity createEntity(std::string name)
	{
		Entity e;
		e.manager = this;
		if (!freeEntities.empty()) {
			e.id = freeEntities.back();
			freeEntities.pop_back();
			auto& entity = getEntity(e);
		} else {
			e.id = entities.size();
			auto& entity = entities.emplace_back();
			entity.mask.reset();
			entity.componentIndexes.fill(ArkInvalidIndex);
		}
		auto& entity = getEntity(e);

		if (name.empty())
			entity.name = std::string("entity_") + std::to_string(e.id);
		else
			entity.name = name;

		EngineLog(LogSource::EntityM, LogLevel::Info, "created (%s)", entity.name.c_str());
		return e;
	}

	// TODO (entity manager): figure out a way to copy components without templates
	// only components are copied, scripts are excluded, maybe it's possible in ArchetypeManager?
	Entity cloneEntity(Entity e)
	{
		//Entity hClone = createEntity();
		//auto entity = getEntity(e);
		//auto clone = getEntity(hClone);
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

		for (int i = 0; i < entity.componentIndexes.size(); i++)
			if (i != ArkInvalidIndex)
				componentManager.removeComponent(i, entity.componentIndexes[i]);

		scriptManager.removeScripts(entity.scriptsIndex);

		//entity.childrenIndex = ArkInvalidIndex;
		entity.name.clear();
		entity.mask.reset();
		entity.scriptsIndex = ArkInvalidIndex;
		entity.componentIndexes.fill(ArkInvalidIndex);
	}

	// if component already exists, then the existing component is returned
	template <typename T, typename... Args>
	T& addComponentOnEntity(Entity e, Args&&... args)
	{
		int compId = componentManager.getComponentId<T>();
		auto& entity = getEntity(e);
		if (entity.mask.test(compId))
			return *getComponentOfEntity<T>(e);

		auto [comp, compIndex] = componentManager.addComponent<T>(std::forward<Args>(args)...);

		entity.mask.set(compId);
		entity.componentIndexes.at(compId) = compIndex;

		return *comp;
	}

	template <typename T>
	T* getComponentOfEntity(Entity e)
	{
		auto& entity = getEntity(e);
		int compId = componentManager.getComponentId<T>();
		if (!entity.mask.test(compId)) {
			EngineLog(LogSource::EntityM, LogLevel::Warning, "entity (%s), doesn't have component (%s)", entity.name.c_str(), Util::getNameOfType<T>());
			return nullptr;
		}
		int compIndex = entity.componentIndexes[compId];
		return componentManager.getComponent<T>(compId, compIndex);
	}

	template <typename T, typename... Args>
	T* addScriptOnEntity(Entity e, Args&&... args)
	{
		auto& entity = getEntity(e);
		auto script = scriptManager.addScript<T>(entity.scriptsIndex, std::forward<Args>(args)...);
		script->m_entity = e;
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

	template <typename T>
	void removeScriptOfEntity(Entity e)
	{
		auto& entity = getEntity(e);
		scriptManager.removeScript<T>(entity.scriptsIndex);
	}

	template <typename T>
	void setScriptActive(Entity e, bool active)
	{
		auto& entity = getEntity(e);
		scriptManager.setActive<T>(entity.scriptsIndex, active);
	}

	const ComponentManager::ComponentMask& getComponentMaskOfEntity(Entity e) {
		auto& entity = getEntity(e);
		return entity.mask;
	}

	void markAsDirty(Entity e)
	{
		dirtyEntities.insert(e);
	}

	std::optional<std::set<Entity>> getDirtyEntities()
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


private:

	struct InternalEntityData {
		//int16_t childrenIndex = ArkInvalidIndex;
		//int16_t parentIndex = ArkInvalidIndex;
		ComponentManager::ComponentMask mask;
		std::array<int16_t, ComponentManager::MaxComponentTypes> componentIndexes;
		int16_t scriptsIndex = ArkInvalidIndex;
		std::string name = "";
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
	//std::vector<std::vector<Entity>> childrenTree;
};

template<typename T, typename ...Args>
inline T& Entity::addComponent(Args&& ...args)
{
	static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
	manager->markAsDirty(*this);
	return manager->addComponentOnEntity<T>(*this, std::forward<Args>(args)...);
}

template<typename T>
inline T& Entity::getComponent()
{
	static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
	auto comp = manager->getComponentOfEntity<T>(*this);
	if (!comp)
		EngineLog(LogSource::EntityM, LogLevel::Error, ">:( \n going to crash..."); // going to crash...
	return *comp;
}

template<typename T>
inline T* Entity::tryGetComponent()
{
	static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
	return manager->getComponentOfEntity<T>(*this);
}

template<typename T, typename ...Args>
inline T* Entity::addScript(Args&& ...args)
{
	static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
	return manager->addScriptOnEntity<T>(*this, std::forward<Args>(args)...);
}

template<typename T>
inline T* Entity::getScript()
{
	static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
	return manager->getScriptOfEntity<T>(*this);
}

template<typename T>
inline void Entity::setScriptActive(bool active)
{
	static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
	return manager->setScriptActive<T>(*this, active);
}

template<typename T>
inline void Entity::removeScript()
{
	static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
	return manager->removeScriptOfEntity<T>(*this);
}




