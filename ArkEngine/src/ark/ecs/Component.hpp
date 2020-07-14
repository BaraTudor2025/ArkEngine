#pragma once


#include <bitset>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <functional>
#include <any>

#include "ark/core/Core.hpp"
#include "ark/core/Logger.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/SceneInspector.hpp"
#include "ark/ecs/Serialize.hpp"
#include "ark/util/Util.hpp"

namespace ark {

	template <typename T>
	struct ARK_ENGINE_API Component {
		Component()
		{
			static_assert(std::is_default_constructible_v<T>, "Component needs to have default constructor");
		}
	};

	class ComponentManager final : NonCopyable {

	public:
		using byte = std::byte;
		static inline constexpr std::size_t MaxComponentTypes = 32;
		using ComponentMask = std::bitset<MaxComponentTypes>;

		bool hasComponentType(std::type_index type)
		{
			auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
			return pos != std::end(this->componentIndexes);
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

		std::type_index getTypeFromId(int id)
		{
			return componentIndexes.at(id);
		}

		std::type_index getTypeFromName(std::string_view name)
		{
			for (const auto& type : componentIndexes) {
				if (name == Util::getNameOfType(type))
					return type;
			}
			EngineLog(LogSource::ComponentM, LogLevel::Error, "getTypeFromName() didn't find (%s)", name.data());
		}

		std::string_view getComponentName(int id)
		{
			return pools[id].metadata.name;
		}

		template <typename T>
		int getComponentId()
		{
			return getComponentId(typeid(T));
		}


		auto addComponent(int compId) -> std::pair<byte*, int>
		{
			auto& pool = pools.at(compId);
			auto [component, index] = pool.getFreeSlot();
			pool.metadata.default_constructor(component);
			return { component, index };
		}

		auto copyComponent(int compId, int index) -> std::pair<byte*, int>
		{
			auto& pool = pools.at(compId);
			return pool.copyComponent(index);
		}

		void removeComponent(int compId, int index)
		{
			auto& pool = pools.at(compId);
			pool.removeComponent(index);
		}

		template <typename T>
		void addComponentType()
		{
			static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
			const auto& type = typeid(T);
			if (hasComponentType(type))
				return;

			EngineLog(LogSource::ComponentM, LogLevel::Info, "adding type (%s)", Util::getNameOfType(type));
			if (componentIndexes.size() == MaxComponentTypes) {
				EngineLog(LogSource::ComponentM, LogLevel::Error, "aborting... nr max of components is &d, trying to add type (%s), no more space", MaxComponentTypes, Util::getNameOfType(type));
				std::abort();
			}
			componentIndexes.push_back(type);
			pools.emplace_back();
			pools.back().constructPoolFrom<T>();
		}

		void renderEditorOfComponent(int* widgetId, int compId, void* component)
		{
			auto& pool = pools.at(compId);
			pool.metadata.renderFields(widgetId, component);
		}

		std::vector<std::type_index>& getTypes()
		{
			return this->componentIndexes;
		}

		json serializeComponent(int compId, void* pComp)
		{
			return pools[compId].metadata.serialize(pComp);
		}

		void deserializeComponent(int compId, void* pComp, const json& jsonObj)
		{
			pools[compId].metadata.deserialize(jsonObj, pComp);
		}

	private:

		struct ComponentPool {

			struct Metadata {
				std::string_view name;
				std::align_val_t alignment;
				int size;
				std::function<void(int*, void*)> renderFields; // render in editor
				std::function<json(const void*)> serialize;
				std::function<void(const json&, void*)> deserialize;

				void(*default_constructor)(void*) = nullptr;
				void(*copy_constructor)(void*, const void*) = nullptr;
				void(*destructor)(void*) = nullptr;
				//void(*move_constructor)(void*, void*) = nullptr;
				//void(*relocate)(void*, void*) = nullptr; /*move + dtor?*/
			};

			struct Chunk {
				Chunk(int bytes_num, std::align_val_t alignment, const std::function<void(void*)>& delete_buffer)
					: numberOfComponents(0),
					buffer((std::byte*)::operator new[](bytes_num, alignment), delete_buffer) { }
				std::unique_ptr<byte[], std::function<void(void*)>> buffer;
				int numberOfComponents;
			};

		private:
			int numberOfComponentsPerChunk;
			int chunk_size;
			std::vector<Chunk> chunks;
			std::vector<int> freeComponents;
			std::function<void(void*)> custom_delete_buffer; // for chunk buffer
		public:
			Metadata metadata;

			template <typename T>
			void constructPoolFrom()
			{
				metadata.name = Util::getNameOfType<T>();
				metadata.size = sizeof(T);
				metadata.renderFields = SceneInspector::renderFieldsOfType<T>;
				metadata.serialize = serialize_value<T>;
				metadata.deserialize = deserialize_value<T>;
				metadata.alignment = std::align_val_t{ alignof(T) };
				custom_delete_buffer = [alignment = metadata.alignment](void* ptr) { ::operator delete[](ptr, alignment); };

				metadata.default_constructor = [](void* This) { new(This)T(); };

				if constexpr (std::is_copy_constructible_v<T>)
					metadata.copy_constructor = [](void* This, const void* That) { new (This)T(*static_cast<const T*>(That)); };

				if constexpr (!std::is_trivially_destructible_v<T>)
					metadata.destructor = [](void* This) { static_cast<T*>(This)->~T(); };
				else
					metadata.destructor = [](void*) {};

				int kiloByte = 2 * 1024;
				numberOfComponentsPerChunk = kiloByte / metadata.size;
				chunk_size = metadata.size * numberOfComponentsPerChunk;
				EngineLog(LogSource::ComponentM, LogLevel::Info, "for (%s), chunkSize (%d), compNumPerChunk(%d), compSize(%d) ", Util::getNameOfType<T>(), chunk_size, numberOfComponentsPerChunk, metadata.size);
			}

			auto copyComponent(int index) -> std::pair<byte*, int>
			{
				auto [newComponent, newIndex] = getFreeSlot();
				byte* component = getComponent(index);
				if (metadata.copy_constructor)
					metadata.copy_constructor(newComponent, component);
				else
					metadata.default_constructor(newComponent);
				return { newComponent, newIndex };
			}

			void removeComponent(int index)
			{
				byte* component = getComponent(index);
				metadata.destructor(component);
				freeComponents.push_back(index);
			}

			std::pair<int, int> getNums(int index)
			{
				int chunkNum = index / numberOfComponentsPerChunk;
				int componentNum = index % numberOfComponentsPerChunk;
				return { chunkNum, componentNum };
			}

			byte* getComponent(int index)
			{
				auto [chunkNum, componentNum] = getNums(index);
				auto& chunk = chunks.at(chunkNum);
				byte* component = &chunk.buffer[componentNum * metadata.size];
				return component;
			}

			auto getFreeSlot() -> std::pair<byte*, int>
			{
				if (freeComponents.empty()) {
					if (chunks.empty() || chunks.back().numberOfComponents == numberOfComponentsPerChunk)
						chunks.emplace_back(chunk_size, metadata.alignment, custom_delete_buffer);
					auto& chunk = chunks.back();
					int index = numberOfComponentsPerChunk * (chunks.size() - 1) + chunk.numberOfComponents;
					byte* component = getComponent(index);
					chunk.numberOfComponents += 1;
					return { component, index };
				}
				else {
					int index = freeComponents.back();
					freeComponents.pop_back();
					return { getComponent(index), index };
				}
			}
		};

		friend class System;
		friend class SceneInspector;
		//std::unordered_map<std::type_index, ComponentPool> pools;
		std::vector<std::type_index> componentIndexes;
		std::vector<ComponentPool> pools;
	};
}
