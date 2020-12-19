#pragma once

#include <bitset>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <functional>
#include <any>
#include <memory>
#include <memory_resource>

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
		}

		auto getTypes() const -> std::vector<std::type_index> const& 
		{
			return this->componentIndexes;
		}

	private:
		std::vector<std::type_index> componentIndexes;
	};
}
