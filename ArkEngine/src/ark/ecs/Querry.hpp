#pragma once

#include "Component.hpp"
#include "Entity.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <concepts>

namespace ark {

	class Registry;

	class EntityQuerry {
	public:

		EntityQuerry() = default;

		bool isValid() const { return bool(data); }

		auto getEntities() const -> const std::vector<Entity>& { 
			if (data)
				return data->entities;
			else
				return {};
		}

		auto getMask() const -> ComponentMask { 
			if (data)
				return data->componentMask;
			else
				return {};
		}

		template <typename F> 
		requires std::invocable<F, std::type_index>
		void forComponentTypes(F f) const;

		template <ConceptComponent... Components, typename F>
		requires std::invocable<F, ark::Entity, Components&...>
			|| std::invocable<F, Components&...>
		void each(F f) const {
			if(data)
				for (Entity entity : data->entities) {
					if constexpr (std::invocable<F, Components&...>)
						f(entity.getComponent<Components>()...);
					else
						f(entity, entity.getComponent<Components>()...);
				}
		}

		template <typename F>
		requires std::invocable<F, Entity>
		void forEntities(F f) {
			if(data)
				for (Entity entity : data->entities)
					f(entity);
		}

		template <typename F>
		requires std::invocable<F, const Entity>
		void forEntities(F f) const {
			if(data)
				for (const Entity entity : data->entities)
					f(entity);
		}

		void onEntityAdd(std::function<void(Entity)> f) {
			data->addEntityCallbacks.emplace_back(std::move(f));
		}

		void onEntityRemove(std::function<void(Entity)> f) {
			data->removeEntityCallbacks.emplace_back(std::move(f));
		}

	private:
		friend class Registry;
		struct SharedData {
			ComponentMask componentMask;
			std::vector<Entity> entities;
			std::vector<std::function<void(Entity)>> addEntityCallbacks;
			std::vector<std::function<void(Entity)>> removeEntityCallbacks;

		};
		std::shared_ptr<SharedData> data;
		Registry* mRegistry = nullptr;
		EntityQuerry(std::shared_ptr<SharedData> data, Registry* reg) : data(std::move(data)), mRegistry(reg) {}
	};
}
