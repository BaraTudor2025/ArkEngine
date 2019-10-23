#pragma once

#include "Component.hpp"
#include "Script.hpp"
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "ManagerInspector.hpp"

#include <SFML/Graphics/Drawable.hpp>

// TODO (Scene): add Directors?

class MessageBus;

class Scene final : public NonCopyable, public NonMovable {

public:
	Scene(MessageBus& bus)
		: componentManager(),
		scriptManager(),
		entityManager(componentManager, scriptManager),
		systemManager(bus, *this, componentManager)
	{}

	~Scene() = default;

	Entity createEntity(std::string name = "") {
		createdEntities.push_back(entityManager.createEntity(std::move(name)));
		return createdEntities.back();
	}

	void createEntities(std::vector<Entity>& entities, int n) {
		for (int i = 0; i < n; i++)
			entities.push_back(createEntity());
	}

	void destroyEntity(Entity entity) {
		destroyedEntities.push_back(entity);
	}

	Entity getEntityByName(std::string name) {
		return entityManager.getEntityByName(name);
	}

	template <typename T, typename...Args>
	T* addSystem(Args&&... args) {
		static_assert(std::is_base_of_v<System, T>, " T not a system type");
		auto system = systemManager.addSystem<T>(std::forward<T>(args)...);
		if (std::is_base_of_v<Renderer, T>)
			renderers.push_back(system);
		return system;
	}

	template <typename T>
	T* getSystem() {
		static_assert(std::is_base_of_v<System, T>, " T not a system type");
		return systemManager.getSystem<T>();
	}

	template <typename T>
	void setSystemActive(bool active) {
		static_assert(std::is_base_of_v<System, T>, " T not a system type");
		systemManager.setSystemActive<T>(active);

		if (std::is_base_of_v<Renderer, T>) {
			auto system = systemManager.getSystem<T>();
			if (active) {
				auto found = Util::find(renderers, system);
				if (!found)
					renderers.push_back(system);
			} else {
				Util::erase(renderers, system);
			}
		}
	}

	template <typename T>
	void removeSystem() {
		static_assert(std::is_base_of_v<System, T>, " T not a system type");
		if (std::is_base_of_v<Renderer, T>)
			Util::erase(renderers, systemManager.getSystem<T>());
		systemManager.removeSystem<T>();
	}

	void forwardEvent(const sf::Event& event)
	{
		systemManager.forEachSystem([&event](System* system){
			system->handleEvent(event);
		});
		scriptManager.forEachScript([&event](Script* script) {
			script->handleEvent(event);
		});
	}

	void forwardMessage(const Message& message)
	{
		systemManager.forEachSystem([&message](System* system){
			system->handleMessage(message);
		});
	}

	void update()
	{
		processPendingData();
		systemManager.forEachSystem([](System* system) {
			system->update();
		});
		scriptManager.forEachScript([](Script* script) {
			script->update();
		});
	}
	
	void render(sf::RenderTarget& target)
	{
		for (auto system : renderers)
			system->render(target);
	}

private:

	template <typename T1, typename T2>
	static auto set_difference(const T1& range1, const T2& range2)
	{
		std::vector<T1::value_type> diff;
		std::set_difference(range1.begin(), range1.end(), range2.begin(), range2.end(), std::back_inserter(diff));
		return diff;
	}

	void processPendingData()
	{
		for (auto entity : createdEntities)
			systemManager.addToSystems(entity);
		createdEntities.clear();

		if (auto dirtyEntities = entityManager.getDirtyEntities(); dirtyEntities.has_value()) {
			std::vector<Entity> modifiedEntities; // entities that are modified and have not been created now
			//std::set_difference(createdEntities.begin(), createdEntities.);
			modifiedEntities = set_difference(createdEntities, dirtyEntities.value());
			for (auto entity : modifiedEntities) {
				systemManager.removeFromSystems(entity);
				systemManager.addToSystems(entity);
			}
		}

		scriptManager.processPendingScripts();

		for (auto entity : destroyedEntities) {
			systemManager.removeFromSystems(entity);
			entityManager.destroyEntity(entity);
		}
		destroyedEntities.clear();
	}

private:
	ComponentManager componentManager;
	ScriptManager scriptManager;
	EntityManager entityManager;
	SystemManager systemManager;

	std::vector<Entity> createdEntities;
	std::vector<Entity> destroyedEntities;
	std::vector<Renderer*> renderers;
};
