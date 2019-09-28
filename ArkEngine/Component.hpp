#pragma once

#include "Core.hpp"
#include "static_any.hpp"

#include <bitset>
#include <unordered_map>

class Entity;

struct ARK_ENGINE_API Component {

	//Entity entity() { return m_entity; }

//private:
	//Entity m_entity;
	//friend class EntityManager;
};

class ComponentManager {

public:

	static inline constexpr std::size_t MaxComponentTypes = 64;
	using ComponentMask = std::bitset<MaxComponentTypes>;

	template <typename T>
	using ComponentPool = std::deque<T>;

	bool hasComponentType(std::type_index type)
	{
		auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
		return pos != std::end(this->componentIndexes);
	} 

	template <typename T>
	bool hasComponentType()
	{
		return hasComponentType(typeid(T));
	} 

	// add component type if not present
	int getComponentId(std::type_index type)
	{
		if (!hasComponentType(type))
			addComponentType(type);

		auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
		if (pos == std::end(this->componentIndexes)) {
			std::cout << "component manager dosent have component type: " << type.name();
			return -1;
		}
		return pos - std::begin(this->componentIndexes);
	}

	template <typename T>
	int getComponentId()
	{
		getComponentId(typeid(T));
	}

	// returns [component, index]
	template <typename T, typename...Args>
	std::pair<T&, int> addComponent(Args&&... args) 
	{
		if (!hasComponentType<T>()) {
			std::cout << "component " << typeid(T).name() " is not supported by any system\n";
		}

		auto compId = getComponentId<T>();
		auto pool = getComponentPool<T>(compId);
		auto it = freeComponents.find(compId);

		if (it != freeComponents.end() && !it->empty()) {
			int index = it->back();
			pool.at(index) = T(std::forward<Args>(args)...);
			it->pop_back();
			return std::make_pair(pool.at(index), index);
		} else {
			pool.emplace_back(std::forward<Args>(args)...);
			return std::make_pair(pool.back(), pool.size() - 1);
		}

	}

	template <typename T>
	T& getComponent(int compId, int index)
	{
		auto pool = getComponentPool<T>(compId);
		return pool.at(index);
	}

	void removeComponent(int compId, int index)
	{
		freeComponents[compId].push_back(index);
	}

private:

	void addComponentType(std::type_index type)
	{
		if (componentIndexes.size() == MaxComponentTypes) {
			std::cerr << "nu mai e loc de tipuri de componente, max: " << MaxComponentTypes << std::endl;
			return;
		}
		componentIndexes.push_back(type);
		componentPools.emplace_back();
	}

	template <typename T>
	ComponentPool<T>& getComponentPool(int compId)
	{
		auto& any_pool = this->componentPools.at(compId);
		return any_cast<ComponentPool<T>>(any_pool);
	}

private:

	using any_pool = static_any<sizeof(ComponentPool<void*>)>;
	std::vector<any_pool> componentPools; // component table
	std::vector<std::type_index> componentIndexes; // index of std::type_index represents index of component pool inside component table
	std::unordered_map<int, std::vector<int>> freeComponents; // indexed by compId, vector<int> holds the indexes of free components
};
