#pragma once

#include "ark/ecs/System.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/DefaultServices.hpp"

class ScriptClass : public NonCopyable {

public:
	ScriptClass(std::type_index type) : mType(type) { }
	virtual ~ScriptClass() = default;

	virtual void bind() noexcept = 0;
	virtual void handleEvent(const sf::Event& ev) noexcept {}
	virtual void update() noexcept {}

	template <typename T> T* getComponent() { return mEntity.tryGetComponent<T>(); }

	ark::Entity entity() { return mEntity; }
	void setActive(bool isActive) { mIsActive = isActive; }

private:
	bool mIsActive = true;
	ark::Entity mEntity;
	std::type_index mType;
	friend class ScriptingSystem;
	friend struct ScriptingComponent;
};

template <typename T>
class ScriptClassT : public ScriptClass {

	//static inline auto _ = []() { 
	//	ark::meta::constructMetadataFrom<T>(typeid(T).name()); 
	//	using Type = T;
	//	ark::meta::services(
	//		ARK_SERVICE_INSPECTOR
	//	);
	//	//ark::meta::service(ark::SceneInspector::serviceName, ark::SceneInspector::renderFieldsOfType<T>)
	//}();

public:
	ScriptClassT() : ScriptClass(typeid(T)) { }
};


struct ScriptingComponent : public NonCopyable, public ark::Component<ScriptingComponent> {

	ScriptingComponent() = default;
	ScriptingComponent(ark::Entity e) : mEntity(e) {}

	template <typename T, typename... Args>
	T* addScript(Args&&... args)
	{
		static_assert(std::is_base_of_v<ScriptClass, T>);
		auto p = std::make_unique<T>(std::forward<Args>(args)...);
		T* pScript = p.get();
		auto& script = mScripts.emplace_back(std::move(p));
		if (mEntity.isValid())
			script->bind();
		return pScript;
	}

	template <typename T>
	T* getScript()
	{
		return dynamic_cast<T*>(findScript(typeid(T)));
	}

	template <typename T>
	void setActive(bool isActive)
	{
		ScriptClass* pScript = findScript(typeid(T));
		pScript->mIsActive = isActive; // add buffering?
	}

	template <typename T>
	void removeScript()
	{
		if (ScriptClass* p = findScript(typeid(T)); p)
			mToBeDeleted.push_back(p);
	}

private:

	ScriptClass* findScript(std::type_index type)
	{
		for (auto& script : mScripts)
			if (script->mType == type)
				return script.get();
		return nullptr;
		//return std::find(mScripts.begin(), mScripts.end(), [&type](const auto& s) { return s.mType == type; });
	}

	ark::Entity mEntity;
	std::vector<std::unique_ptr<ScriptClass>> mScripts;
	std::vector<ScriptClass*> mToBeDeleted;
	friend class ScriptingSystem;
};
ARK_REGISTER_TYPE(ScriptingComponent, "ScriptingComponent", ARK_DEFAULT_SERVICES) { return members(); }

class ScriptingSystem : public ark::SystemT<ScriptingSystem> {
public:
	ScriptingSystem() = default;

	void init() override
	{
		requireComponent<ScriptingComponent>();
	}

	void onEntityAdded(ark::Entity entity) override
	{
		auto& scripts = entity.getComponent<ScriptingComponent>().mScripts;
		for (auto& script : scripts) {
			if (not script->mEntity.isValid()) {
				script->mEntity = entity;
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

	//void handleMessage(const ark::Message& m) override
	//{
	//	// nush
	//}

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
