#pragma once

#include <vector>

#include <SFML/Window/Event.hpp>

#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/Querry.hpp"
#include "ark/core/Message.hpp"
#include "ark/core/MessageBus.hpp"

namespace ark
{
	class Registry;

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
			componentManager->addComponentType(typeid(T));
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
		void constructQuerry();

		friend class SystemManager;
		ComponentManager* componentManager = nullptr;
		EntityQuerry querry;
		std::vector<std::type_index> componentTypes; // folosit temporar doar pentru construcita lui querry
		Registry* mRegisry = nullptr;
		MessageBus* messageBus = nullptr;
		// used for inspection
		std::vector<std::string_view> componentNames;
		bool active = true;
	};

	template <typename T>
	class SystemT : public System {
	public:
		SystemT() : System(typeid(T)) {}
	};

	class SystemManager {

	public:
		SystemManager(MessageBus& bus, Registry& registry, ComponentManager& compMgr) : messageBus(bus), registry(registry), componentManager(compMgr) {}
		~SystemManager() = default;

		template <typename T, typename...Args>
		T* addSystem(Args&&... args)
		{
			if (hasSystem<T>())
				return getSystem<T>();

			System* system = systems.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)).get();
			activeSystems.push_back(system);
			system->mRegisry = &registry;
			system->componentManager = &componentManager;
			system->messageBus = &messageBus;
			system->init();
			system->constructQuerry();
			return dynamic_cast<T*>(system);
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

		const auto& getSystems() const
		{
			return systems;
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
				Util::erase_if(systems, [system](auto& sys) {
					return sys.get() == system;
				});
			}
		}

		// used by Registry to call handleEvent, handleMessage, update
		template <typename F>
		void forEachSystem(F f)
		{
			for (auto system : activeSystems)
				f(system);
		}

		void setSystemActive(std::type_index type, bool active)
		{
			System* system = getSystem(type);
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
		}

	private:
		std::vector<std::unique_ptr<System>> systems;
		std::vector<System*> activeSystems;
		MessageBus& messageBus;
		Registry& registry;
		ComponentManager& componentManager;
	};
}
