#pragma once

#include "Component.hpp"
#include "EntityManager.hpp"
#include "Entity.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <concepts>
#include <algorithm>

namespace ark
{
	template <typename... Ts>
	class EntityQuery {
		std::vector<ScopedConnection> m_conns;
		std::vector<ark::Entity> m_entities;
		ark::ComponentMask m_mask;
		bool m_modified = false;

	public:
		EntityQuery(ark::EntityManager& man) {
			this->connect(man);
			man.idFromType<Ts...>(m_mask);
			man.view<Ts...>().each([&, this](ark::Entity ent) {
				m_entities.push_back(ent);
			});
		}
		EntityQuery() = default;
		EntityQuery(EntityQuery&&) = default;

		auto begin() { return m_entities.begin(); }
		auto end() { return m_entities.end(); }

		void connect(ark::EntityManager& man) {
			(man.addType(typeid(Ts)), ...);
			(m_conns.emplace_back(man.onAdd<Ts>().connect([this](ark::EntityManager& man, ark::Entity entity) {
				if ((man.has(entity, m_mask))) {
					m_modified = true;
					m_entities.push_back(entity);
				}
			})), ...);
			(m_conns.emplace_back(man.onRemove<Ts>().connect([this](ark::EntityManager& man, ark::Entity entity) {
				m_modified = true;
				std::erase(m_entities, entity);
			})), ...);
		}

		const auto& entities() const {
			return m_entities;
		}

		template <typename F>
		void sort(F&& f) {
			std::sort(m_entities.begin(), m_entities.end(), std::forward<F>(f));
		}

		void reconstruct(ark::EntityManager& manager) {
			if (m_modified) {
				m_entities.clear();
				for (auto view = manager.view<Ts...>(); ark::Entity ent : view.each<ark::Entity>())
					m_entities.push_back(ent);
				m_modified = false;
			}
		}

		bool isDirty() {
			return m_modified;
		}
	};
}
