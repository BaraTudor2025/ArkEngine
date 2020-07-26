#pragma once

#include <vector>

#include <SFML/Window/Event.hpp>

#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/core/Message.hpp"
#include "ark/core/MessageBus.hpp"

namespace ark {

	class Scene;

	class ARK_ENGINE_API System : public NonCopyable {

	public:
		System(std::type_index type) : type(type), name(Util::getNameOfType(type)) {}
		virtual ~System() = default;

		virtual void init() {}
		virtual void update() {}
		virtual void handleEvent(sf::Event) {}
		virtual void handleMessage(const Message&) {}

		bool isActive() { return active; }

		const std::string_view name;
		const std::vector<Entity>& getEntities() const { return entities; }
		const std::vector<std::string_view>& getComponentNames() const { return componentNames; }

	protected:
		template <typename T>
		void requireComponent()
		{
			static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
			componentTypes.push_back(typeid(T));
			componentManager->addComponentType<T>();
		}

		template <typename T>
		T* postMessage(int id)
		{
			return messageBus->post<T>(id);
		}

		std::vector<Entity>& getEntities() { return entities; }

		Scene* scene() { return m_scene; }

		virtual void onEntityAdded(Entity) {}
		virtual void onEntityRemoved(Entity) {}

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
				componentNames.push_back(Util::getNameOfType(type));

			for (auto type : componentTypes)
				componentMask.set(cm.idFromType(type));
			componentTypes.clear();
		}

	private:
		friend class SystemManager;
		ComponentManager* componentManager = nullptr;
		std::vector<Entity> entities;
		std::vector<std::type_index> componentTypes;
		ComponentManager::ComponentMask componentMask;
		Scene* m_scene = nullptr;
		MessageBus* messageBus = nullptr;
		std::type_index type;
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
		SystemManager(MessageBus& bus, Scene& scene, ComponentManager& compMgr) : messageBus(bus), scene(scene), componentManager(compMgr) {}
		~SystemManager() = default;

		template <typename T, typename...Args>
		T* addSystem(Args&&... args)
		{
			if (hasSystem<T>())
				return getSystem<T>();

			auto& system = systems.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			activeSystems.push_back(system.get());
			system->m_scene = &scene;
			system->componentManager = &componentManager;
			system->messageBus = &messageBus;
			system->init();
			system->constructMask(componentManager);
			if (system->componentMask.none())
				EngineLog(LogSource::SystemM, LogLevel::Warning, "(%s) dosen't have any component requirements", Util::getNameOfType(system->type));

			return dynamic_cast<T*>(system.get());
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
				if (system->componentMask.any())
					if ((entityMask & system->componentMask) == system->componentMask)
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
}
