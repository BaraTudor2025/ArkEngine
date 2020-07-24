#pragma once

#include <functional>

#include <SFML/Window/Event.hpp>

#include "ark/core/Core.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/util/Util.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/DefaultServices.hpp"

namespace ark {

	class Entity;

	class ARK_ENGINE_API Script : public NonCopyable {

	public:
		Script(std::type_index type) : mType(type), name(Util::getNameOfType(type)) {}
		virtual ~Script() = default;

		//if a script is disabled in the same frame of construction then init will not be called
		virtual void init() {}
		virtual void update() {}
		virtual void handleEvent(const sf::Event&) {}

		template <typename T> T* getComponent() { return m_entity.tryGetComponent<T>(); }
		template <typename T> T* getScript() { return m_entity.getScript<T>(); };
		Entity entity() { return m_entity; }
		void deactivate() { m_entity.setScriptActive(mType, false); }
		bool isActive() { return active; }

		const std::string_view name;
		__declspec(property(get = getType)) std::type_index type;
		std::type_index getType() const { return mType; }

	private:
		Entity m_entity;
		bool active = true;
		bool isInitialized = false;
		std::type_index mType;

		friend class EntityManager;
		friend class ScriptManager;
	};

	template <typename T>
	int registerScript();


	template <typename T>
	class ScriptT : public Script {

		static inline const auto _ = registerScript<T>();
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
			auto& ctor = metadata.at(type).construct;
			return addScriptImpl(indexOfPool, type, ctor);
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

		bool hasScript(int indexOfPool, std::type_index type)
		{
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
				if (script->mType == type)
					return script.get();
			return nullptr;
		}

		const std::vector<std::unique_ptr<Script>>& getScripts(int indexOfPool)
		{
			return scriptPools[indexOfPool];
		}

		std::type_index getTypeFromName(std::string_view name)
		{
			for (const auto& [type, _] : metadata) {
				if (name == Util::getNameOfType(type))
					return type;
			}
			EngineLog(LogSource::ScriptM, LogLevel::Error, "getTypeFromName() didn't find (%s)", name.data());
		}

		json serializeScripts(int indexOfPool)
		{
			if (indexOfPool == ArkInvalidIndex)
				return {};
			auto& scripts = scriptPools[indexOfPool];
			json jsonScripts;
			for (const auto& script : scripts)
				jsonScripts[script->name.data()] = metadata.at(script->mType).serialize(script.get());
			return jsonScripts;
		}

		void deserializeScripts(int indexOfPool, const json& jsonObj)
		{
			if (indexOfPool == ArkInvalidIndex)
				return;
			auto& scripts = scriptPools[indexOfPool];
			for (const auto& script : scripts) {
				const json& jsonScript = jsonObj.at(script->name.data());
				metadata.at(script->mType).deserialize(jsonScript, script.get());
			}
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
				if (!script->active)
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
				deleteScriptFromLists(indexOfPool, script->mType);
				Util::erase(scriptsToBeDeleted, std::pair{indexOfPool, script->mType});
			}

			scripts.clear();
			freePools.push_back(indexOfPool);
		}

	private:
		void deleteScriptFromLists(int indexOfPool, std::type_index type)
		{
			auto script = getScript(indexOfPool, type);
			Util::erase(activeScripts, script);
			Util::erase(pendingScripts, script);
			Util::erase(pendingDeactivatedScripts, script);
		}

		void deleteScript(int indexOfPool, std::type_index type)
		{
			Util::erase_if(scriptPools.at(indexOfPool), [type](auto& p) { return p->mType == type; });
		}

	private:
		std::vector<std::vector<std::unique_ptr<Script>>> scriptPools;
		std::vector<int> freePools;

		std::vector<Script*> activeScripts;
		std::vector<Script*> pendingScripts; // set to active and/or initialize
		std::vector<Script*> pendingDeactivatedScripts;
		std::vector<std::pair<int, std::type_index>> scriptsToBeDeleted; // indexOfPool and type

		friend class EntityManager;

		struct ScriptMetadata {
			std::function<std::unique_ptr<Script>()> construct;
			std::function<json(const void*)> serialize;
			std::function<void(const json&, void*)> deserialize;
		};
		static inline std::unordered_map<std::type_index, ScriptMetadata> metadata;

		template <typename T>
		friend int registerScript()
		{
			static_assert(std::is_base_of_v<Script, T>);
			static_assert(std::is_default_constructible_v<T>, "Script needs to have default constructor");

			auto newService = ark::meta::service("unique_ptr", []() -> std::unique_ptr<Script> { return std::make_unique<T>(); });

			ark::meta::constructMetadataFrom<T>(typeid(T).name());
			using Type = T;
			ark::meta::services<T>(ARK_SERVICE_INSPECTOR, newService);

			auto& metadata = ScriptManager::metadata[typeid(T)];
			metadata.serialize = serialize_value<T>;
			metadata.deserialize = deserialize_value<T>;
			metadata.construct = []() -> std::unique_ptr<Script> {
				return std::make_unique<T>();
			};
			return 0;
		}
	};
}
