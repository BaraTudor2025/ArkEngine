#pragma once

#include "Component.hpp"
#include "Script.hpp"
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"

#include <SFML/Graphics/Drawable.hpp>

// TODO Scene: add Directors?
// TODO Scene: inherit from sf::Drawable? and add vector<Drawables*> dSystems; asta cand scot 'void render()' din systeme
// TODO Scene: add MessageBus
// TODO Scene: add class RenderSystem{ virtual void render(sf::RenderTarget&) = 0; }

class Scene {

public:
	Scene()
		: componentManager(),
		scriptManager(),
		entityManager(componentManager, scriptManager),
		systemManager(componentManager, *this)
	{}

	~Scene() = default;

	Entity createEntity(std::string name = "") {
		pendingEntities.push_back(entityManager.createEntity(std::move(name)));
		return pendingEntities.back();
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
		return systemManager.addSystem<T>(std::forward<T>(args)...);
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
	}

	template <typename T>
	void removeSystem() {
		static_assert(std::is_base_of_v<System, T>, " T not a system type");
		systemManager.removeSystem<T>();
	}

	//void forwardMessage(const Message& message);
	void forwardEvent(const sf::Event& event)
	{
		systemManager.forEachSystem([&event](System* system){
			system->handleEvent(event);
		});
		scriptManager.forEachScript([&event](Script* script) {
			script->handleEvent(event);
		});
	}

	void updateSystems()
	{
		systemManager.forEachSystem([](System* system) {
			system->update();
		});
	}

	void updateScripts()
	{
		scriptManager.forEachScript([](Script* script) {
			script->update();
		});
	}

	void fixedUpdateSystems()
	{
		systemManager.forEachSystem([](System* system) {
			system->fixedUpdate();
		});
	}	

	void fixedUpdateScripts()
	{
		scriptManager.forEachScript([](Script* script) {
			script->fixedUpdate();
		});
	}
	
	void processPendingData()
	{
		for (auto entity : pendingEntities)
			systemManager.addToSystems(entity);
		pendingEntities.clear();

		scriptManager.processPendingScripts();

		for (auto entity : destroyedEntities) {
			systemManager.removeFromSystems(entity);
			entityManager.destroyEntity(entity);
		}
		destroyedEntities.clear();
	}

protected:
	virtual void init() = 0;

private:
	void render(sf::RenderTarget& target)
	{
		systemManager.forEachSystem([&target](System* system){
			system->render(target);
		});
	}


private:
	ComponentManager componentManager;
	ScriptManager scriptManager;
	EntityManager entityManager;
	SystemManager systemManager;

	std::vector<Entity> pendingEntities;
	std::vector<Entity> destroyedEntities;
	friend class ArkEngine;
};
