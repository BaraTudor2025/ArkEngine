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
			return Util::contains(componentIndexes, type);
		}

		int idFromType(std::type_index type)
		{
			auto pos = std::find(std::begin(componentIndexes), std::end(componentIndexes), type);
			if (pos == std::end(componentIndexes)) {
				EngineLog(LogSource::ComponentM, LogLevel::Critical, "type not found (%s) ", meta::getMetadata(type)->name);
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

		void addComponentType(std::type_index type)
		{
			if (hasComponentType(type))
				return;

			EngineLog(LogSource::ComponentM, LogLevel::Info, "adding type (%s)", meta::getMetadata(type)->name);
			if (componentIndexes.size() == MaxComponentTypes) {
				EngineLog(LogSource::ComponentM, LogLevel::Error, 
					"aborting... nr max of components is &d, trying to add type (%s), no more space", MaxComponentTypes, meta::getMetadata(type)->name);
				std::abort();
			}
			componentIndexes.push_back(type);
			pools.emplace_back().constructPoolFrom(type);
		}

		const std::vector<std::type_index>& getTypes() const
		{
			return this->componentIndexes;
		}

	private:

		struct ComponentPool {

			struct Chunk {
				Chunk(int bytes_num, std::align_val_t align, const std::function<void(void*)>& delete_buffer)
					: numberOfComponents(0),
					buffer((std::byte*)::operator new[](bytes_num, align), delete_buffer) 
				{ }
				std::unique_ptr<byte[], const std::function<void(void*)>&> buffer;
				int numberOfComponents;
			};

		private:
			int numberOfComponentsPerChunk;
			int chunkSize;
			std::function<void(void*)> custom_delete_buffer; // for chunk buffer
			std::vector<Chunk> chunks;
			int componentSize;
			std::align_val_t align;
		public:
			std::vector<int> freeComponents;

			void constructPoolFrom(std::type_index type)
			{
				auto mdata = meta::getMetadata(type);
				componentSize = mdata->size;
				align = std::align_val_t{ mdata->align };
				custom_delete_buffer = [alignment = align](void* ptr) { ::operator delete[](ptr, alignment); };

				int kiloByte = 2 * 1024;
				numberOfComponentsPerChunk = kiloByte / componentSize;
				chunkSize = componentSize * numberOfComponentsPerChunk;
				EngineLog(LogSource::ComponentM, LogLevel::Info,
					"for (%s), chunkSize (%d), compNumPerChunk(%d), compSize(%d)",
					mdata->name , chunkSize, numberOfComponentsPerChunk, componentSize);
			}

			byte* componentAt(int index)
			{
				int chunkNum = index / numberOfComponentsPerChunk;
				int componentNum = index % numberOfComponentsPerChunk;
				return &chunks.at(chunkNum).buffer[componentNum * componentSize];
			}

			// allocates a new chunk if not enough space is available
			auto getFreeSlot() -> std::pair<byte*, int>
			{
				if (freeComponents.empty()) {
					if (chunks.empty() || chunks.back().numberOfComponents == numberOfComponentsPerChunk)
						chunks.emplace_back(chunkSize, align, custom_delete_buffer);
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
		std::vector<std::type_index> componentIndexes;
		std::vector<ComponentPool> pools;

		ComponentPool& getPool(std::type_index type)
		{
			return pools.at(idFromType(type));
		}

	};
}
