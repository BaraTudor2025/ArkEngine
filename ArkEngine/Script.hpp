#pragma once

#include "Core.hpp"
#include "Util.hpp"
#include "Entity.hpp"

#include <SFML/Window/Event.hpp>

class Entity;

// TODO (script): maybe try to return nullptr if component isn't found?
class ARK_ENGINE_API Script : public NonCopyable {

public:
	Script() = default;
	virtual ~Script() = default;

	virtual void init() { }
	virtual void update() { }
	virtual void handleEvent(const sf::Event&) { }
	template <typename T> T* getComponent() { return &m_entity.getComponent<T>(); }
	template <typename T> T* getScript() { return m_entity.getScript<T>(); };
	Entity entity() { return m_entity; }

private:
	Entity m_entity;

	friend class EntityManager;
	friend class ScriptManager;
};


class ScriptManager final : public NonCopyable {

public:
	ScriptManager() = default;
	~ScriptManager() = default;

	template <typename T, typename... Args>
	T* addScript(int16_t& indexOfPool, Args&& ... args)
	{
		if (indexOfPool == ArkInvalidIndex) {
			if (!freePools.empty()) {
				indexOfPool = freePools.back();
				freePools.pop_back();
			} else {
				indexOfPool = scriptPools.size();
				scriptPools.emplace_back();
			}
		} else if (hasScript<T>(indexOfPool)) {
			return getScript<T>(indexOfPool);
		}

		auto& scripts = scriptPools.at(indexOfPool);
		auto& script = scripts.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		pendingScripts.push_back(script.get());

		return dynamic_cast<T*>(script.get());
	}
	
	template <typename T>
	bool hasScript(int indexOfPool) {
		return getScript<T>(indexOfPool) != nullptr;
	}

	template <typename T>
	T* getScript(int indexOfPool)
	{
		if (indexOfPool == ArkInvalidIndex)
			return nullptr;

		auto& scripts = scriptPools.at(indexOfPool);
		for (auto& script : scripts)
			if (auto s = dynamic_cast<T*>(script.get()); s)
				return s;

		return nullptr;
	}

	template <typename T>
	void setActive(int indexOfPool, bool active)
	{
		auto script = getScript<T>(indexOfPool);
		if (!script)
			return;

		auto activeScript = Util::find(activeScripts, script);
		if (activeScript && !active)
			deactivatedPendingScripts.push_back(script);
		else if (!activeScript && active)
			activatedPendingScripts.push_back(script);
	}

	void processPendingScripts()
	{
		if (pendingScripts.empty() && activatedPendingScripts.empty() && deactivatedPendingScripts.empty())
			return;

		Util::push_back_range(activeScripts, pendingScripts);
		for (auto script : pendingScripts)
			script->init();
		pendingScripts.clear();

		Util::push_back_range(activeScripts, activatedPendingScripts);
		activatedPendingScripts.clear();

		for (auto script : deactivatedPendingScripts)
			Util::erase(activeScripts, script);
		deactivatedPendingScripts.clear();
	}

	// f must take Script* as arg
	// used by Scene to call handleEvent, handleMessage, update
	template <typename F>
	void forEachScript(F f)
	{
		for (auto script : activeScripts)
			f(script);
	}

	template <typename T>
	void removeScript(int indexOfPool)
	{
		if (indexOfPool == ArkInvalidIndex)
			return;

		auto script = getScript<T>(indexOfPool);
		if (script) {
			Util::erase(activeScripts, script);
			Util::erase_if(scriptPools, [script](auto& p) { return p.get() == script; });
		}
	}

	void removeScripts(int indexOfPool)
	{
		if (indexOfPool == ArkInvalidIndex)
			return;

		auto& scripts = scriptPools.at(indexOfPool);
		for (auto& script : scripts)
			Util::erase_if(activeScripts, [&script](auto& s) { return s == script.get(); });
			
		scripts.clear();
		freePools.push_back(indexOfPool);
	}

private:
	std::vector<std::vector<std::unique_ptr<Script>>> scriptPools;
	std::vector<int> freePools;
	std::vector<Script*> activeScripts;
	std::vector<Script*> pendingScripts;
	std::vector<Script*> deactivatedPendingScripts;
	std::vector<Script*> activatedPendingScripts;
	friend class EntityManager;
};
