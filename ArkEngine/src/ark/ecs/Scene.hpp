#pragma once

#include "Component.hpp"
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "Director.hpp"
#include "Renderer.hpp"

#include <SFML/Graphics/Drawable.hpp>
#include <memory>


namespace ark {

	class MessageBus;
	class EntitiesView;

	class Scene final : public NonCopyable, public NonMovable {

	public:
		Scene(MessageBus& bus, std::pmr::memory_resource* upstreamComponent = std::pmr::new_delete_resource())
			: componentManager(),
			entityManager(componentManager, upstreamComponent),
			systemManager(bus, *this, componentManager),
			mMessageBus(bus)
		{}

		~Scene() = default;

		[[nodiscard]]
		auto createEntity() -> Entity
		{
			createdEntities.push_back(entityManager.createEntity());
			return createdEntities.back();
		}
		
		[[nodiscard]]
		auto entityFromId(int id) -> Entity
		{
			Entity e;
			e.id = id;
			e.manager = &entityManager;
			return e;
		}

		[[nodiscard]]
		auto cloneEntity(Entity e) -> Entity
		{
			createdEntities.push_back(entityManager.cloneEntity(e));
			return createdEntities.back();
		}

		template <typename TComponent, typename F>
		void onConstruction(F&& f)
		{
			entityManager.addOnConstruction<TComponent>(std::forward<F>(f));
		}

		void destroyEntity(Entity entity)
		{
			destroyedEntities.push_back(entity);
		}

		template <typename F>
		void forEachEntity(F&& f)
		{
			for (auto& entityData : entityManager.entities) {
				if (!entityData.isFree) {
					auto e = entityFromId(entityData.id);
					f(e);
				}
			}
		}

		auto entitiesView() -> EntitiesView {
			return this->entityManager.entitiesView();
		}

		template <typename T, typename...Args>
		auto addSystem(Args&&... args) -> T*
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			auto system = systemManager.addSystem<T>(std::forward<T>(args)...);
			if constexpr (std::is_base_of_v<Renderer, T>)
				renderers.push_back(system);
			return system;
		}

		template <typename T>
		[[nodiscard]]
		auto getSystem() -> T*
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			return systemManager.getSystem<T>();
		}

		[[nodiscard]]
		const auto& getSystems()
		{
			return systemManager.getSystems();
		}

		template <typename T>
		void setSystemActive(bool active)
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			systemManager.setSystemActive(typeid(T), active);

			if (std::is_base_of_v<Renderer, T>) {
				auto system = systemManager.getSystem<T>();
				if (active) {
					if (!Util::contains(renderers, system))
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
		auto addDirector(Args&&... args) -> T*
		{
			static_assert(std::is_base_of_v<Director, T>, " T not a system type");
			auto& director = mDirectors.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			director->mScene = this;
			director->mType = typeid(T);
			director->mMessageBus = &mMessageBus;
			director->init();
			if constexpr (std::is_base_of_v<Renderer, T>)
				renderers.push_back(dynamic_cast<Renderer*>(director.get()));
			return static_cast<T*>(director.get());
		}

		template <typename T>
		[[nodiscard]]
		auto getDirector() -> T*
		{
			static_assert(std::is_base_of_v<Director, T>, " T not a director type");
			for (auto& dir : mDirectors)
				if (dir->mType == typeid(T))
					return static_cast<T*>(dir.get());
			return nullptr;
		}

		[[nodiscard]]
		const auto& getComponentTypes() const
		{
			return componentManager.getTypes();
		}

		void handleEvent(const sf::Event& event)
		{
			systemManager.forEachSystem([&event](System* system){
				system->handleEvent(event);
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

			for (auto entity : destroyedEntities) {
				systemManager.removeFromSystems(entity);
				entityManager.destroyEntity(entity);
			}

			if (auto modifiedEntities = entityManager.getModifiedEntities(); modifiedEntities.has_value()) {
				// modified entities that have not been created now (entites created this frame are also marked as 'modified')
				std::vector<Entity> me = Util::set_difference(modifiedEntities.value(), createdEntities);
				for (auto entity : me) {
					systemManager.processModifiedEntityToSystems(entity);
				}
			}
			createdEntities.clear();
			destroyedEntities.clear();
		}

	private:

		friend class EntitiesView;
		SystemManager systemManager;
		ComponentManager componentManager;
		EntityManager entityManager;
		MessageBus& mMessageBus;

		std::vector<std::unique_ptr<Director>> mDirectors;
		std::vector<Entity> createdEntities;
		std::vector<Entity> destroyedEntities;
		std::vector<Renderer*> renderers;

	}; // class Scene
}
