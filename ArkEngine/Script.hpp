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
	virtual void fixedUpdate() { }
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

	// creates new pool
	// returns [script*, scriptIndex or pool index]
	template <typename T, typename... Args>
	std::pair<T*, int> addScript(Args&& ... args)
	{
		auto& scripts = scriptPools.emplace_back();

		auto& script = scripts.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		pendingScripts.push_back(script.get());

		return std::make_pair<T*, int>(dynamic_cast<T*>(script.get()), scriptPools.size() - 1);
	}

	// index of pool, the use 
	// return existing script if one already exsists
	template <typename T, typename... Args>
	T* addScriptAt(int indexOfPool, Args&& ... args)
	{
		if (hasScript<T>(indexOfPool))
			return getScript<T>(indexOfPool);

		auto& scripts = scriptPools.at(indexOfPool);
		auto& script = scripts.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		pendingScripts.push_back(script.get());

		return dynamic_cast<T*>(script.get());

		//auto& scripts = scriptPools.at(indexOfPool);
		//scripts.push_back(std::move(script));

		//return scripts.back().get();
	}
	
	template <typename T>
	bool hasScript(int indexOfPool) {
		return getScript<T>(indexOfPool) != nullptr;
	}

	template <typename T>
	T* getScript(int indexOfPool)
	{
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
			Util::erase(activeScripts, script);
		else if (!activeScript && active)
			activeScripts.push_back(script);
	}

	void processPendingScripts()
	{
		activeScripts.insert(activeScripts.end(), pendingScripts.begin(), pendingScripts.end());

		for (auto script : pendingScripts)
			script->init();

		pendingScripts.clear();
	}

	// f must take Script* as arg
	// used by Scene to call handleEvent, handleMessage, update and fixedUpdate
	template <typename F>
	void forEachScript(F f)
	{
		for (auto script : activeScripts)
			f(script);
	}

	template <typename T>
	void removeScript(int index)
	{
		auto script = getScript<T>(index);
		if (script) {
			Util::erase(activeScripts, script);
			Util::erase_if(scriptPools, [script](auto& p) { return p.get() == script; });
		}
	}

	void removeScripts(int index)
	{
		auto& scripts = scriptPools.at(index);
		for (auto& script : scripts) {
			Util::erase_if(activeScripts, [&script](auto& s) { return s == script.get(); });
		}
		Util::erase_at(scriptPools, index);
	}
	

private:
	std::vector<std::vector<std::unique_ptr<Script>>> scriptPools;
	std::vector<Script*> activeScripts;
	std::vector<Script*> pendingScripts;
	friend class EntityManager;
};

