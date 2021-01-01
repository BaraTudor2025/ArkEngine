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
			entityManager(componentManager, mComponentsAdded, upstreamComponent)
		{}
		~Registry() = default;

		void addComponentType(std::type_index type) {
			componentManager.addComponentType(type);
		}

		int idFromType(std::type_index type) const {
			return componentManager.idFromType(type);
		}

		auto typeFromId(int id) -> std::type_index const {
			return componentManager.typeFromId(id);
		}

		[[nodiscard]]
		auto createEntity() -> Entity {
			return entityManager.createEntity();
		}
		
		auto entityFromId(int id) -> Entity
		{
			Entity e;
			e.id = id;
			e.manager = &entityManager;
			return e;
		}

		[[nodiscard]]
		auto cloneEntity(Entity e) -> Entity {
			return entityManager.cloneEntity(e);
		}

		template <typename TComponent, typename F>
		void onConstruction(F&& f) {
			entityManager.addOnConstruction<TComponent>(std::forward<F>(f));
		}

		void safeRemoveComponent(Entity entity, std::type_index type) 
		{
			auto compId = componentManager.idFromType(type);
			auto index = Util::get_index_if(mComponentsRemoved, [id = entity.getID()](const auto& pair) { return pair.first == id; });
			if (index == ArkInvalidIndex) {
				auto& [id, mask] = mComponentsRemoved.emplace_back();
				id = entity.getID();
				mask.set(compId);
			}
			else
				mComponentsRemoved[index].second.set(compId);
		}

		void destroyEntity(Entity entity)
		{
			mDestroyedEntities.push_back(entity);
			auto index = Util::get_index_if(mComponentsRemoved, [id = entity.getID()](const auto& pair) { return pair.first == id; });
			if (index == ArkInvalidIndex)
				mComponentsRemoved.push_back({ entity.getID(), entity.getComponentMask() });
			else
				mComponentsRemoved[index].second |= entity.getComponentMask();
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

		template <typename... Args> [[nodiscard]]
		auto makeQuerry() -> EntityQuerry  {
			return makeQuerry({ typeid(Args)... });
		}

		[[nodiscard]]
		auto makeQuerry(const std::vector<std::type_index>& types) -> EntityQuerry 
		{
			auto mask = ComponentManager::ComponentMask();
			for (auto type : types) {
				componentManager.addComponentType(type);
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
			// search for existing querry; remove expired querries 
			for (auto& querry : querries) {
				if (querry.expired()) {
					cleanup += 1;
					pos.push_back(index);
				}
				else if (auto data = querry.lock(); data->componentMask == mask) {
					return EntityQuerry{std::move(data), this};
				}
				index++;
			}
			for (int i = 0; i < cleanup; i++) {
				Util::erase_if(querries, [](auto& q) { return q.expired(); });
				Util::erase(querryMasks, querryMasks[pos[i]]);
			}
			// create new querry if one does not exist
			auto querry = EntityQuerry(std::make_shared<EntityQuerry::SharedData>(), this);
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
		const auto& getComponentTypes() const {
			return componentManager.getTypes();
		}

		// update querries, remove components, destroy entities
		void update()
		{
			// update with added components
			for (auto [entityId, amask] : mComponentsAdded) {
				Entity entity = entityFromId(entityId);
				forQuerries(entity.getComponentMask(), amask, [this, entity](auto* data) mutable { 
					data->entities.push_back(entity);
					for (auto& f : data->addEntityCallbacks) {
						f(entity);
					}
				});
			}
			// update with removed components/destroyed entities
			for (auto [entityId, rmask] : mComponentsRemoved) {
				Entity entity = entityFromId(entityId);
				forQuerries(entity.getComponentMask(), rmask, [this, entity](auto* data) mutable { 
					Util::erase_if(data->entities, [&](Entity e) {
						if (entity == e) {
							for (auto& f : data->removeEntityCallbacks)
								f(entity);
							return true;
						} else 
							return false;
					});
				});
			}
			// remove components and update bitmask
			for (auto [entityId, mask] : mComponentsRemoved) {
				Entity entity = entityFromId(entityId);
				if (entity.isValid())
					for (int i = 0; i < mask.size(); i++)
						if (mask[i])
							entityManager.removeComponentOfEntity(entity, componentManager.typeFromId(i));
			}
			for (Entity entity : mDestroyedEntities)
				entityManager.destroyEntity(entity);
			mDestroyedEntities.clear();
			mComponentsAdded.clear();
			mComponentsRemoved.clear();
		}

	private:

		template <typename F>
		void forQuerries(ComponentManager::ComponentMask entityMask, ComponentManager::ComponentMask modifiedMask, F f) {
			int index = 0;
			for (auto qmask : querryMasks) {
				// 1.check: querry-ul are cel putin o componenta added/removed
				// 2.check: entity poate fi bagat in querry
				// pentru add: facem 1.check pentru a sari peste querri-urile care au deja entity-ul
				// pentru remove: entityMask inca nu este updatat (vezi update de mai sus) deci 2.check este irelevant
				if (/*1*/(modifiedMask & qmask).any() && /*2*/(entityMask & qmask) == qmask)
					if (auto data = querries[index].lock(); data)
						f(data.get());
				index++;
			}
		}

		ComponentManager componentManager;
		EntityManager entityManager;

		std::vector<ComponentManager::ComponentMask> querryMasks; // cache, ca sa nu dau .lock() la shared-ptr pentru ->mask
		std::vector<std::weak_ptr<EntityQuerry::SharedData>> querries;
		std::vector<Entity> mDestroyedEntities;
		// momentan, mComponentsAdded este folosit de EntityManager si mRemoved de Registry print safeRemove
		// on call of Add/RemoveComponent, add or update mask with id of component added or removed
		std::vector<std::pair<Entity::ID, ComponentManager::ComponentMask>> mComponentsAdded;
		std::vector<std::pair<Entity::ID, ComponentManager::ComponentMask>> mComponentsRemoved;

	}; // class Registry

	template <typename F> 
	requires std::invocable<F, const ark::meta::Metadata&>
	void EntityQuerry::forComponents(F f) const {
		if (!data)
			return;
		for (int i = 0; i < data->componentMask.size(); i++)
			if (data->componentMask[i])
				f(*ark::meta::getMetadata(mRegistry->typeFromId(i)));
	}
}
