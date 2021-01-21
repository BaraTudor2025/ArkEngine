#pragma once

#include "ark/ecs/System.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/DefaultServices.hpp"

class Script {

public:
	Script(std::type_index type) : mType(type) {}
	Script(const Script&) = default;
	virtual ~Script() = default;

	// scripts are immediatly binded
	virtual void bind() noexcept = 0;
	virtual void handleEvent(const sf::Event& ev) noexcept {}
	virtual void update() noexcept {}

	template <typename T> T* getComponent() { return mEntity.tryGetComponent<T>(); }

	ark::Entity entity() { return mEntity; }
	void setActive(bool isActive) { mIsActive = isActive; }
	bool isActive() const { return mIsActive; }

	__declspec(property(get = getType))
		std::type_index type;

	std::type_index getType() const { return mType; }

	__declspec(property(get = getRegistry))
		ark::Registry& registry;

	__declspec(property(get = getSystemManager))
		ark::SystemManager& systemManager;

	ark::Registry& getRegistry() { return *mRegistry; }

private:
	bool mIsActive = true;
	ark::Entity mEntity;
	ark::Registry* mRegistry;
	std::type_index mType;

	friend class ScriptingSystem;
	friend struct ScriptingComponent;
	friend void deserializeScriptComponents(ark::Entity&, const nlohmann::json& obj, void* p);
};

inline const std::string_view gScriptGroupName = "ark_scripts";

template <typename T>
void registerScript(bool customRegistration)
{
	if (nullptr == ark::meta::getMetadata(typeid(T)))
		ark::meta::registerMetadata<T>();
	if (not customRegistration) {
		using Type = T;
		registerServiceDefault<T>();
	}
	auto* mdata = ark::meta::getMetadata(typeid(T));
	mdata->func("unique_ptr", +[]() -> std::unique_ptr<Script> { return std::make_unique<T>(); });
	mdata->func("clone", +[](Script* script) -> std::unique_ptr<Script> { return std::make_unique<T>(*static_cast<const T*>(script)); });
	ark::meta::addTypeToGroup(gScriptGroupName, typeid(T));
}

template <typename T, bool CustomRegistration = false>
class ScriptT : public Script {
	// as fi folosit o lambda, dar da eroarea: use of undefined type; pentru ca T este derivata, 
	// si cum clasa inca nu este definita, nu poate fi instantata template
	static inline auto _ = (registerScript<T>(CustomRegistration), 0);
public:
	ScriptT() : Script(typeid(T)) {}
	ScriptT(const ScriptT&) = default;
};


struct ScriptingComponent {

	ScriptingComponent() = default;
	ScriptingComponent(ScriptingComponent&&) = default;
	/* NO copy ctor */
	ScriptingComponent(const ScriptingComponent&) = delete;
	//ScriptingComponent(ark::Entity, ark::Registry) = delete;
	//ScriptingComponent(const ScriptingComponent&, ark::Entity, ark::Registry) = delete;

	static auto onConstruction() // -> std::function<void(ScriptingComponent& comp, ark::Entity e, ark::Registry& reg)>
	{
		return [](ScriptingComponent& comp, ark::Entity e, ark::Registry& reg) {
			comp._setEntity(e);
			comp._setScene(&reg);
		};
	}

	static void onCopy(ScriptingComponent& This, const ScriptingComponent& other) {
		for (const auto& script : other.mScripts)
			This.copyScript(script.get());
	}

	template <typename T, typename... Args>
	T* addScript(Args&&... args)
	{
		if (auto s = getScript(typeid(T))) {
			return static_cast<T*>(s);
		}
		else {
			static_assert(std::is_base_of_v<Script, T>);
			auto script = mScripts.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)).get();
			_bindScript(script);
			return static_cast<T*>(script);
		}
	}

	Script* addScript(std::type_index type)
	{
		if (auto s = getScript(type))
			return s;
		else {
			auto create = ark::meta::getMetadata(type)->func<std::unique_ptr<Script>()>("unique_ptr");
			auto script = mScripts.emplace_back(create()).get();
			_bindScript(script);
			return script;
		}
	}

	Script* copyScript(Script* toCopy)
	{
		if (auto s = getScript(toCopy->type))
			return s;
		else {
			auto clone = ark::meta::getMetadata(toCopy->type)->func<std::unique_ptr<Script>(Script*)>("clone");
			auto script = mScripts.emplace_back(clone(toCopy)).get();
			_bindScript(script);
			return script;
		}
	}

	template <typename T>
	T* getScript()
	{
		return dynamic_cast<T*>(getScript(typeid(T)));
	}

	Script* getScript(std::type_index type)
	{
		for (auto& script : mScripts)
			if (script->mType == type)
				return script.get();
		return nullptr;
	}

	void setActive(std::type_index type, bool isActive)
	{
		Script* pScript = getScript(type);
		pScript->mIsActive = isActive; // add buffering?
	}

	template <typename T>
	void setActive(bool isActive) { setActive(typeid(T), isActive); }

	void removeScript(std::type_index type)
	{
		if (Script* p = getScript(type))
			mToBeDeleted.push_back(p);
	}

	template <typename T>
	void removeScript() { removeScript(typeid(T)); }

	void _setEntity(ark::Entity e)
	{
		mEntity = e;
	}

	void _setScene(ark::Registry* registry)
	{
		mRegistry = registry;
	}

private:

	void _bindScript(Script* script)
	{
		script->mEntity = mEntity;
		script->mRegistry = mRegistry;
		script->bind();
	}

	ark::Entity mEntity;
	ark::Registry* mRegistry;
	std::vector<std::unique_ptr<Script>> mScripts;
	std::vector<Script*> mToBeDeleted;

	friend bool renderScriptComponents(int* widgetId, void* pvScriptComponent);
	friend nlohmann::json serializeScriptComponents(const void* p);
	friend void deserializeScriptComponents(ark::Entity&, const nlohmann::json& obj, void* p);
	friend class ScriptingSystem;
};

static nlohmann::json serializeScriptComponents(const void* pvScriptComponent)
{
	nlohmann::json jsonScripts;
	const ScriptingComponent* scriptingComp = static_cast<const ScriptingComponent*>(pvScriptComponent);
	for (const auto& script : scriptingComp->mScripts) {
		if (auto serialize = ark::meta::getMetadata(script->type)->func<nlohmann::json(const void*)>(ark::serde::serviceSerializeName)) {
			const auto* mdata = ark::meta::getMetadata(script->type);
			jsonScripts[mdata->name] = serialize(script.get());
		}
	}
	return jsonScripts;
}

static void deserializeScriptComponents(ark::Entity& entity, const nlohmann::json& jsonScripts, void* pvScriptComponent)
{
	ScriptingComponent* scriptingComp = static_cast<ScriptingComponent*>(pvScriptComponent);
	// allocate scripts and default construct
	for (const auto& [scriptName, _] : jsonScripts.items()) {
		const auto* mdata = ark::meta::getMetadata(scriptName);
		auto* pScript = scriptingComp->addScript(mdata->type);
	}
	// then initialize
	scriptingComp->mEntity = entity;
	for(auto& script : scriptingComp->mScripts){
		const auto* mdata = ark::meta::getMetadata(script->type);
		script->mEntity = scriptingComp->mEntity;
		script->mRegistry = scriptingComp->mRegistry;
		script->bind();
		if (auto deserialize = ark::meta::getMetadata(mdata->type)->func<void(ark::Entity&, const nlohmann::json&, void*)>(ark::serde::serviceDeserializeName )) {
			deserialize(entity, jsonScripts.at(mdata->name), script.get());
		}
	}
}

static bool renderScriptComponents(int* widgetId, void* pvScriptComponent)
{
	ScriptingComponent* scriptingComp = static_cast<ScriptingComponent*>(pvScriptComponent);
	for (auto& script : scriptingComp->mScripts) {
		const auto* mdata = ark::meta::getMetadata(script->type);

		//ark::SceneInspector::renderPropertiesOfType
		// TODO script ???
		if (auto render = ark::meta::getMetadata(script->type)->func<bool(int*, void*)>(ark::SceneInspector::serviceName)) {
			ImGui::AlignTextToFramePadding();
			if (ImGui::TreeNodeEx(mdata->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
				bool deleted = ark::AlignButtonToRight("remove script", [&]() {
					scriptingComp->removeScript(script->type);
				});
				if (!deleted) {
					ImGui::PushID(*widgetId);
					ark::ArkSetFieldName("is-active");
					bool bIsActive = script->isActive();
					if (ImGui::Checkbox("", &bIsActive))
						script->setActive(bIsActive);
					render(widgetId, script.get());
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}
		ImGui::Separator();

	}
	const auto& typeList = ark::meta::getTypeGroup(gScriptGroupName);

	auto scriptGetter = [](void* data, int index, const char** out_text) mutable -> bool {
		const auto& types = *static_cast<std::remove_reference_t<decltype(typeList)>*>(data);
		auto it = types.begin();
		std::advance(it, index);
		*out_text = ark::meta::getMetadata(*it)->name.c_str();
		return true;
	};
	int scriptItemIndex;
	if (ImGui::Combo("add script", &scriptItemIndex, scriptGetter, (void*)(&typeList), typeList.size())) {
		auto it = typeList.begin() + scriptItemIndex;
		scriptingComp->addScript(*it);
	}
	return false;
}

ARK_REGISTER_COMPONENT(ScriptingComponent, 0)
{
	return members<ScriptingComponent>(); 
}

class ScriptingSystem : public ark::SystemT<ScriptingSystem> {
public:
	ScriptingSystem() = default;

	void init() override
	{
		querry = entityManager.makeQuerry<ScriptingComponent>();
	}

	void update() override
	{
		for (ark::Entity e : getEntities()) {
			auto& scriptComp = e.getComponent<ScriptingComponent>();

			// delete script from previous frame
			if (not scriptComp.mToBeDeleted.empty()) {
				for (Script* pScript : scriptComp.mToBeDeleted) {
					std::erase_if(scriptComp.mScripts, [type = pScript->mType](const auto& script) { return script->mType == type; });
				}
				scriptComp.mToBeDeleted.clear();
			}

			// update
			for (auto& script : scriptComp.mScripts) {
				if (script->mIsActive)
					script->update();
			}
		}
	}

	void handleEvent(sf::Event event) override
	{
		forEachScript([&event](auto& script) {
			script->handleEvent(event);
		});
	}

	void handleMessage(const ark::Message& m) override
	{
		// nush
	}

private:
	template <typename F>
	void forEachScript(F f)
	{
		for (auto& e : getEntities()) {
			auto& scriptComp = e.getComponent<ScriptingComponent>();
			for (auto& script : scriptComp.mScripts) {
				if (script->mIsActive)
					f(script);
			}
		}
	}
};
