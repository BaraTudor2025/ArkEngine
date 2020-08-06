#pragma once

#include "ark/ecs/System.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/DefaultServices.hpp"

class Script : public NonCopyable {

public:
	Script(std::type_index type) : mType(type) {}
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

	__declspec(property(get = getScene))
		ark::Scene& scene;

	ark::Scene& getScene() { return *mScene; }

private:
	bool mIsActive = true;
	ark::Entity mEntity;
	ark::Scene* mScene;
	std::type_index mType;

	friend class ScriptingSystem;
	friend struct ScriptingComponent;
	friend void deserializeScriptComponents(ark::Entity&, const nlohmann::json& obj, void* p);
};

inline const std::string_view gScriptGroupName = "ark_scripts";

template <typename T>
void registerScript(bool customRegistration)
{
	if (not customRegistration) {
		using Type = T;
		ARK_DEFAULT_SERVICES;
		ark::meta::constructMetadataFrom<T>();
	}
	ark::meta::service<T>("unique_ptr", []() -> std::unique_ptr<Script> { return std::make_unique<T>(); });
	ark::meta::addTypeToGroup(gScriptGroupName, typeid(T));
}

template <typename T, bool CustomRegistration = false>
class ScriptT : public Script {
	static inline auto _ = (registerScript<T>(CustomRegistration), 0);
public:
	ScriptT() : Script(typeid(T)) {}
};


struct ScriptingComponent : public NonCopyable, public ark::Component<ScriptingComponent> {

	ScriptingComponent() = default;

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
			auto create = ark::meta::getService<std::unique_ptr<Script>()>(type, "unique_ptr");
			auto script = mScripts.emplace_back(create()).get();
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

	void _setScene(ark::Scene* scene)
	{
		mScene = scene;
	}

private:

	void _bindScript(Script* script)
	{
		script->mEntity = mEntity;
		script->mScene = mScene;
		script->bind();
	}

	ark::Entity mEntity;
	ark::Scene* mScene;
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
		if (auto serialize = ark::meta::getService<nlohmann::json(const void*)>(script->type, ark::SerdeJsonDirector::serviceSerializeName)) {
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
		script->mScene = scriptingComp->mScene;
		script->bind();
		if (auto deserialize = ark::meta::getService<void(ark::Entity&, const nlohmann::json&, void*)>(mdata->type, ark::SerdeJsonDirector::serviceDeserializeName )) {
			deserialize(entity, jsonScripts.at(mdata->name), script.get());
		}
	}
}

static bool renderScriptComponents(int* widgetId, void* pvScriptComponent)
{
	ScriptingComponent* scriptingComp = static_cast<ScriptingComponent*>(pvScriptComponent);
	for (auto& script : scriptingComp->mScripts) {
		const auto* mdata = ark::meta::getMetadata(script->type);

		if (auto render = ark::meta::getService<bool(int*, void*)>(script->type, ark::SceneInspector::serviceName)) {
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

ARK_REGISTER_TYPE(ScriptingComponent, "ScriptingComponent", 
	ark::meta::service<ScriptingComponent>(ark::SceneInspector::serviceName, renderScriptComponents),
	ark::meta::service<ScriptingComponent>(ark::SerdeJsonDirector::serviceSerializeName, serializeScriptComponents),
	ark::meta::service<ScriptingComponent>(ark::SerdeJsonDirector::serviceDeserializeName, deserializeScriptComponents)
)
{
	return members(); 
}

class ScriptingSystem : public ark::SystemT<ScriptingSystem> {
public:
	ScriptingSystem() = default;

	void init() override
	{
		requireComponent<ScriptingComponent>();
	}

	void update() override
	{
		for (ark::Entity e : getEntities()) {
			auto& scriptComp = e.getComponent<ScriptingComponent>();

			// delete script from previous frame
			if (not scriptComp.mToBeDeleted.empty()) {
				for (Script* pScript : scriptComp.mToBeDeleted) {
					Util::erase_if(scriptComp.mScripts, [type = pScript->mType](const auto& script) { return script->mType == type; });
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
