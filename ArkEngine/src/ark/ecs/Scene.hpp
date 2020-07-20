#pragma once

#include "Component.hpp"
#include "Script.hpp"
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "Director.hpp"

#include <SFML/Graphics/Drawable.hpp>
#include <memory>

// TODO (Scene): add Directors?

namespace ark {

	class MessageBus;

	class Scene final : public NonCopyable, public NonMovable {

	public:
		Scene(MessageBus& bus)
			: componentManager(),
			scriptManager(),
			entityManager(componentManager, scriptManager),
			systemManager(bus, *this, componentManager),
			mMessageBus(bus)
		{}

		~Scene() = default;

		Entity createEntity(std::string name = "")
		{
			createdEntities.push_back(entityManager.createEntity(std::move(name)));
			return createdEntities.back();
		}

		Entity loadEntity(std::string name)
		{
			Entity e = createEntity(name);
			entityManager.deserializeEntity(e);
			return e;
		}

		void createEntities(std::vector<Entity>& entities, int n)
		{
			for (int i = 0; i < n; i++)
				entities.push_back(createEntity());
		}

		Entity cloneEntity(Entity e, std::string name = "")
		{
			createdEntities.push_back(entityManager.cloneEntity(e, name));
			return createdEntities.back();
		}

		void serializeEntity(Entity e)
		{
			entityManager.serializeEntity(e);
		}

		void destroyEntity(Entity entity)
		{
			destroyedEntities.push_back(entity);
		}

		Entity getEntityByName(std::string name)
		{
			return entityManager.getEntityByName(name);
		}


		template <typename T, typename...Args>
		T* addSystem(Args&&... args)
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			auto system = systemManager.addSystem<T>(std::forward<T>(args)...);
			if constexpr (std::is_base_of_v<Renderer, T>)
				renderers.push_back(system);
			return system;
		}

		template <typename T>
		T* getSystem()
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			return systemManager.getSystem<T>();
		}

		const auto& getSystems()
		{
			return systemManager.getSystems();
		}

		template <typename T>
		void setSystemActive(bool active)
		{
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
		void removeSystem()
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			if (std::is_base_of_v<Renderer, T>)
				Util::erase(renderers, systemManager.getSystem<T>());
			systemManager.removeSystem<T>();
		}

		template <typename T, typename...Args>
		T* addDirector(Args&&... args)
		{
			static_assert(std::is_base_of_v<Director, T>, " T not a system type");
			Director* director = mDirectors.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)).get();
			director->mScene = this;
			director->mType = typeid(T);
			director->mMessageBus = &mMessageBus;
			director->init();
			if constexpr (std::is_base_of_v<Renderer, T>)
				renderers.push_back(director);
			return static_cast<T*>(director);
		}

		template <typename T>
		T* getDirector()
		{
			static_assert(std::is_base_of_v<Director, T>, " T not a system type");
			for (auto& dir : mDirectors)
				if (dir->mType == typeid(T))
					return dir.get();
			return nullptr;
		}

		std::type_index componentTypeFromId(int id) const
		{
			return componentManager.typeFromId(id);
		}

		const auto& getComponentTypes() const
		{
			return componentManager.getTypes();
		}

		void handleEvent(const sf::Event& event)
		{
			systemManager.forEachSystem([&event](System* system){
				system->handleEvent(event);
			});
			scriptManager.forEachScript([&event](Script* script) {
				script->handleEvent(event);
			});
			for (auto& dir : mDirectors)
				dir->handleEvent(event);
		}

		void handleMessage(const Message& message)
		{
			systemManager.forEachSystem([&message](System* system) {
				system->handleMessage(message);
			});
			for (auto& dir : mDirectors)
				dir->handleMessage(message);
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
			for (auto& dir : mDirectors)
				dir->update();
		}

		void preRender(sf::RenderTarget& target)
		{
			for (auto renderer : renderers)
				renderer->preRender(target);
		}
		void render(sf::RenderTarget& target)
		{
			for (auto renderer : renderers)
				renderer->render(target);
		}
		void postRender(sf::RenderTarget& target)
		{
			for (auto renderer : renderers)
				renderer->postRender(target);
		}

		void processPendingData()
		{
			for (auto entity : createdEntities)
				systemManager.addToSystems(entity);

			if (auto modifiedEntities = entityManager.getModifiedEntities(); modifiedEntities.has_value()) {
				// modified entities that have not been created now (entites created this frame are also marked as 'modified')
				std::vector<Entity> me = Util::set_difference(modifiedEntities.value(), createdEntities);
				for (auto entity : me) {
					EngineLog(LogSource::EntityM, LogLevel::Info, "modified (%s)", entity.getName().c_str());
					systemManager.removeFromSystems(entity);
					systemManager.addToSystems(entity);
				}
			}
			createdEntities.clear();

			scriptManager.processPendingScripts();

			for (auto entity : destroyedEntities) {
				systemManager.removeFromSystems(entity);
				entityManager.destroyEntity(entity);
			}
			destroyedEntities.clear();
		}

	private:
		friend class EntityManager;
		friend class SceneInspector;
		friend class Director;
		ComponentManager componentManager;
		ScriptManager scriptManager;
		EntityManager entityManager;
		SystemManager systemManager;
		MessageBus& mMessageBus;

		std::vector<std::unique_ptr<Director>> mDirectors;
		std::vector<Entity> createdEntities;
		std::vector<Entity> destroyedEntities;
		std::vector<Renderer*> renderers;
	};

	template <typename F>
	void Director::forEachEntity(F&& f)
	{
		for (auto& entityData : mScene->entityManager.entities) {
			f(entityData);
		}
	}
}
