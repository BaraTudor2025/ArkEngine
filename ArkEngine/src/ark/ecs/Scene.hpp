#pragma once

#include "Component.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"
#include "Querry.hpp"

#include <SFML/Graphics/Drawable.hpp>
#include <memory>

namespace ark {

	template <typename F, typename T>
	concept IsOnConstruction =
		std::invocable<F, T&>
		|| std::invocable<F, T&, Entity>
		|| std::invocable<F, T&, Entity, Registry&>;

	template <typename T, typename... Args>
	concept HasOnConstruction = requires(Args&&... args) {
		{ T::onConstruction(std::forward<Args>(args)...) } -> IsOnConstruction<T>;
	};

	// wrapper for EntityManager to make querries easier to manage
	class Registry final  {

	public:
		Registry(std::pmr::memory_resource* upstreamComponent = std::pmr::new_delete_resource())
			: entityManager(mComponentsAdded, upstreamComponent)
		{}
		Registry(Registry&&) = default;
		~Registry() = default;

		template <ConceptComponent T>
		void addDefaultComponent() {
			addDefaultComponent(typeid(T));
		}

		void addDefaultComponent(std::type_index type) {
			mDefaultComponentTypes.push_back(type);
			addComponentType(type);
		}

		template <ConceptComponent... Args>
		void addComponentType() {
			(addComponentType(typeid(Args)), ...);
		}

		void addComponentType(std::type_index type) {
			entityManager.addComponentType(type);
		}

		void addAllComponentTypesFromMetaGroup() {
			const auto& types = ark::meta::getTypeGroup(ARK_META_COMPONENT_GROUP);
			for (auto type : types)
				entityManager.addComponentType(type);
		}

		int idFromType(std::type_index type) const {
			return entityManager.idFromType(type);
		}

		auto typeFromId(int id) -> std::type_index const {
			return entityManager.typeFromId(id);
		}

		[[nodiscard]]
		auto createEntity() -> Entity {
			Entity entity = entityManager.createEntity();
			for (std::type_index type : mDefaultComponentTypes)
				entity.addComponent(type);
			return entity;
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

		template <ConceptComponent T, typename F>
		requires IsOnConstruction<F, T>
		void onConstruction(F&& f) {
			entityManager.addOnConstruction<T>(std::forward<F>(f), this);
		}

		template <ConceptComponent T, typename... Args>
		requires HasOnConstruction<T, Args...>
		void onConstruction(Args&&... args) {
			entityManager.addOnConstruction<T>(T::onConstruction(std::forward<Args>(args)...), this);
		}

		template <ConceptComponent T, typename F>
		requires std::invocable<F, T&, const T&> 
		void onCopy(F&& f) {
			entityManager.addOnCopy<T>(std::forward<F>(f));
		}

		void safeRemoveComponent(Entity entity, std::type_index type) 
		{
			auto compId = entityManager.idFromType(type);
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
		requires std::invocable<F, Entity>
		void forEachEntity(F&& f)
		{
			for (Entity entity : entityManager.entitiesView())
				f(entity);
		}

		auto entitiesView() -> EntitiesView {
			return this->entityManager.entitiesView();
		}

		// get or create new querry and populate it if it's created;
		// querries can be updated trough Registry::update() with the Entities that have been modified since the querry creation
		// hence when a componet is added/removed it is not added to the querries automatically
		template <ConceptComponent... Args> [[nodiscard]]
		auto makeQuerry() -> EntityQuerry  {
			return makeQuerry({ typeid(Args)... });
		}

		[[nodiscard]]
		auto makeQuerry(const std::vector<std::type_index>& types) -> EntityQuerry 
		{
			auto mask = ComponentMask();
			for (auto type : types) {
				entityManager.addComponentType(type);
				mask.set(entityManager.idFromType(type));
			}
			return makeQuerry(mask);
		}

		[[nodiscard]]
		auto makeQuerry(ComponentMask mask) -> EntityQuerry 
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
			// create new querry if one does not exist and populate it
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
			return entityManager.getTypes();
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
							entityManager.removeComponentOfEntity(entity, entityManager.typeFromId(i));

			}
			for (Entity entity : mDestroyedEntities)
				entityManager.destroyEntity(entity);
			mDestroyedEntities.clear();
			mComponentsAdded.clear();
			mComponentsRemoved.clear();
		}

	private:

		template <typename F>
		void forQuerries(ComponentMask entityMask, ComponentMask modifiedMask, F f) {
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

		EntityManager entityManager;

		std::vector<ComponentMask> querryMasks; // cache, ca sa nu dau .lock() la shared-ptr pentru ->mask
		std::vector<std::weak_ptr<EntityQuerry::SharedData>> querries;
		std::vector<Entity> mDestroyedEntities;
		std::vector<std::type_index> mDefaultComponentTypes;
		// momentan, mComponentsAdded este folosit de EntityManager si mRemoved de Registry print safeRemove
		// on call of Add/RemoveComponent, add or update mask with id of component added or removed
		std::vector<std::pair<Entity::ID, ComponentMask>> mComponentsAdded;
		std::vector<std::pair<Entity::ID, ComponentMask>> mComponentsRemoved;

	}; // class Registry

	template <typename F> 
	requires std::invocable<F, std::type_index>
	void EntityQuerry::forComponents(F f) const {
		if (!data)
			return;
		for (int i = 0; i < data->componentMask.size(); i++)
			if (data->componentMask[i])
				f(mRegistry->typeFromId(i));
	}
}
