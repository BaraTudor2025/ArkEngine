#pragma once

#include <vector>
#include <concepts>

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include "ark/core/Message.hpp"
#include "ark/core/MessageBus.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/Querry.hpp"
#include "ark/ecs/Renderer.hpp"
#include "ark/ecs/Scene.hpp"

namespace ark
{
	class SystemManager;

	class ARK_ENGINE_API System : public NonCopyable {

	public:
		System(std::type_index type) : type(type), name(ark::meta::detail::prettifyTypeName(type.name())) {}
		virtual ~System() = default;

		virtual void init() {}
		virtual void update() = 0;
		virtual void handleEvent(sf::Event) {}
		virtual void handleMessage(const Message&) {}

		bool isActive() { return active; }

		const std::string name;
		const std::type_index type;
		auto getEntities() const -> const std::vector<Entity>& { return querry.getEntities(); }
		auto getQuerry() const -> const EntityQuerry& { return querry; }

	protected:

		EntityQuerry querry;

		template <typename T>
		T* postMessage(int id)
		{
			return messageBus->post<T>(id);
		}

		Registry& getEntityManager() const { return *mRegisry; }
		SystemManager& getSystemManager() const { return *mSystemManager; }

		__declspec(property(get=getEntityManager)) 
			Registry entityManager;

		__declspec(property(get=getSystemManager)) 
			SystemManager systemManager;

	private:
		friend class SystemManager;
		Registry* mRegisry = nullptr;
		MessageBus* messageBus = nullptr;
		SystemManager* mSystemManager = nullptr;
		bool active = true;
	};

	template <typename T>
	class SystemT : public System {
	public:
		SystemT() : System(typeid(T)) {}
	};


	/* NOTE: recomand declararea managerului dupa cea a registrului/entity-manager, ca sa poata fi distrus dupa el */
	class SystemManager {

	public:
		SystemManager(MessageBus& bus, Registry& registry) : messageBus(bus), registry(registry) {}
		~SystemManager() = default;

		template <typename T, typename...Args>
		requires std::derived_from<T, System>
		T* addSystem(Args&&... args)
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");

			if (hasSystem<T>())
				return getSystem<T>();

			System* system = systems.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)).get();
			activeSystems.push_back(system);
			system->mRegisry = &registry;
			system->messageBus = &messageBus;
			system->mSystemManager = this;
			system->init();

			if constexpr (std::is_base_of_v<Renderer, T>)
				renderers.push_back(dynamic_cast<T*>(system));
			return dynamic_cast<T*>(system);
		}

		template <typename T>
		requires std::derived_from<T, System>
		T* getSystem()
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
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

		const auto& getSystems() const
		{
			return systems;
		}

		template <typename T>
		requires std::derived_from<T, System>
		bool hasSystem()
		{
			return hasSystem(typeid(T));
		}

		bool hasSystem(std::type_index type)
		{
			return getSystem(type) != nullptr;
		}

		template <typename T>
		requires std::derived_from<T, System>
		void removeSystem()
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			if (std::is_base_of_v<Renderer, T>)
				Util::erase(renderers, getSystem<T>());
			if (auto system = getSystem<T>(); system) {
				Util::erase_if(systems, [system](auto& sys) {
					return sys.get() == system;
				});
			}
		}

		template <typename F>
		requires std::invocable<F, System*>
		void forEachSystem(F f)
		{
			for (auto system : activeSystems)
				f(system);
		}

		template <typename T>
		requires std::derived_from<T, System>
		void setSystemActive(bool active)
		{
			static_assert(std::is_base_of_v<System, T>, " T not a system type");
			System* system = getSystem(typeid(T));
			if (!system)
				return;

			auto isCurrentlyActive = Util::contains(activeSystems, system);

			if (isCurrentlyActive && !active) {
				Util::erase(activeSystems, system);
				system->active = false;
			}
			else if (!isCurrentlyActive && active) {
				activeSystems.push_back(system);
				system->active = true;
			}

			if (std::is_base_of_v<Renderer, T>) {
				System* system = getSystem<T>();
				if (active) {
					if (!Util::contains(renderers, system))
						renderers.push_back(system);
				} else {
					Util::erase(renderers, system);
				}
			}
		}

		void handleEvent(const sf::Event& event)
		{
			forEachSystem([&event](System* system){
				system->handleEvent(event);
			});
		}

		void handleMessage(const Message& message)
		{
			forEachSystem([&message](System* system) {
				system->handleMessage(message);
			});
		}

		void update() 
		{
			forEachSystem([](System* system) {
				system->update();
			});
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

	private:
		std::vector<std::unique_ptr<System>> systems;
		std::vector<Renderer*> renderers;
		std::vector<System*> activeSystems;
		MessageBus& messageBus;
		Registry& registry;
	};
}
