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
#include "ark/ecs/SerializeDirector.hpp"
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

		int idFromType(std::type_index type)
		{
			auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
			if (pos == std::end(this->componentIndexes)) {
				EngineLog(LogSource::ComponentM, LogLevel::Critical, "type not found (%s) ", Util::getNameOfType(type));
				return ArkInvalidIndex;
			}
			else 
				return pos - std::begin(this->componentIndexes);
		}

		std::type_index typeFromId(int id) const
		{
			return componentIndexes.at(id);
		}

		std::type_index typeFromName(std::string_view name)
		{
			for (const auto& type : componentIndexes) {
				if (name == Util::getNameOfType(type))
					return type;
			}
			EngineLog(LogSource::ComponentM, LogLevel::Error, "getTypeFromName() didn't find (%s)", name.data());
		}

		std::string_view nameFromId(int id)
		{
			return pools[id].metadata.name;
		}

		template <typename T>
		int idFromType()
		{
			return idFromType(typeid(T));
		}

		auto addComponent(int compId, bool defaultConstruct) -> std::pair<byte*, int>
		{
			auto& pool = pools.at(compId);
			auto [newComponent, newIndex] = pool.getFreeSlot();
			auto metadata = meta::getMetadata(this->typeFromId(compId));
			if(defaultConstruct)
				metadata->default_constructor(newComponent);
			return { newComponent, newIndex };
		}

		auto copyComponent(int compId, int index) -> std::pair<byte*, int>
		{
			auto& pool = pools.at(compId);
			auto [newComponent, newIndex] = pool.getFreeSlot();

			auto metadata = meta::getMetadata(this->typeFromId(compId));
			if (metadata->copy_constructor) {
				byte* copiedComponent = pool.componentAt(index);
				metadata->copy_constructor(newComponent, copiedComponent);
			}
			else
				metadata->default_constructor(newComponent);
			return { newComponent, newIndex };
		}

		void removeComponent(int compId, int index)
		{
			auto& pool = pools.at(compId);
			byte* component = pool.componentAt(index);
			auto metadata = meta::getMetadata(this->typeFromId(compId));
			if(metadata->destructor)
				metadata->destructor(component);
			pool.freeComponents.push_back(index);
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
			pools.emplace_back().constructPoolFrom<T>();
		}

		const std::vector<std::type_index>& getTypes() const
		{
			return this->componentIndexes;
		}

		json serializeComponent(int compId, void* pComp)
		{
			//return pools[compId].metadata.serialize(pComp);
			return {};
		}

		void deserializeComponent(int compId, void* pComp, const json& jsonObj)
		{
			//Entity e;
			//pools[compId].metadata.deserialize(e, jsonObj, pComp);
		}

	private:

		struct ComponentPool {

			struct Metadata {
				std::string_view name;
				int size;
				std::align_val_t alignment;

				//std::function<json(const void*)> serialize;
				//std::function<void(Entity, const json&, void*)> deserialize;
			};

			struct Chunk {
				Chunk(int bytes_num, std::align_val_t alignment, const std::function<void(void*)>& delete_buffer)
					: numberOfComponents(0),
					buffer((std::byte*)::operator new[](bytes_num, alignment), delete_buffer) 
				{ }
				std::unique_ptr<byte[], const std::function<void(void*)>&> buffer;
				int numberOfComponents;
			};

		private:
			int numberOfComponentsPerChunk;
			int chunk_size;
			std::function<void(void*)> custom_delete_buffer; // for chunk buffer
			std::vector<Chunk> chunks;
		public:
			std::vector<int> freeComponents;
			Metadata metadata;

			template <typename T>
			void constructPoolFrom()
			{
				metadata.name = Util::getNameOfType<T>();
				metadata.size = sizeof(T);
				//metadata.serialize = serialize_value<T>;
				//metadata.deserialize = deserialize_value<T>;
				metadata.alignment = std::align_val_t{ alignof(T) };
				custom_delete_buffer = [alignment = metadata.alignment](void* ptr) { ::operator delete[](ptr, alignment); };

				int kiloByte = 2 * 1024;
				numberOfComponentsPerChunk = kiloByte / metadata.size;
				chunk_size = metadata.size * numberOfComponentsPerChunk;
				EngineLog(LogSource::ComponentM, LogLevel::Info, "for (%s), chunkSize (%d), compNumPerChunk(%d), compSize(%d) ", Util::getNameOfType<T>(), chunk_size, numberOfComponentsPerChunk, metadata.size);
			}

			byte* componentAt(int index)
			{
				int chunkNum = index / numberOfComponentsPerChunk;
				int componentNum = index % numberOfComponentsPerChunk;
				return &chunks.at(chunkNum).buffer[componentNum * metadata.size];
			}

			// allocates a new chunk if enough space is not available
			auto getFreeSlot() -> std::pair<byte*, int>
			{
				if (freeComponents.empty()) {
					if (chunks.empty() || chunks.back().numberOfComponents == numberOfComponentsPerChunk)
						chunks.emplace_back(chunk_size, metadata.alignment, custom_delete_buffer);
					auto& chunk = chunks.back();
					int index = numberOfComponentsPerChunk * (chunks.size() - 1) + chunk.numberOfComponents;
					chunk.numberOfComponents += 1;
					byte* component = componentAt(index);
					return { component, index };
				}
				else {
					int index = freeComponents.back();
					freeComponents.pop_back();
					return { componentAt(index), index };
				}
			}
		};

		friend class System;
		//std::unordered_map<std::type_index, ComponentPool> pools;
		std::vector<std::type_index> componentIndexes;
		std::vector<ComponentPool> pools;
	};
}
