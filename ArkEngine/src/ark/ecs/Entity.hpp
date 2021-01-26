#pragma once

#include "ark/core/Core.hpp"
#include "ark/ecs/Component.hpp"

namespace ark {

	class EntityManager;

	struct RuntimeComponent {
		std::type_index type = typeid(void);
		void* ptr = nullptr;
	};

	class RuntimeComponentView;

	// this is more of a handle
	// member function definitions are at the bottom of EntityManager.hpp
	class [[nodiscard]] Entity final {

	public:
		using ID = int;

		Entity() = default;
		~Entity() = default;
		Entity(ID id, EntityManager* m) : id(id), manager(m) {}

		ID getID() const { return id; }

		[[nodiscard]]
		bool isValid() const;

		operator bool() const { return isValid(); }

		template <ConceptComponent T, typename...Args>
		T& addComponent(Args&& ... args);

		void addComponent(std::type_index type);

		template <ConceptComponent T>
		requires std::is_aggregate_v<T>
		T& add(T&& comp = T{}) {
			return this->addComponent<T>(std::forward<T>(comp));
		}

		template <ConceptComponent T, typename...Args>
		T& add(Args&& ... args) {
			return this->addComponent<T>(std::forward<Args>(args)...);
		}

		void add(std::type_index type) {
			this->addComponent(type);
		}

		template <ConceptComponent T>
		[[nodiscard]]
		T& getComponent();

		template <ConceptComponent T>
		[[nodiscard]]
		const T& getComponent() const;

		[[nodiscard]]
		void* getComponent(std::type_index type);

		template <ConceptComponent... Cs>
		auto getComponents() -> std::tuple<Cs&...>
		{
			return { this->getComponent<Cs>()... };
		}

		template <ConceptComponent... Ts>
		decltype(auto) get() {
			static_assert(sizeof...(Ts) > 0, "trebuie sa ai argumente");
			if constexpr (sizeof...(Ts) == 1)
				return this->getComponent<Ts...>();
			else
				return this->getComponents<Ts...>();
		}

		// returns nullptr if component is not found
		template <ConceptComponent T>
		[[nodiscard]]
		T* tryGetComponent();

		template <ConceptComponent T>
		[[nodiscard]]
		const T* tryGetComponent() const;

		template <typename F>
		requires std::invocable<F, RuntimeComponent>
		void forEachComponent(F&& f);

		[[nodiscard]]
		auto runtimeComponentView() -> RuntimeComponentView;

		// should only be used in the paused editor, or if only one system requires the 'T' component
		// TODO: remove on postUpdate?

		template <ConceptComponent T>
		void removeComponent();

		void removeComponent(std::type_index type);

	public:
		[[nodiscard]]
		auto getComponentMask() const -> const ComponentMask&;

#if 0 //disable entity children

		enum { AllChilren = -1 };

		// if depht is AllChilren then get the whole hierarchy, else get until specified depht
		// depth 1 means children, 2 means children and grandchildren, 3 etc...
		// depth 0 is consireded no children

		std::vector<Entity> getChildren(int depth = 1) { return manager.getChildrenOfEntity(*this, depth); }

		template <typename T>
		std::vector<T*> getComponentsInChildren();

		template <typename T>
		std::vector<T*> getComponentInParents();

		bool hasChildren() { return manager.entityHasChildren(*this); }

		std::optional<Entity> getParent() { return manager.getParentOfEntity(*this); }
		void addChild(Entity e) { manager.addChildTo(*this, e); }
		void removeChild(Entity e);

		void addAsParent(Entity e)
		{
			this->removeFromParent();
			e.addChild(*this);
		}

		void removeFromParent()
		{
			auto p = getParent();
			if (p)
				p->removeChild(*this);
		}
#endif
		//disable entity children

		friend bool operator==(const Entity left, const Entity right)
		{
			return left.manager == right.manager && left.id == right.id;
		}

		friend bool operator!=(const Entity left, const Entity right)
		{
			if (left.manager == right.manager)
				return left.id != right.id;
			else
				return true;
		}

		friend bool operator<(const Entity left, const Entity right)
		{
			return left.id < right.id;
		}

	private:

		EntityManager* manager = nullptr;
		int id = ArkInvalidIndex;
		friend class EntityManager;
		friend class Registry;
	};
}
