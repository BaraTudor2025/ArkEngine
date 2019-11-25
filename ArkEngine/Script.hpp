#pragma once

#include "Core.hpp"
#include "Util.hpp"
#include "Entity.hpp"

#include <SFML/Window/Event.hpp>
#include <functional>

class Entity;

class ARK_ENGINE_API Script : public NonCopyable {

public:
	Script(std::type_index type) : type(type), name(Util::getNameOfType(type)) {}
	virtual ~Script() = default;

	//if a script is disabled in the same frame of construction then init will not be called
	virtual void init() { }
	virtual void update() { }
	virtual void handleEvent(const sf::Event&) { }

	template <typename T> T* getComponent() { return m_entity.tryGetComponent<T>(); }
	template <typename T> T* getScript() { return m_entity.getScript<T>(); };
	Entity entity() { return m_entity; }
	void deactivate() { m_entity.setScriptActive(type, false); }
	bool isActive() { return active; }

	const std::string_view name;

private:
	Entity m_entity;
	bool active = true;
	bool isInitialized = false;
	std::type_index type;

	friend class EntityManager;
	friend class ScriptManager;
	friend class SceneInspector;
};

template <typename T>
int addScriptTypeToFactorie();

// helper class
template <typename T>
class ScriptT : public Script {
	// calling this function at start up time
	static inline const auto _ = addScriptTypeToFactorie<T>();
public:
	ScriptT() : Script(typeid(T))
	{
		static_assert(std::is_base_of_v<Script, T>);
		static_assert(std::is_default_constructible_v<T>, "Script needs to have default constructor");
	}
};


class ScriptManager final : public NonCopyable {

public:
	ScriptManager() = default;
	~ScriptManager() = default;

	template <typename T, typename... Args>
	T* addScript(int16_t& indexOfPool, Args&&... args)
	{
		auto createScript = [&]() {
			return std::make_unique<T>(std::forward<Args>(args)...);
		};
		auto script = addScriptImpl(indexOfPool, typeid(T), createScript);
		return dynamic_cast<T*>(script);
	}

	Script* addScript(int16_t& indexOfPool, std::type_index type)
	{
		auto& factorie = factories.at(type);
		return addScriptImpl(indexOfPool, type, factorie);
	}	

	Script* addScriptImpl(int16_t& indexOfPool, std::type_index type, std::function<std::unique_ptr<Script>()> factorie)
	{
		if (indexOfPool == ArkInvalidIndex) {
			if (!freePools.empty()) {
				indexOfPool = freePools.back();
				freePools.pop_back();
			} else {
				indexOfPool = scriptPools.size();
				scriptPools.emplace_back();
			}
		} else if (hasScript(indexOfPool, type)) {
			return getScript(indexOfPool, type);
		}

		auto& scripts = scriptPools.at(indexOfPool);
		auto& script = scripts.emplace_back(factorie());
		pendingScripts.push_back(script.get());

		return script.get();
	}

	bool hasScript(int indexOfPool, std::type_index type) {
		return getScript(indexOfPool, type) != nullptr;
	}

	template <typename T>
	T* getScript(int indexOfPool)
	{
		return dynamic_cast<T*>(getScript(indexOfPool, typeid(T)));
	}

	Script* getScript(int indexOfPool, std::type_index type)
	{
		if (indexOfPool == ArkInvalidIndex)
			return nullptr;

		auto& scripts = scriptPools.at(indexOfPool);
		for (auto& script : scripts)
			if (script->type == type)
				return script.get();
		return nullptr;
	}

	const std::vector<std::unique_ptr<Script>>& getScripts(int indexOfPool)
	{
		return scriptPools[indexOfPool];
	}

	void setActive(int indexOfPool, bool active, std::type_index type)
	{
		auto script = getScript(indexOfPool, type);
		if (!script)
			return;

		if (script->active && !active) {
			script->active = false;
			pendingDeactivatedScripts.push_back(script);
		} else if (!script->active && active) {
			Util::erase(pendingDeactivatedScripts, script);
			script->active = true;
			auto isCurrentlyActive = Util::find(activeScripts, script);
			auto isPending = Util::find(pendingScripts, script);
			if (!isCurrentlyActive && !isPending)
				pendingScripts.push_back(script);
		}
	}

	void processPendingScripts()
	{
		for (auto& delScript : scriptsToBeDeleted) {
			deleteScriptFromLists(delScript.first, delScript.second);
			deleteScript(delScript.first, delScript.second);
		}
		scriptsToBeDeleted.clear();

		if (pendingScripts.empty() && pendingDeactivatedScripts.empty())
			return;

		for (auto script : pendingScripts) {
			if (script->active) {
				activeScripts.push_back(script);
				if (!script->isInitialized) {
					script->init();
					script->isInitialized = true;
				}
			}
		}
		pendingScripts.clear();

		for (auto script : pendingDeactivatedScripts)
			if(!script->active)
				Util::erase(activeScripts, script);
		pendingDeactivatedScripts.clear();
	}

	// used by Scene to call handleEvent, handleMessage, update
	template <typename F>
	void forEachScript(F f)
	{
		for (auto script : activeScripts)
			f(script);
	}

	void removeScript(int indexOfPool, std::type_index type)
	{
		if (indexOfPool == ArkInvalidIndex)
			return;

		scriptsToBeDeleted.emplace_back(indexOfPool, type);
	}

	void removeScripts(int indexOfPool)
	{
		if (indexOfPool == ArkInvalidIndex)
			return;

		auto& scripts = scriptPools.at(indexOfPool);
		for (auto& script : scripts) {
			EngineLog(LogSource::ScriptM, LogLevel::Info, "deleting (%s)", script->name.data());
			deleteScriptFromLists(indexOfPool, script->type);
			Util::erase(scriptsToBeDeleted, std::pair{indexOfPool, script->type});
		}
			
		scripts.clear();
		freePools.push_back(indexOfPool);
	}

private:
	void deleteScriptFromLists(int indexOfPool, std::type_index type) {
		auto script = getScript(indexOfPool, type);
		Util::erase(activeScripts, script);
		Util::erase(pendingScripts, script);
		Util::erase(pendingDeactivatedScripts, script);
	}

	void deleteScript(int indexOfPool, std::type_index type) {
		Util::erase_if(scriptPools.at(indexOfPool), [type](auto& p) { return p->type == type; });
	}

private:
	std::vector<std::vector<std::unique_ptr<Script>>> scriptPools;
	std::vector<int> freePools;
	std::vector<Script*> activeScripts;
	std::vector<Script*> pendingScripts; // set to active and/or initialize
	std::vector<Script*> pendingDeactivatedScripts;
	std::vector<std::pair<int, std::type_index>> scriptsToBeDeleted; // indexOfPool and type
	static inline std::unordered_map<std::type_index, std::function<std::unique_ptr<Script>()>> factories;
	friend class EntityManager;
	friend class SceneInspector;

	template <typename T>
	friend int addScriptTypeToFactorie()
	{
		static_assert(std::is_base_of_v<Script, T>);
		static_assert(std::is_default_constructible_v<T>, "Script needs to have default constructor");

		auto& factories = ScriptManager::factories;
		if (auto it = factories.find(typeid(T)); it == factories.end()) {
			factories[typeid(T)] = []() -> std::unique_ptr<Script> {
				return std::make_unique<T>();
			};
		}
		return 0;
	}
};
