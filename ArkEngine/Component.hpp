#pragma once

#include "Core.hpp"
#include "Util.hpp"
#include "static_any.hpp"

#include <bitset>
#include <unordered_map>
#include <vector>
#include <type_traits>

struct ARK_ENGINE_API Component {

};

// TOOD (comp manager): add a vector of destructors since we construct in place when 
class ComponentManager final : public NonCopyable  { 

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
			std::cout << "component manager dosent have component type: " << type.name() << '\n';
			return ArkInvalidIndex;
		}
		return pos - std::begin(this->componentIndexes);
	}

	template <typename T>
	int getComponentId()
	{
		return getComponentId(typeid(T));
	}

	// returns [component*, index]
	template <typename T, typename...Args>
	std::pair<T*, int> addComponent(Args&&... args) 
	{
		if (!hasComponentType<T>()) {
			std::cout << "component " << typeid(T).name() << " is not supported by any system\n";
		}

		auto compId = getComponentId<T>();
		auto& pool = getComponentPool<T>(compId, true);
		auto it = freeComponents.find(compId);

		if (it != freeComponents.end()) {
			auto& [compId, freeSlots] = *it;
			if (!freeSlots.empty()) {
				int index = freeSlots.back();
				freeSlots.pop_back();

				T* slot = &pool.at(index);
				// temporary solution to the destruction problem
				if constexpr (!std::is_trivially_destructible_v<T>)
					slot->~T();
				Util::construct_in_place<T>(slot, std::forward<Args>(args)...);
				return std::make_pair<T*, int>(std::move(slot), std::move(index));
			}
		}
		pool.emplace_back(std::forward<Args>(args)...);
		return std::make_pair<T*, int>(&pool.back(), pool.size() - 1);
	}

	template <typename T>
	T* getComponent(int compId, int index)
	{
		auto& pool = getComponentPool<T>(compId, false);
		return &pool.at(index);
	}

	void removeComponent(int compId, int index)
	{
		freeComponents[compId].push_back(index);
	}

private:

	void addComponentType(std::type_index type)
	{
		std::cout << "adding " << type.name() << " as component type\n";
		if (componentIndexes.size() == MaxComponentTypes) {
			std::cerr << "nu mai e loc de tipuri de componente, nr. max: " << MaxComponentTypes << std::endl;
			return;
		}
		componentIndexes.push_back(type);
	}

	template <typename T>
	ComponentPool<T>& getComponentPool(int compId, bool addPool)
	{
		auto& any_pool = this->componentPools.at(compId);
		if (addPool && any_pool.empty())
			any_pool = ComponentPool<T>();
		return any_cast<ComponentPool<T>>(any_pool);
	}

private:
	using any_pool = static_any<sizeof(ComponentPool<void*>)>;
	std::array<any_pool, MaxComponentTypes> componentPools; // component table
	std::vector<std::type_index> componentIndexes; // index of std::type_index represents index of component pool inside component table
	std::unordered_map<int, std::vector<int>> freeComponents; // indexed by compId, vector<int> holds the indexes of free components
};


