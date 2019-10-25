#pragma once

#include "Entity.hpp"
#include "Component.hpp"
#include "Message.hpp"
#include "MessageBus.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <vector>

class Scene;

class ARK_ENGINE_API Renderer : public NonCopyable {

public:
	Renderer() = default;
	virtual ~Renderer() = default;

	virtual void render(sf::RenderTarget&) = 0;

};

class ARK_ENGINE_API System : public NonCopyable {

public:
	System(std::type_index type) : type(type), name(Util::getNameOfType(type)) {  }
	virtual ~System() = default;

	virtual void update() { }
	virtual void handleEvent(sf::Event) { }
	virtual void handleMessage(const Message&) { }

	bool isActive() { return active; }

protected:
	template <typename T>
	void requireComponent() {
		componentTypes.push_back(typeid(T));
	}

	template <typename T>
	T* postMessage(int id) {
		return messageBus->post<T>(id);
	}

	std::vector<Entity>& getEntities() { return entities; }

	Scene* scene() { return m_scene; }

	virtual void onEntityAdded(Entity) { }
	virtual void onEntityRemoved(Entity) { }

private:
	void addEntity(Entity e)
	{
		EngineLog(LogSource::SystemM, LogLevel::Info, "On (%s) added entity (%s)", Util::getNameOfType(type), e.getName().c_str());
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
			componentNames.push_back(Util::getNameOfType(type));

		for (auto type : componentTypes)
			componentMask.set(cm.getComponentId(type));
		componentTypes.clear();
	}

private:
	friend class SystemManager;
	std::vector<Entity> entities;
	std::vector<std::type_index> componentTypes;
	std::vector<std::string_view> componentNames;
	ComponentManager::ComponentMask componentMask;
	Scene* m_scene = nullptr;
	MessageBus* messageBus = nullptr;
	std::type_index type;
	std::string_view name;
	bool active = true;
};


class SystemManager {

public:
	SystemManager(MessageBus& bus, Scene& scene, ComponentManager& compMgr): messageBus(bus), scene(scene), componentManager(compMgr) {}
	~SystemManager() = default;

	template <typename T, typename...Args>
	T* addSystem(Args&&... args)
	{
		if (hasSystem<T>())
			return getSystem<T>();

		auto& sys = systems.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
		activeSystems.push_back(sys.get());
		sys->m_scene = &scene;
		sys->messageBus = &messageBus;
		sys->constructMask(componentManager);
		if (sys->componentMask.none())
			EngineLog(LogSource::SystemM, LogLevel::Warning, "(%s) dosen't have any component requirements", Util::getNameOfType(sys->type));

		return dynamic_cast<T*>(sys.get());
	}

	template <typename T>
	T* getSystem() 
	{
		if (System* sys = getSystem(typeid(T)); sys)
			return dynamic_cast<T*>(sys);
		else
			return nullptr;
	}

	System* getSystem(std::type_index type)
	{
		for (auto& system : systems)
			if (system->type == type)
				return system.get();
		return nullptr;
	}

	std::vector<System*> getSystems()
	{
		std::vector<System*> retSystems;
		for (auto& sys : systems)
			retSystems.push_back(sys.get());
		return retSystems;
	}

	template <typename T>
	bool hasSystem()
	{
		return hasSystem(typeid(T));
	}

	bool hasSystem(std::type_index type)
	{
		return getSystem(type) != nullptr;
	}

	template <typename T>
	void removeSystem()
	{
		if (auto system = getSystem<T>(); system) {
			Util::remove_if(systems, [system](auto& sys) {
				return sys.get() == system;
			});
		}
	}

	// used by Scene to call handleEvent, handleMessage, update
	template <typename F>
	void forEachSystem(F f)
	{
		for (auto system : activeSystems)
			f(system);
	}

	template <typename T>
	void setSystemActive(bool active)
	{
		setSystemActive(typeid(T), active);
	}

	void setSystemActive(std::type_index type, bool active)
	{
		System* system = getSystem(type);
		if (!system)
			return;

		auto activeSystem = Util::find(activeSystems, system);

		if (activeSystem && !active) {
			Util::erase(activeSystems, system);
			system->active = false;
		} else if (!activeSystem && active) {
			activeSystems.push_back(system);
			system->active = true;
		} 
	}

	void addToSystems(Entity entity) 
	{
		const auto& entityMask = entity.getComponentMask();
		for (auto& system : systems)
			if(system->componentMask.any())
				if((entityMask & system->componentMask) == system->componentMask)
					system->addEntity(entity);
	}

	void removeFromSystems(Entity entity)
	{
		for (auto& system : systems)
			system->removeEntity(entity);
	}

	// implemented in ManagerInspectors.cpp
	void renderInspector();

private:
	std::vector<std::unique_ptr<System>> systems;
	std::vector<System*> activeSystems;
	MessageBus& messageBus;
	Scene& scene;
	ComponentManager& componentManager;
};
