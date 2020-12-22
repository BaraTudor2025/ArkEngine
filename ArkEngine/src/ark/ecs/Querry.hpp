#pragma once

#include "Component.hpp"
#include "Entity.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <concepts>

namespace ark {

	// created with Registry::makeQuerry({typeid(Component)...});
	class EntityQuerry {
	public:

		EntityQuerry() = default;

		auto getEntities() const -> const std::vector<Entity>& { return data->entities; }

		void onEntityAdd(std::function<void(Entity)> f) {
			data->addEntityCallbacks.emplace_back(std::move(f));
		}

		void onEntityRemove(std::function<void(Entity)> f) {
			data->removeEntityCallbacks.emplace_back(std::move(f));
		}

		//auto getTypes() -> const std::vector<std::type_index>& { }

		template <typename F>
		requires std::invocable<F, Entity>
		void forEntities(F f) {
			for (Entity entity : data->entities) {
				f(entity);
			}
		}

	private:
		friend class Registry;
		struct SharedData {
			ComponentManager::ComponentMask componentMask;
			std::vector<Entity> entities;
			std::vector<std::function<void(Entity)>> addEntityCallbacks;
			std::vector<std::function<void(Entity)>> removeEntityCallbacks;

		};
		std::shared_ptr<SharedData> data;
		EntityQuerry(std::shared_ptr<SharedData> data) : data(std::move(data)) {}
	};
}
