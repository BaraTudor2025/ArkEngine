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

constexpr auto ARK_META_COMPONENT_GROUP = std::string_view{"components"};

namespace ark {
	// semi regulat type + move ctor
	// TODO: make copy mandatory

	template <typename T>
	concept ConceptComponent = std::default_initializable<T> && std::move_constructible<std::remove_const_t<T>> //&& std::copy_constructible<T>
		&& std::is_object_v<T> && !std::is_pointer_v<T>; 

	static inline constexpr std::size_t MaxComponentTypes = 32;
	using ComponentMask = std::bitset<MaxComponentTypes>;
}
