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

	template <typename T> T* getComponent() { return mEntity.tryGet<T>(); }

	ark::Entity entity() { return mEntity; }
	void setActive(bool isActive) { mIsActive = isActive; }
	bool isActive() const { return mIsActive; }

	__declspec(property(get = getType))
		std::type_index type;

	std::type_index getType() const { return mType; }

	__declspec(property(get = getRegistry))
		ark::EntityManager& registry;

	__declspec(property(get = getSystemManager))
		ark::SystemManager& systemManager;

	ark::EntityManager& getRegistry() { return *mManager; }

private:
	bool mIsActive = true;
	ark::Entity mEntity;
	ark::EntityManager* mManager;
	std::type_index mType;

	friend class ScriptingSystem;
	friend struct ScriptingComponent;
	friend void deserializeScriptComponents(ark::Entity&, const nlohmann::json& obj, void* p);
};

inline const std::string_view gScriptGroupName = "ark_scripts";

template <typename T>
void registerScript(bool customRegistration)
{
	auto* mdata = ark::meta::type<T>();
	if (not customRegistration)
		registerServiceDefault<T>();
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

	static auto onAdd(ark::EntityManager& man, ark::EntityId entity) {
		auto& comp = man.get<ScriptingComponent>(entity);
		comp.mEntity = ark::Entity{ entity, man };
		comp.mManager = &man;
	}

	static void onClone(ark::Entity This, ark::Entity That) {
		auto& thisComp = This.get<ScriptingComponent>();
		auto& thatComp = That.get<ScriptingComponent>();
		for (const auto& script : thatComp.mScripts)
			thisComp.copyScript(script.get());
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
			auto create = ark::meta::resolve(type)->func<std::unique_ptr<Script>()>("unique_ptr");
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
			auto clone = ark::meta::resolve(toCopy->type)->func<std::unique_ptr<Script>(Script*)>("clone");
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

	void _setScene(ark::EntityManager* registry)
	{
		mManager = registry;
	}

private:

	void _bindScript(Script* script)
	{
		script->mEntity = mEntity;
		script->mManager = mManager;
		script->bind();
	}

	ark::Entity mEntity;
	ark::EntityManager* mManager;
	std::vector<std::unique_ptr<Script>> mScripts;
	std::vector<Script*> mToBeDeleted;

	friend bool renderScriptComponents(std::type_index type, void* instance, int* widgetID, std::string_view label, ark::EditorOptions* parentOptions);
	friend nlohmann::json serializeScriptComponents(const void* p);
	friend void deserializeScriptComponents(ark::Entity&, const nlohmann::json& obj, void* p);
	friend class ScriptingSystem;
};

static nlohmann::json serializeScriptComponents(const void* pvScriptComponent)
{
	nlohmann::json jsonScripts;
	const ScriptingComponent* scriptingComp = static_cast<const ScriptingComponent*>(pvScriptComponent);
	for (const auto& script : scriptingComp->mScripts) {
		const auto* type = ark::meta::resolve(script->type);
		if (auto serialize = type->func<nlohmann::json(const void*)>(ark::serde::serviceSerializeName)) {
			jsonScripts[type->name] = serialize(script.get());
		}
	}
	return jsonScripts;
}

static void deserializeScriptComponents(ark::Entity& entity, const nlohmann::json& jsonScripts, void* pvScriptComponent)
{
	ScriptingComponent* scriptingComp = static_cast<ScriptingComponent*>(pvScriptComponent);
	// allocate scripts and default construct
	for (const auto& [scriptName, _] : jsonScripts.items()) {
		const auto* mdata = ark::meta::resolve(scriptName);
		auto* pScript = scriptingComp->addScript(mdata->type);
	}
	// then initialize
	scriptingComp->mEntity = entity;
	for(auto& script : scriptingComp->mScripts){
		const auto* mdata = ark::meta::resolve(script->type);
		script->mEntity = scriptingComp->mEntity;
		script->mManager = scriptingComp->mManager;
		script->bind();
		if (auto deserialize = mdata->func<void(ark::Entity&, const nlohmann::json&, void*)>(ark::serde::serviceDeserializeName)) {
			deserialize(entity, jsonScripts.at(mdata->name), script.get());
		}
	}
}

static bool renderScriptComponents(std::type_index, void* instance, int* widgetID, std::string_view, ark::EditorOptions*)
{
	ScriptingComponent* scriptingComp = static_cast<ScriptingComponent*>(instance);
	for (auto& script : scriptingComp->mScripts) {
		const auto* mdata = ark::meta::resolve(script->type);

		ImGui::AlignTextToFramePadding();
		if (ImGui::TreeNodeEx(mdata->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
			bool deleted = ark::AlignButtonToRight("remove script", [&]() {
				scriptingComp->removeScript(script->type);
			});
			if (!deleted) {
				ImGui::PushID(*widgetID);
				ark::ArkSetFieldName("is-active");
				bool bIsActive = script->isActive();
				if (ImGui::Checkbox("", &bIsActive))
					script->setActive(bIsActive);
				if (auto edit = mdata->func<ark::RenderTypeEditor>(ark::RENDER_EDITOR)) {
					(*widgetID)++;
					ImGui::PushID(*widgetID);
					edit(script->type, script.get(), widgetID, "", nullptr);
					ImGui::PopID();
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
			(*widgetID)++;
		}
		ImGui::Separator();

	}
	const auto& typeList = ark::meta::getTypeGroup(gScriptGroupName);

	auto scriptGetter = [](void* data, int index, const char** out_text) mutable -> bool {
		const auto& types = *static_cast<std::remove_reference_t<decltype(typeList)>*>(data);
		auto it = types.begin();
		std::advance(it, index);
		*out_text = ark::meta::resolve(*it)->name.c_str();
		return true;
	};
	int scriptItemIndex;
	if (ImGui::Combo("add script", &scriptItemIndex, scriptGetter, (void*)(&typeList), typeList.size())) {
		auto it = typeList.begin() + scriptItemIndex;
		scriptingComp->addScript(*it);
	}
	return false;
}

// ar trebui ca functiile sa fie date in main()
ARK_REGISTER_COMPONENT(ScriptingComponent, 0)
{
	return members<ScriptingComponent>(); 
}

class ScriptingSystem : public ark::SystemT<ScriptingSystem> {
	ark::View<ScriptingComponent> view;
public:
	ScriptingSystem() = default;

	void init() override
	{
		view = entityManager.view<ScriptingComponent>();
	}

	void update() override
	{
		for (auto& scriptComp : view) {
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
		for (auto& scriptComp : view) {
			for (auto& script : scriptComp.mScripts) {
				if (script->mIsActive)
					f(script);
			}
		}
	}
};
