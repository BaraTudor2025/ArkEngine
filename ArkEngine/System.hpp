#pragma once

#include "Entity.hpp"
#include "Component.hpp"
#include "Message.hpp"
#include "MessageBus.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <vector>

class Scene;

class ARK_ENGINE_API System : public NonCopyable {

public:
	System(std::type_index type) : type(type) {}
	virtual ~System();

	virtual void update() { }
	virtual void handleEvent(sf::Event) { }
	virtual void handleMessage(const Message&) { }
	virtual void render(sf::RenderTarget& target) { } // TODO (system): remove render function

protected:
	template <typename T>
	void requireComponent() {
		componentTypes.push_back(typeid(T));
	}

	template <typename F>
	void forEachEntity(F f) {
		for (auto entity : entities)
			f(entity);
	}

	template <typename T>
	T* post(int id) {
		return messageBus->post<T>(id);
	}

	std::vector<Entity>& getEntities() { return entities; }

	Scene* scene() { return m_scene; }

	virtual void onEntityAdded(Entity) { }
	virtual void onEntityRemoved(Entity) { }

private:
	void addEntity(Entity e)
	{
		std::cout << "On system " << type.name() << ": entity(" << e.name() << ") added\n";
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
	MessageBus* messageBus = nullptr;
	const std::type_index type;
	friend class SystemManager;
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
			std::cout << " System " << sys->type.name() << " dosen't have any component requirements\n\n";

		return dynamic_cast<T*>(sys.get());
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
		if (auto system = getSystem<T>(); system) {
			Util::remove_if(systems, [system](auto& sys) {
				return sys.get() == system;
			});
		}
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
		else if (!activeSystem && active)
			activeSystems.push_back(system);
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

private:
	std::vector<std::unique_ptr<System>> systems;
	std::vector<System*> activeSystems;
	MessageBus& messageBus;
	Scene& scene;
	ComponentManager& componentManager;
};
