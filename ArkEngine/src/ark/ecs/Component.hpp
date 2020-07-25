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
			auto pos = std::find(std::begin(componentIndexes), std::end(componentIndexes), type);// [&type](const auto& data) { return data.type == type; });
			return pos != std::end(componentIndexes);
		}

		int idFromType(std::type_index type)
		{
			auto pos = std::find(std::begin(componentIndexes), std::end(componentIndexes), type);//[&type](const auto& data) { return data.type == type; });
			if (pos == std::end(componentIndexes)) {
				EngineLog(LogSource::ComponentM, LogLevel::Critical, "type not found (%s) ", Util::getNameOfType(type));
				return ArkInvalidIndex;
			}
			else 
				return pos - std::begin(componentIndexes);
		}

		auto addComponent(std::type_index type, bool defaultConstruct) -> std::pair<byte*, int>
		{
			auto& pool = getPool(type);
			auto [newComponent, newIndex] = pool.getFreeSlot();
			auto metadata = meta::getMetadata(type);
			if(defaultConstruct)
				metadata->default_constructor(newComponent);
			return { newComponent, newIndex };
		}

		auto copyComponent(std::type_index type, int index) -> std::pair<byte*, int>
		{
			auto& pool = getPool(type);
			auto [newComponent, newIndex] = pool.getFreeSlot();

			auto metadata = meta::getMetadata(type);
			if (metadata->copy_constructor) {
				byte* copiedComponent = pool.componentAt(index);
				metadata->copy_constructor(newComponent, copiedComponent);
			}
			else
				metadata->default_constructor(newComponent);
			return { newComponent, newIndex };
		}

		void removeComponent(std::type_index type, int index)
		{
			auto& pool = getPool(type);
			byte* pComponent = pool.componentAt(index);
			auto metadata = meta::getMetadata(type);
			if(metadata->destructor)
				metadata->destructor(pComponent);
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

	private:

		struct ComponentPool {

			struct Metadata {
				int size;
				std::align_val_t alignment;
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
				metadata.size = sizeof(T);
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

			// allocates a new chunk if not enough space is available
			auto getFreeSlot() -> std::pair<byte*, int>
			{
				if (freeComponents.empty()) {
					if (chunks.empty() || chunks.back().numberOfComponents == numberOfComponentsPerChunk)
						chunks.emplace_back(chunk_size, metadata.alignment, custom_delete_buffer);
					auto& chunk = chunks.back();
					int index = numberOfComponentsPerChunk * (chunks.size() - 1) + chunk.numberOfComponents;
					chunk.numberOfComponents += 1;
					byte* pComponent = componentAt(index);
					return { pComponent, index };
				}
				else {
					int index = freeComponents.back();
					freeComponents.pop_back();
					return { componentAt(index), index };
				}
			}
		};
		friend class System;
		std::vector<std::type_index> componentIndexes;
		std::vector<ComponentPool> pools;

		ComponentPool& getPool(std::type_index type)
		{
			return pools.at(idFromType(type));
		}

	};
}
