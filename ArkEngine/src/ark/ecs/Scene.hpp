#pragma once

#include "Component.hpp"
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "Director.hpp"
#include "Renderer.hpp"
#include "Querry.hpp"

#include <SFML/Graphics/Drawable.hpp>
#include <memory>


namespace ark {

	class MessageBus;
	class EntitiesView;

	class Registry final : public NonCopyable, public NonMovable {

	public:
		Registry(MessageBus& bus, std::pmr::memory_resource* upstreamComponent = std::pmr::new_delete_resource())
			: componentManager(),
			entityManager(componentManager, upstreamComponent),
			systemManager(bus, *this, componentManager),
			mMessageBus(bus)
		{}

		~Registry() = default;

		[[nodiscard]]
		auto createEntity() -> Entity
		{
			createdEntities.push_back(entityManager.createEntity());
			return createdEntities.back();
		}
		
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

		// is removed in Registry::update()
		void safeRemoveComponent(Entity e, std::type_index type) {
			// TODO (querry): adauga optimizare pentru querry remove, poate un vector<bitmask> ca parcurgerea sa fie liniara
			this->mComponentsToRemove.push_back({ e, type });
			//addDataRemoved(e, type);
			//this->mEntitiesModifiedRemove.push_back({});
		}

		// is destroyed in Registry::update()
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

		[[nodiscard]]
		auto makeQuerry(const std::vector<std::type_index>& types) -> EntityQuerry 
		{
			auto mask = ComponentManager::ComponentMask();
			for (auto type : types) {
				mask.set(componentManager.idFromType(type));
			}
			return makeQuerry(mask);
		}

		[[nodiscard]]
		auto makeQuerry(ComponentManager::ComponentMask mask) -> EntityQuerry 
		{
			assert(("empty component mask", !mask.none()));
			int cleanup = 0;
			auto pos = std::vector<int>();
			int index = 0;
			// search for existing/cached querry
			for (auto& querry : querries) {
				if (querry.expired()) {
					cleanup += 1;
					pos.push_back(index);
				}
				else if (auto data = querry.lock(); data->componentMask == mask) {
					return EntityQuerry{std::move(data)};
				}
				index++;
			}
			for (int i = 0; i < cleanup; i++) {
				Util::erase_if(querries, [](auto& q) { return q.expired(); });
				Util::erase(querryMasks, querryMasks[pos[i]]);
			}

			// if no querry already exists then make a new one
			auto querry = EntityQuerry(std::make_shared<EntityQuerry::SharedData>());
			querry.data->componentMask = mask;
			for (auto entity : entitiesView()) {
				if ((entity.getComponentMask() & mask) == mask) {
					querry.data->entities.push_back({ entity.id, &this->entityManager });
				}
			}
			this->querries.push_back(querry.data);
			this->querryMasks.push_back(querry.data->componentMask);
			return querry;
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
			director->mRegistry = this;
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

		// removal of components, destruction of entities and updation of querries
		// is delayed by 1 frame
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
			for (Entity entity : destroyedEntities) {
				auto entityMask = entity.getComponentMask();
				if (entityMask.none())
					return;
				forQuerries(entity, entityMask, 
					[this, entity](auto* data) mutable { this->removeFromQuerry(data, entity); });
				entityManager.destroyEntity(entity);
			}
			
			for (auto& [e, type] : mComponentsToRemove)
				if (e.isValid())
					e.removeComponent(type);

			for (auto entity : createdEntities)
				forQuerries(entity, entity.getComponentMask(), 
					[this, entity](auto* data) mutable { this->addToQuerry(data, entity); });

			// modified == entity had added or removed components
			if (auto modifiedEntities = entityManager.getModifiedEntities(); modifiedEntities.has_value()) {
				// modified entities that have not been created this frame
				std::vector<Entity> ent_mod = Util::set_difference(modifiedEntities.value(), createdEntities);
				for (Entity entity : ent_mod)
					processModifiedEntityToQuerries(entity);
			}
			createdEntities.clear();
			destroyedEntities.clear();
			mComponentsToRemove.clear();
		}

	private:

		//void addDataRemoved(Entity e, std::type_index type) {
		//	for (ModifiedData& mod : mEntitiesModifiedRemove) {
		//		if (mod.id == e.getID()) {
		//			mod.mask.set(componentManager.idFromType(type));
		//			return;
		//		}
		//	}
		//	auto mask = ComponentManager::ComponentMask();
		//	mask.set(componentManager.idFromType(type));
		//	mEntitiesModifiedRemove.push_back({ e.getID(), mask });
		//}

		// TODO (querry): optimizat mizeria asta
		void processModifiedEntityToQuerries(Entity entity)
		{
			auto entityMask = entity.getComponentMask();

			int index = 0;
			for (auto qmask : querryMasks) {
				auto data = querries[index].lock();
				if ((entityMask & qmask) == qmask) {
					if (data) {
						// adauga daca nu este
						bool found = false;
						for (Entity e : data->entities)
							if (e == entity)
								found = true;
						if (!found) {
							addToQuerry(data.get(), entity);
						}
					}
				}
				else {
					removeFromQuerry(data.get(), entity);
				}
				index++;
			}
		}

		void addToQuerry(EntityQuerry::SharedData* data, Entity entity) {
			data->entities.push_back(entity);
			for (auto& f : data->addEntityCallbacks) {
				f(entity);
			}
		}

		void removeFromQuerry(EntityQuerry::SharedData* data, Entity entity) {
			Util::erase_if(data->entities, [&](Entity e) {
				if (entity == e) {
					for (auto& f : data->removeEntityCallbacks)
						f(entity);
					return true;
				}
				return false;
			});
		}

		template <typename F>
		void forQuerries(Entity entity, ComponentManager::ComponentMask entityMask, F f) {
			int index = 0;
			for (auto qmask : querryMasks) {
				if ((entityMask & qmask) == qmask) {
					auto data = querries[index].lock();
					if (data)
						f(data.get());
				}
				index++;
			}
		}

		friend class EntitiesView;
		SystemManager systemManager;
		ComponentManager componentManager;
		EntityManager entityManager;
		MessageBus& mMessageBus;

		std::vector<ComponentManager::ComponentMask> querryMasks;
		std::vector<std::weak_ptr<EntityQuerry::SharedData>> querries;
		std::vector<std::pair<Entity, std::type_index>> mComponentsToRemove;
		std::vector<std::unique_ptr<Director>> mDirectors;
		std::vector<Entity> createdEntities;
		std::vector<Entity> destroyedEntities;
		std::vector<Renderer*> renderers;
		//struct ModifiedData {
		//	Entity::ID id;
		//	ComponentManager::ComponentMask mask;
		//};
		//std::vector<ModifiedData> mEntitiesModifiedAdd;
		//std::vector<ModifiedData> mEntitiesModifiedRemove;

	}; // class Registry

	inline void System::constructQuerry()
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

}
