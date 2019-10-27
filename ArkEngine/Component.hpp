#pragma once

#include "Core.hpp"
#include "Util.hpp"

#include <libs/static_any.hpp>

#include <bitset>
#include <unordered_map>
#include <vector>
#include <type_traits>

struct ARK_ENGINE_API Component {

};

// TOOD (comp manager): add a vector of destructors since we construct in place when 
class ComponentManager final : public NonCopyable  { 

public:

	static inline constexpr std::size_t MaxComponentTypes = 32;
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

	int getComponentId(std::type_index type)
	{
		auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
		if (pos == std::end(this->componentIndexes)) {
			EngineLog(LogSource::ComponentM, LogLevel::Critical, "type not found (%s) ", Util::getNameOfType(type));
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
			EngineLog(LogSource::ComponentM, LogLevel::Error, "type (%s) is not supported", Util::getNameOfType<T>());
			return {nullptr, ArkInvalidIndex};
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
				if constexpr (!std::is_trivially_destructible_v<T>)
					metadataTable[typeid(T)].destruct(slot);
				Util::construct_in_place<T>(slot, std::forward<Args>(args)...);
				return {slot, index};
			}
		}
		pool.emplace_back(std::forward<Args>(args)...);
		return {&pool.back(), pool.size() - 1};
	}

	template <typename T>
	T* getComponent(int compId, int index)
	{
		auto& pool = getComponentPool<T>(compId, false);
		return &pool.at(index);
	}

	void* copyComponent(std::type_index type, void* component)
	{
		//auto& metadata = metadataTable[type];
		//ComponentPool pool = getComponentPool(type);
		//if (metadata.isCopyable) {
		//	pool.
		//}
	}

	void removeComponent(int compId, int index)
	{
		freeComponents[compId].push_back(index);
	}

private:

	template <typename T>
	void addComponentType()
	{
		const auto& type = typeid(T);
		if (hasComponentType(type))
			return;

		EngineLog(LogSource::ComponentM, LogLevel::Info, "adding type (%s)", Util::getNameOfType(type));
		if (componentIndexes.size() == MaxComponentTypes) {
			EngineLog(LogSource::ComponentM, LogLevel::Error, "aborting... nr max of components is &d, trying to add type (%s), no more space", MaxComponentTypes, Util::getNameOfType(type));
			std::abort();
		}
		componentIndexes.push_back(type);

		ComponentMetadata metadata;
		metadata.constructMetadataFrom<T>();
		metadataTable[typeid(T)] = metadata;
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
	using constructor_t = void (*)(void*);
	using copy_constructor_t = void (*)(void*, const void*);
	using destructor_t = void (*)(void*);

	template <typename T> static void defaultConstructorInstace(T* This) { new (This)T(); }
	template <typename T> static void copyConstructorInstace(T* This, const T* That) { new (This)T(*That); }
	template <typename T> static void destructorInstance(T* This) { This->~T(); }

	struct ComponentMetadata {
		std::string_view name;
		int size;
		bool isDefaultConstructible;
		bool isCopyable;

	private:
		constructor_t default_constructor = nullptr;
		copy_constructor_t copy_constructor = nullptr;
		destructor_t destructor = nullptr;

	public:
		ComponentMetadata() = default;

		template <typename T>
		void constructMetadataFrom()
		{
			size = sizeof(T);
			name = Util::getNameOfType<T>();

			if constexpr (std::is_default_constructible_v<T>) {
				default_constructor = Util::reinterpretCast<constructor_t>(defaultConstructorInstace<T>);
				isDefaultConstructible = true;
			}

			isCopyable = std::is_copy_constructible_v<T>;
			if constexpr (!std::is_trivially_copy_constructible_v<T>)
				copy_constructor = Util::reinterpretCast<copy_constructor_t>(copyConstructorInstace<T>);

			if constexpr (!std::is_trivially_destructible_v<T>)
				destructor = Util::reinterpretCast<destructor_t>(destructorInstance<T>);
		}

		void default_construct(void* p)
		{
			if (default_constructor)
				default_constructor(p);
		}

		void copy_construct(void* This, const void* That)
		{
			if (copy_constructor)
				copy_constructor(This, That);
		}

		void destruct(void* p)
		{
			if (destructor)
				destructor(p);
		}
	};

private:
	friend class System;
	std::vector<std::type_index> componentIndexes; // index of std::type_index represents index of component pool inside component table
	std::unordered_map<std::type_index, ComponentMetadata> metadataTable;
	using any_pool = static_any<sizeof(ComponentPool<void*>)>;
	std::array<any_pool, MaxComponentTypes> componentPools; // component table
	std::unordered_map<int, std::vector<int>> freeComponents; // indexed by compId, vector<int> holds the indexes of free components
};


