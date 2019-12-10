#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Graphics/Color.hpp>

#include "ark/core/Core.hpp"
#include "ark/ecs/Meta.hpp"

namespace sf {

	static void to_json(nlohmann::json& jsonObj, const Vector2f& vec)
	{
		jsonObj = nlohmann::json{{"x", vec.x,}, {"y", vec.y}};
	}

	static void to_json(nlohmann::json& jsonObj, const Time& time)
	{
		jsonObj = nlohmann::json(time.asSeconds());
	}

	static void to_json(nlohmann::json& jsonObj, const Color& color)
	{
		jsonObj = nlohmann::json{{"r", color.r}, {"g", color.g}, {"b", color.b}, {"a", color.a}};
	}
}

namespace ark {

	using nlohmann::json;

	// TODO (json): serialize sf::Vector2f with json::adl_serializer or meta::register?
	template <typename Type>
	static json serialize_value(const void* pValue)
	{
		const Type& value = *static_cast<const Type*>(pValue);
		json jsonObj;
		meta::doForAllMembers<Type>([&value, &jsonObj](auto& member) {
			using MemberType = meta::get_member_type<decltype(member)>;

			if constexpr (meta::isRegistered<MemberType>()) {
				auto local = member.getCopy(value);
				jsonObj[member.getName()] = serialize_value<MemberType>(&local);
			} else if constexpr (std::is_enum_v<MemberType> /*&& is registered */) {
				auto memberValue = member.getCopy(value);
				const auto& fields = meta::getEnumValues<MemberType>();
				const char* fieldName = meta::getNameOfEnumValue<MemberType>(memberValue);
				jsonObj[member.getName()] = fieldName;
			} else /* normal value */ {
				jsonObj[member.getName()] = member.getCopy(value);
			}
		});
		return jsonObj;
	}
}

