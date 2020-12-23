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
#include "ark/ecs/Director.hpp"
#include "ark/ecs/Scene.hpp"

namespace ark
{
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
		auto getComponentNames() const -> const std::vector<std::string_view>& { return componentNames; }

	protected:

		template <typename T>
		void requireComponent()
		{
			static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
			componentTypes.push_back(typeid(T));
			mRegisry->addComponentType(typeid(T));
		}

		template <typename T>
		T* postMessage(int id)
		{
			return messageBus->post<T>(id);
		}

		Registry* registry() { return mRegisry; }

		virtual void onEntityAdded(Entity) {}
		virtual void onEntityRemoved(Entity) {}

	private:
		void constructQuerry()
		{
			for (auto type : componentTypes)
				componentNames.push_back(meta::getMetadata(type)->name);
			querry = mRegisry->makeQuerry(componentTypes);
			querry.onEntityAdd([this](Entity e) mutable {
				this->onEntityAdded(e);
			});
			querry.onEntityRemove([this](Entity e) mutable {
				this->onEntityRemoved(e);
			});

			//for (auto type : componentTypes)
			//	componentMask.set(cm.idFromType(type));
			componentTypes.clear();
		}

		friend class SystemManager;
		EntityQuerry querry;
		// TODO: remove
		std::vector<std::type_index> componentTypes; // folosit temporar doar pentru construcita lui querry
		Registry* mRegisry = nullptr;
		MessageBus* messageBus = nullptr;
		// TODO: remove
		// used for inspection
		std::vector<std::string_view> componentNames;
		bool active = true;
	};

	template <typename T>
	class SystemT : public System {
	public:
		SystemT() : System(typeid(T)) {}
	};


	/* NOTE: recomand declararea managerului dupa a registrului/entity-manager, ca sa poata fi distrul dupa el */
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
			system->init();
			system->constructQuerry();

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

		template <typename T, typename...Args>
		requires std::derived_from<T, Director>
		T* addDirector(Args&&... args)
		{
			static_assert(std::is_base_of_v<Director, T>, " T not a system type");
			//System* system = systems.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)).get();
			Director* director = mDirectors.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)).get();
			director->mRegistry = &this->registry;
			director->mType = typeid(T);
			director->mMessageBus = &this->messageBus;
			director->mSystemManager = this;
			director->init();
			if constexpr (std::is_base_of_v<Renderer, T>)
				renderers.push_back(dynamic_cast<Renderer*>(director));
			return static_cast<T*>(director);
		}

		template <typename T>
		requires std::derived_from<T, Director>
		auto getDirector() -> T*
		{
			static_assert(std::is_base_of_v<Director, T>, " T not a director type");
			for (auto& dir : mDirectors)
				if (dir->mType == typeid(T))
					return static_cast<T*>(dir.get());
			return nullptr;
		} 

		void handleEvent(const sf::Event& event)
		{
			forEachSystem([&event](System* system){
				system->handleEvent(event);
			});
			for (auto& dir : mDirectors)
				dir->handleEvent(event);
		}

		void handleMessage(const Message& message)
		{
			forEachSystem([&message](System* system) {
				system->handleMessage(message);
			});
			for (auto& dir : mDirectors)
				dir->handleMessage(message);
		}

		void update() 
		{
			forEachSystem([](System* system) {
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

		//void setSystemActive(std::type_index type, bool active)
		//{
		//	System* system = getSystem(type);
		//	if (!system)
		//		return;

		//	auto isCurrentlyActive = Util::contains(activeSystems, system);

		//	if (isCurrentlyActive && !active) {
		//		Util::erase(activeSystems, system);
		//		system->active = false;
		//	}
		//	else if (!isCurrentlyActive && active) {
		//		activeSystems.push_back(system);
		//		system->active = true;
		//	}

			//if (active) {
			//	if (!Util::contains(renderers, system))
			//		renderers.push_back(dynamic_cast<Renderer*>(system));
			//} else {
			//	Util::erase(renderers, system);
			//}
		//}

	private:
		std::vector<std::unique_ptr<Director>> mDirectors;
		std::vector<std::unique_ptr<System>> systems;
		std::vector<Renderer*> renderers;
		std::vector<System*> activeSystems;
		MessageBus& messageBus;
		Registry& registry;
	};
}
