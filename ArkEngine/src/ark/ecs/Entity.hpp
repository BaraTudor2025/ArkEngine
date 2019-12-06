#pragma once

#include "ark/core/Core.hpp"
#include "ark/ecs/Component.hpp"

namespace ark {

	class EntityManager;
	class Script;

	// this is more of a handle
	// func definitions are at the bottom of EntityManager.hpp
	class Entity final {

	public:
		Entity() = default;
		~Entity() = default;

		Entity clone(std::string name = "");

		//TODO: Entity cloneWithScripts(std::string name = "");


		template <typename T, typename...Args>
		T& addComponent(Args&& ... args);

		void addComponent(std::type_index type);


		template <typename T>
		T& getComponent();

		// returns nullptr if component is not found
		template <typename T>
		T* tryGetComponent();


		// should only be used in the paused editor, or if only one system requires the 'T' component
		template <typename T>
		void removeComponent();

		void removeComponent(std::type_index type);


		template <typename T, typename...Args>
		T* addScript(Args&& ... args);

		Script* addScript(std::type_index type);

		// returns nullptr if script is not found
		template <typename T>
		T* getScript();

		Script* getScript(std::type_index type);


		template <typename T>
		bool hasScript();

		bool hasScript(std::type_index type);


		template <typename T>
		void setScriptActive(bool active);

		void setScriptActive(std::type_index type, bool active);


		template <typename T>
		void removeScript();

		void removeScript(std::type_index type);


		const ComponentManager::ComponentMask& getComponentMask();

		bool isValid() { return manager != nullptr; }

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

		const std::string& getName();

		void setName(std::string name);

		friend bool operator==(Entity left, Entity right)
		{
			return left.manager == right.manager && left.id == right.id;
		}

		friend bool operator!=(Entity left, Entity right)
		{
			if (left.manager == right.manager)
				return left.id != right.id;
			else
				return true;
		}

		friend bool operator<(Entity left, Entity right)
		{
			return left.id < right.id;
		}

	private:

		EntityManager* manager = nullptr;
		int id = ArkInvalidIndex;
		friend class EntityManager;
		friend class SceneInspector;
	};
}
