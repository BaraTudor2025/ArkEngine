#pragma once

#include "ark/ecs/System.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/DefaultServices.hpp"

class ScriptClass : public NonCopyable {

public:
	ScriptClass(std::type_index type) : mType(type) {}
	virtual ~ScriptClass() = default;

	virtual void bind() noexcept = 0;
	virtual void handleEvent(const sf::Event& ev) noexcept {}
	virtual void update() noexcept {}

	template <typename T> T* getComponent() { return mEntity.tryGetComponent<T>(); }

	ark::Entity entity() { return mEntity; }
	void setActive(bool isActive) { mIsActive = isActive; }

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
	friend void deserializeScriptComponents(ark::Scene&, ark::Entity&, const nlohmann::json& obj, void* p);
};

inline const std::string_view gScriptGroupName = "ark_scripts";

template <typename T>
void registerScript()
{
	using Type = T;
	ark::meta::constructMetadataFrom<T>(typeid(T).name());
	ark::meta::services<T>(ARK_DEFAULT_SERVICES);
	ark::meta::services<T>(ark::meta::service("unique_ptr", []() -> std::unique_ptr<ScriptClass> { return std::make_unique<T>(); }));
	ark::meta::addTypeToGroup(gScriptGroupName, typeid(T));
}

template <typename T>
class ScriptClassT : public ScriptClass {
	static inline auto _ = (registerScript<T>(), 0);
public:
	ScriptClassT() : ScriptClass(typeid(T)) {}
};


struct ScriptingComponent : public NonCopyable, public ark::Component<ScriptingComponent> {

	ScriptingComponent() = default;

	// if entity is provided then scripts will be binded immediatly,
	// oherwise they will be binded next frame
	// next frames, after the creation of the scripting component, scripts will be binded immediatly
	ScriptingComponent(ark::Entity e) : mEntity(e) {}

	template <typename T, typename... Args>
	T* addScript(Args&&... args)
	{
		if (auto s = getScript(typeid(T))) {
			return static_cast<T*>(s);
		}
		else {
			static_assert(std::is_base_of_v<ScriptClass, T>);
			auto& script = mScripts.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			if (mEntity.isValid()) {
				script->mEntity = mEntity;
				script->mScene = mScene;
				script->bind();
			}
			return static_cast<T*>(script.get());
		}
	}

	ScriptClass* addScript(std::type_index type)
	{
		if (auto s = getScript(type))
			return s;
		else {
			auto create = ark::meta::getService<std::unique_ptr<ScriptClass>()>(type, "unique_ptr");
			auto& script = mScripts.emplace_back(create());
			if (mEntity.isValid()) {
				script->mEntity = mEntity;
				script->mScene = mScene;
				script->bind();
			}
			return script.get();
		}
	}

	template <typename T>
	T* getScript()
	{
		return dynamic_cast<T*>(getScript(typeid(T)));
	}

	ScriptClass* getScript(std::type_index type)
	{
		for (auto& script : mScripts)
			if (script->mType == type)
				return script.get();
		return nullptr;
	}

	void setActive(std::type_index type, bool isActive)
	{
		ScriptClass* pScript = getScript(type);
		pScript->mIsActive = isActive; // add buffering?
	}

	template <typename T>
	void setActive(bool isActive) { setActive(typeid(T), isActive); }

	void removeScript(std::type_index type)
	{
		if (ScriptClass* p = getScript(type))
			mToBeDeleted.push_back(p);
	}

	template <typename T>
	void removeScript() { removeScript(typeid(T)); }

private:
	ark::Entity mEntity;
	ark::Scene* mScene;
	std::vector<std::unique_ptr<ScriptClass>> mScripts;
	std::vector<ScriptClass*> mToBeDeleted;

	friend void renderScriptComponents(int* widgetId, void* pvScriptComponent);
	friend nlohmann::json serializeScriptComponents(const void* p);
	friend void deserializeScriptComponents(ark::Scene&, ark::Entity&, const nlohmann::json& obj, void* p);
	friend class ScriptingSystem;
};

static nlohmann::json serializeScriptComponents(const void* pvScriptComponent)
{
	nlohmann::json jsonScripts;
	const ScriptingComponent* scriptingComp = static_cast<const ScriptingComponent*>(pvScriptComponent);
	for (const auto& script : scriptingComp->mScripts) {
		if (auto serialize = ark::meta::getService<nlohmann::json(const void*)>(script->type, ark::SerdeJsonDirector::serviceSerializeName)) {
			const auto* mdata = ark::meta::getMetadata(script->type);
			jsonScripts[mdata->name.data()] = serialize(script.get());
		}
	}
	return jsonScripts;
}

static void deserializeScriptComponents(ark::Scene& scene, ark::Entity& entity, const nlohmann::json& jsonScripts, void* pvScriptComponent)
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
		script->mEntity = entity;
		script->mScene = &scene;
		script->bind();
		if (auto deserialize = ark::meta::getService<void(ark::Scene&, ark::Entity&, const nlohmann::json&, void*)>(mdata->type, ark::SerdeJsonDirector::serviceDeserializeName )) {
			deserialize(scene, entity, jsonScripts.at(mdata->name.data()), script.get());
		}
	}
}

static void renderScriptComponents(int* widgetId, void* pvScriptComponent)
{
	ScriptingComponent* scriptingComp = static_cast<ScriptingComponent*>(pvScriptComponent);
	for (auto& script : scriptingComp->mScripts) {
		const auto* mdata = ark::meta::getMetadata(script->type);

		if (auto render = ark::meta::getService<void(int*, void*)>(script->type, ark::SceneInspector::serviceName)) {
			ImGui::AlignTextToFramePadding();
			if (ImGui::TreeNodeEx(mdata->name.data(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
				bool deleted = ark::AlignButtonToRight("remove script", [&]() {
					scriptingComp->removeScript(script->type);
				});
				if(!deleted)
					render(widgetId, script.get());
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
		//*out_text = it->name();
		*out_text = ark::meta::getMetadata(*it)->name.data();
		return true;
	};
	int scriptItemIndex;
	if (ImGui::Combo("add script", &scriptItemIndex, scriptGetter, (void*)(&typeList), typeList.size())) {
		auto it = typeList.begin() + scriptItemIndex;
		scriptingComp->addScript(*it);
	}
}


ARK_REGISTER_TYPE(ScriptingComponent, "ScriptingComponent", 
	ark::meta::service(ark::SceneInspector::serviceName, renderScriptComponents),
	ark::meta::service(ark::SerdeJsonDirector::serviceSerializeName, serializeScriptComponents),
	ark::meta::service(ark::SerdeJsonDirector::serviceDeserializeName, deserializeScriptComponents)
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

	void onEntityAdded(ark::Entity entity) override
	{
		auto& scriptComp = entity.getComponent<ScriptingComponent>();
		scriptComp.mEntity = entity;
		scriptComp.mScene = scene();
		for (auto& script : scriptComp.mScripts) {
			if (not script->mEntity.isValid()) {
				script->mEntity = entity;
				script->mScene = scene();
				script->bind();
			}
		}
	}

	void update() override
	{
		for (ark::Entity e : getEntities()) {
			auto& scriptComp = e.getComponent<ScriptingComponent>();

			// delete script from previous frame
			if (not scriptComp.mToBeDeleted.empty()) {
				for (ScriptClass* pScript : scriptComp.mToBeDeleted) {
					Util::erase_if(scriptComp.mScripts, [pScript](const auto& script) { return script->mType == pScript->mType; });
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
		for (auto e : getEntities()) {
			auto& scriptComp = e.getComponent<ScriptingComponent>();
			for (auto& script : scriptComp.mScripts) {
				if (script->mIsActive)
					f(script);
			}
		}
	}
};
