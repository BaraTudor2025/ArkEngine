#pragma once

#include "Entity.hpp"
#include "Component.hpp"

#include <vector>

class Scene;

class ARK_ENGINE_API System : public NonCopyable {

public:
	System() = default;
	virtual ~System() = 0;

	virtual void init() { }
	virtual void update() { }
	virtual void fixedUpdate() { }
	virtual void handleEvent(sf::Event) { }
	//virtual void handleMessage(Message&) { }
	virtual void render(sf::RenderTarget& target) { } // TODO: remove render function

protected:
	template <typename T>
	void requireComponent()
	{
		componentTypes.push_back(typeid(T));
	}

	template <typename F>
	void forEachEntity(F f)
	{
		for (auto entity : entities)
			f(entity);
	}

	Scene* scene() { return m_scene; }

	virtual void onEntityAdded(Entity) { }
	virtual void onEntityRemoved(Entity) { }

private:
	void addEntity(Entity e)
	{
		entities.push_back(e);
		onEntityAdded(e);
	}

	void removeEntity(Entity entity)
	{
		Util::erase_if(entities, [&entity, this](Entity e) {
			if (entity == e) {
				onEntityRemoved(e);
				return true;
			}
			return false;
		});
	}

	void constructMask(ComponentManager& cm)
	{
		for (auto type : componentTypes)
			componentMask.set(cm.getComponentId(type));
		componentTypes.clear();
	}

private:
	std::vector<Entity> entities;
	ComponentManager::ComponentMask componentMask;
	std::vector<std::type_index> componentTypes;
	Scene* m_scene = nullptr;
	friend class SystemManager;
};


class SystemManager {

public:
	SystemManager(ComponentManager& compMgr, Scene& scene): scene(scene), componentManager(compMgr) {}
	~SystemManager() = default;

	template <typename T>
	T* addSystem()
	{
		if (hasSystem<T>())
			return getSystem<T>();

		auto sys = systems.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		activeSystems.push_back(sys.get());
		sys->m_scene = scene;
		sys->constructMask(componentManager);

		return sys.get();
	}

	template <typename T>
	T* getSystem() 
	{
		for (auto& system : systems)
			if (auto s = dynamic_cast<T*>(system.get()); s)
				return s;
		return nullptr;
	}

	template <typename T>
	bool hasSystem()
	{
		return getSystem<T>() != nullptr;
	}

	template <typename T>
	void removeSystem()
	{
		auto system = getSystem<T>();
		if (system)
			Util::remove_if(systems, [system](auto& sys) {
				return sys.get() == system;
			});
	}

	// used by Scene to call handleEvent, handleMessage, update and fixedUpdate
	template <typename F>
	void forEachSystem(F f)
	{
		for (auto system : activeSystems)
			f(system);
	}

	template <typename T>
	void setSystemActive(bool active)
	{
		auto system = getSystem<T>();
		if (!system)
			return;
		auto activeSystem = Util::find(activeSystems, system);

		if(activeSystem && !active)
			Util::erase(activeSystems, system);
		else if (!activeScript && active)
			activeSystems.push_back(system);
	}

	void addToSystems(Entity entity) 
	{
		auto entityMask = entity.getComponentMask();
		for (auto& system : systems)
			if(system->componentMask == entityMask)
				system->addEntity(entity);
	}

	void removeFromSystems(Entity entity) 
	{
		for (auto& system : systems)
			system->removeEntity(entity);
	}


private:
	std::vector<std::unique_ptr<System>> systems;
	std::vector<System*> activeSystems;
	ComponentManager& componentManager;
	Scene& scene;
};
