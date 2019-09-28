#pragma once

#include "Core.hpp"
#include "Component.hpp"
#include "Script.hpp"
#include "static_any.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>

#include <unordered_map>
#include <typeindex>
#include <optional>
#include <bitset>
#include <tuple>

#define ARK_ENGINE_API

class EntityManager;
class ScriptManager;
class Script;
class EntityArray;


// this is more of a handle
struct Entity final {

public:
	Entity() = default;
	~Entity() = default;

	template <typename T, typename...Args>
	T& addComponent(Args&& ... args) { 
		static_assert(std::is_base_of_v<Component, T>, std::string(typeid(T).name()) + " is not a component type");
		return manager->addComponentOnEntity<T>(*this, std::forward<Args>(args)...); 
	}

	template <typename T>
	T& getComponent()  { 
		static_assert(std::is_base_of_v<Component, T>, std::string(typeid(T).name()) + " is not a component type");
		return manager->getScriptOfEntity<T>(*this); 
	}

	template <typename T, typename...Args>
	T* addScript(Args&& ... args) {
		static_assert(std::is_base_of_v<Script, T>, std::string(typeid(T).name()) + " is not a script type");
		return manager->addScriptOnEntity<T>(*this, std::forward<Args>(args)...);  
	}

	template <typename T>
	T* getScript() { 
		static_assert(std::is_base_of_v<Script, T>, std::string(typeid(T).name()) + " is not a script type");
		return manager->getComponentOfEntity<T>(*this); 
	}

	template <typename T>
	void removeScript() { 
		static_assert(std::is_base_of_v<Script, T>, std::string(typeid(T).name()) + " is not a script type");
		return manager->removeScriptOfEntity<T>(*this); 
	}

	ComponentManager::ComponentMask getComponentMask() { return manager->getComponentMaskOfEntity(*this); }

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

	void addAsParent(Entity e) {
		this->removeFromParent();
		e.addChild(*this);
	}

	void removeFromParent() {
		auto p = getParent();
		if (p)
			p->removeChild(*this);
	}
#endif
//disable entity children

	const std::string& name() { return manager->getNameOfEntity(*this); }

	void setName(std::string name) { return manager->setNameOfEntity(*this, name); }

	friend bool operator==(Entity left, Entity right) {
		return left.manager == right.manager && left.id == right.id;
	}

	friend bool operator!=(Entity left, Entity right) {
		if (left.manager == right.manager)
			return left.id != right.id;
		else
			return true;
	}

private:

	EntityManager* manager = nullptr;
	int id = -1;
	friend class EntityManager;
};



class EntityManager final : public NonCopyable, public NonMovable {

public:
	EntityManager(ComponentManager& cm, ScriptManager& sm): componentManager(cm), scriptManager(sm) {}
	~EntityManager() = default;

public:

	Entity createEntity(std::string name = "")
	{
		Entity e;
		e.manager = this;
		if (!freeEntities.empty()) {
			e.id = freeEntities.back();
			freeEntities.pop_back();
			auto entity = getEntity(e);
			entity.name = name;
		} else {
			e.id = entities.size();
			entities.emplace_back();
			entities.back().name = name;
		}

		return e;
	}

	// TODO figure out a way to copy components without templates
	// only components are copied, scripts are excluded, maybe it's possible in ArchetypeManager?
	Entity cloneEntity(Entity e)
	{
		//Entity hClone = createEntity();
		//auto entity = getEntity(e);
		//auto clone = getEntity(hClone);
		//for (auto [compId, compIndex] : entity.componentsIndexes) {
		//	//componentManager.get
		//}

	}

	const std::string& getNameOfEntity(Entity e)
	{
		auto name = getEntity(e).name;
		if (name.empty())
			return std::string("entity") + std::to_string(e.id);
		else
			return name;
	}

	void setNameOfEntity(Entity e, std::string name)
	{
		getEntity(e).name = name;
	}

	Entity getEntityByName(std::string name)
	{
		for (int i = 0; i < entities.size(); i++) {
			if (entities[i].name == name) {
				Entity e;
				e.id = i;
				e.manager = this;
				return e;
			}
		}
		std::cout << "didn't find entity with name: " << name << '\n';
		return {};
	}

	void destroyEntity(Entity e)
	{
		freeEntities.push_back(e.id);
		auto entity = getEntity(e);

		for (auto [compId, compIndex] : entity.componentsIndexes)
			componentManager.removeComponent(compId, compIndex);
		scriptManager.removeScripts(entity.scriptsIndex);

		//entity.childrenIndex = -1;
		entity.name.clear();
		entity.mask.reset();
		entity.scriptsIndex = -1;
		entity.componentsIndexes.fill({-1, -1});
	}

	// if component already exists, then the existing component is returned
	template <typename T, typename... Args>
	T& addComponentOnEntity(Entity e, Args&&... args)
	{
		int compId = componentManager.getComponentId<T>();
		auto entity = getEntity(e);
		if (entity.mask.test(compId))
			return getComponentOfEntity<T>(e);

		auto [comp, compIndex] = componentManager.addComponent<T>(std::forward<Args>(args)...);
		//comp.m_entity = e;

		entity.mask.set(compId);
		int compNum = entity.mask.count();
		entity.componentsIndexes.at(compNum) = { compId, compIndex };

		return comp;
	}

	template <typename T>
	T& getComponentOfEntity(Entity e)
	{
		auto entity = getEntity(e);
		int compId = componentManager.getComponentId<T>();
		if (!entity.mask.test(compId)) {
			// make this an assert
			std::cerr << "entity " << entity.name << " dosent have component " << typeid(T).name();
		}
		
		int compIndex = -1;
		for (auto& compData : entity.componentsIndexes)
			if (compData.componentId == compId)
				compIndex = compData.index;

		return componentManager.getComponent<T>(compId, compIndex);
	}

	template <typename T, typename... Args>
	T* addScritpOnEntity(Entity e, Args&& ... args)
	{
		auto entity = getEntity(e);
		if (entity.scriptsIndex == -1) {
			auto [s, index] = scriptManager.addScript<T>(std::forward<Args>(args)...);
			s->m_entity = e;
			entity.scriptsIndex = index;
			return s;
		} else {
			auto s = scriptManager.addScriptAt<T>(index, std::forward<Args>(args)...);
			s->m_entity = e;
			return s;
		}
	}

	template <typename T>
	T* getScriptOfEntity(Entity e)
	{
		auto entity = getEntity(e);
		if (entity.scriptsIndex == -1) {
			std::cerr << "entity " << entity.name << " doesn't have the " << typeid(T).name() << " script\n";
			return nullptr;
		}

		auto s = scriptManager.getScript<T>(entity.scriptsIndex);
		if (s == nullptr) {
			std::cerr << "entity " << entity.name << " doesn't have the " << typeid(T).name() << " script\n";
		}

		return s;
	}

	template <typename T>
	void removeScriptOfEntity(Entity e)
	{
		auto entity = getEntity(e);
		if (entity.scriptsIndex != -1)
			scriptManager.removeScript<T>(entity.scriptsIndex);
	}

	ComponentManager::ComponentMask getComponentMaskOfEntity(Entity e) {
		return getEntity(e).mask;
	}

#if 0 // disable entity children
	void addChildTo(Entity p, Entity c)
	{
		if (!isValidEntity(p))
			return;
		if (!isValidEntity(c)) {
			std::cerr << "above entity is an invalid child";
			return;
		}

		getEntity(c).parentIndex = p.id;
		auto parent = getEntity(p);
		if (p.hasChildren()) {
			childrenTree.at(parent.childrenIndex).push_back(c);
		} else {
			childrenTree.emplace_back();
			parent.childrenIndex = childrenTree.size() - 1;
			childrenTree.back().push_back(c);
		}
	}

	std::optional<Entity> getParentOfEntity(Entity e)
	{
		auto entity = getEntity(e);
		if (entity.parentIndex == -1)
			return {};
		else {
			Entity parent(*this);
			parent.id = entity.parentIndex;
			return parent;
		}
	}

	std::vector<Entity> getChildrenOfEntity(Entity e, int depth)
	{
		if (!isValidEntity(e))
			return {};

		auto entity = getEntity(e);
		if (entity.childrenIndex == -1 || depth == 0)
			return {};

		auto cs = childrenTree.at(entity.childrenIndex);

		if (depth == 1)
			return cs;

		std::vector<Entity> children(cs);
		for (auto child : cs) {
			if (depth == Entity::AllChilren) {
				auto grandsons = getChildrenOfEntity(child, depth); // maintain depth
				children.insert(children.end(), grandsons.begin(), grandsons.end());
			} else {
				auto grandsons = getChildrenOfEntity(child, depth - 1); // 
				children.insert(children.end(), grandsons.begin(), grandsons.end());
			}
		}
		return children;
			
	}

	bool entityHasChildren(Entity e)
	{
		return getEntity(e).childrenIndex == -1;
	}
#endif // disable entity children


private:

	struct InternalEntityData {
		struct ComponentData {
			int16_t componentId = -1;
			int16_t index = -1;
		};
		ComponentManager::ComponentMask mask;
		std::array<ComponentData, ComponentManager::MaxComponentTypes> componentsIndexes;
		//int16_t childrenIndex = -1;
		//int16_t parentIndex = -1;
		int16_t scriptsIndex = -1;
		std::string name = "";
	};

	InternalEntityData& getEntity(Entity e)
	{
		return entities.at(e.id);
	}

private:
	std::vector<InternalEntityData> entities;

	ComponentManager& componentManager;
	ScriptManager& scriptManager;
	std::vector<int> freeEntities;
	//std::vector<std::vector<Entity>> childrenTree;
};


// future entity archetype manager to write
// components of a single entity are stored in the same buffer, as oposed to storing them in pools
#if false

namespace Experimental {

	static inline constexpr std::size_t MaxComponentTypes = 64;

	//using Archetype = const ArchetypeMask&;

	// component must be POD type
	class ArchetypeComponentManger {

	public:

		using ArchetypeMask = std::bitset<MaxComponentTypes>;

	private:
		template <typename T>
		void addType()
		{
			if (!hasComponentType<T>())
				addComponentType<T>();
		}


		template <typename T>
		bool hasComponentType()
		{
			auto pos = std::find(std::begin(this->types), std::end(this->types), typeid(T));
			return pos != std::end(this->types);
		} 

		int getComponentId(std::type_index type)
		{
			auto pos = std::find(std::begin(this->types), std::end(this->types), type);
			if (pos == std::end(this->types)) {
				std::cout << "component manager dosent have component type: " << type.name();
				return -1;
			}
			return pos - std::begin(this->types);
		}

		template <typename T>
		int getComponentId()
		{
			getComponentId(typeid(T));
		}

		//static std::string getComponentNames()
		//{
		//}

	private:

		template <typename T>
		void addComponentType()
		{
			if (types.size() == MaxComponentTypes) {
				std::cerr << "nu mai e loc de tipuri de componente, max: " << MaxComponentTypes << std::endl;
				return;
			}
			types.push_back(typeid(T));
		}

	private:

		static constexpr inline int chunkSizeInBytes = 2 * 1024;

		using Chunk = std::vector<uint8_t>;

		struct ComponentMetadata {
			std::type_index type;
			int size;
		};

		struct InternalComponentArchetypeData {
			int entitySize; // sum of sizes of component types in this archetype
			std::vector<ComponentMetadata> metadata;
			std::vector<Chunk> chunks;
		};

		struct PendingComponentData {
			std::vector<int8_t> buffer; // temporary component storage
			std::vector<ComponentMetadata> componentMetadata;
			ArchetypeMask archetype;
		};

		std::vector<std::type_index> types;
		//std::vector<std::any> componentTemplates; // has an instance defautl constructed of each component type, used for cloning
		std::unordered_map<ArchetypeMask, InternalComponentArchetypeData> pools;
		std::unordered_map<ArchetypeMask, std::vector<int>> freeSlots;
		std::vector<PendingComponentData> pendingComponents;

		friend class ArchetypeEntityManager;
	};


	class ArchetypeEntityManager;
	//class ArchetypeEntityManager::EntityData;


	class Entity {

	public:

	private:
		ArchetypeEntityManager& manager;
		//ArchetypeEntityManager::EntityData& data;
		friend class ScriptManager;
		friend class ArchetypeEntityManager;
	};


	class ArchetypeEntityManager {

	public:


		//template <typename T>
		//bool hasComponentType()

		//template <typename T>
		//void addComponentType()

		//template <typename T>
		//int getComponentId()

		//template <typename T>
		//ComponentContainer<T>& getComponentPool()

		template <typename T, typename... Args>
		T* addComponentOnEntity(Entity& entity, Args&& ... args);

		template <typename T>
		T* getComponentOfEntity(Entity& entity);

		template <typename... Ts>
		std::tuple<Ts...> getComponents(); // TODO: cum plm o implementez pe asta getComponents->std::tuple<ts...>?

		// as putea sa fac asa, si fac if constexpr(!is_same<T3, void*>) {...} s.a.m.d.
		// ar trebui sa returnez nullptr pentru 'void*'
		template <
			typename T1, 
			typename T2, 
			typename T3=void*, 
			typename T4=void*, 
			typename T5=void*>
		std::tuple<T1, T2, T3, T4, T5> getComponents2(); 

		// sau...
		template <typename T1, typename T2>
		std::tuple<T1, T2> getComponents3();

		// s.a.m.d. ...
		template <typename T1, typename T2, typename T3>
		std::tuple<T1, T2, T3> getComponents3();

	private:

		struct EntityData {
			ComponentManager::ComponentMask mask;
			std::vector<int8_t> buffer; // component storage
			struct ComponentMetadata {
				std::type_index type;
				int size;
			};
			std::vector<ComponentMetadata> componentMetadata;
		};

		//struct EntitiesPool {
		//	Detail::ComponentMask componentMask;
		//	std::vector<int8_t> buffer;
		//	std::deque<EntityData> pool;
		//};
		//std::vector<EntitiesPool> pools;

		std::unordered_map<ComponentManager::ComponentMask, std::deque<EntityData>> pools;

		struct PendingEntity {
			EntityData entityData;
			ComponentManager::ComponentMask componentMask;
		};
		std::vector<PendingEntity> pendingEntities;
	};
}
#endif

