#pragma once

#include "Component.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "Querry.hpp"

#include <SFML/Graphics/Drawable.hpp>
#include <memory>

namespace ark {

	// wrapper for EntityManager to make querries easier to manage
	class Registry final : public NonCopyable, public NonMovable {

	public:
		Registry(std::pmr::memory_resource* upstreamComponent = std::pmr::new_delete_resource())
			: componentManager(),
			entityManager(componentManager, upstreamComponent)
		{}

		void addComponentType(std::type_index type) {
			componentManager.addComponentType(type);
		}

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

		[[nodiscard]]
		const auto& getComponentTypes() const
		{
			return componentManager.getTypes();
		}

		// removal of components, destruction of entities and updation of querries
		// is delayed by 1 frame
		void update()
		{
			processPendingData();
		}

	private:

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

		ComponentManager componentManager;
		EntityManager entityManager;

		std::vector<ComponentManager::ComponentMask> querryMasks;
		std::vector<std::weak_ptr<EntityQuerry::SharedData>> querries;
		std::vector<std::pair<Entity, std::type_index>> mComponentsToRemove;
		std::vector<Entity> createdEntities;
		std::vector<Entity> destroyedEntities;
		//struct ModifiedData {
		//	Entity::ID id;
		//	ComponentManager::ComponentMask mask;
		//};
		//std::vector<ModifiedData> mEntitiesModifiedAdd;
		//std::vector<ModifiedData> mEntitiesModifiedRemove;

	}; // class Registry
}
