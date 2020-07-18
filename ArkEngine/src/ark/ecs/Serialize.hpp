#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Graphics/Color.hpp>

#include "ark/core/Core.hpp"
#include "ark/ecs/Meta.hpp"

namespace sf {

	template <typename T>
	static void to_json(nlohmann::json& jsonObj, const Vector2<T>& vec)
	{
		jsonObj = nlohmann::json{{"x", vec.x,}, {"y", vec.y}};
	}

	template <typename T>
	static void from_json(const nlohmann::json& jsonObj, Vector2<T>& vec)
	{
		vec.x = jsonObj.at("x").get<T>();
		vec.y = jsonObj.at("y").get<T>();
	}

	static void to_json(nlohmann::json& jsonObj, const Time& time)
	{
		jsonObj = nlohmann::json(time.asSeconds());
	}

	static void from_json(const nlohmann::json& jsonObj, Time& time)
	{
		time = sf::seconds(jsonObj.get<float>());
	}

	static void to_json(nlohmann::json& jsonObj, const Color& color)
	{
		jsonObj = nlohmann::json{{"r", color.r}, {"g", color.g}, {"b", color.b}, {"a", color.a}};
	}

	static void from_json(const nlohmann::json& jsonObj, Color& color)
	{
		color.r = jsonObj.at("r").get<Uint8>();
		color.g = jsonObj.at("g").get<Uint8>();
		color.b = jsonObj.at("b").get<Uint8>();
		color.a = jsonObj.at("a").get<Uint8>();
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
		meta::doForAllProperties<Type>([&value, &jsonObj](auto& member) {
			using MemberType = meta::get_member_type<decltype(member)>;

			if constexpr (meta::isRegistered<MemberType>()) {
				auto local = member.getCopy(value);
				jsonObj[member.getName()] = serialize_value<MemberType>(&local);
			} else if constexpr (std::is_enum_v<MemberType> /*&& is registered */) {
				auto memberValue = member.getCopy(value);
				const auto& fields = meta::getEnumValues<MemberType>();
				const char* fieldName = meta::getNameOfEnumValue<MemberType>(memberValue);
				jsonObj[member.getName()] = fieldName;
			} else /* normal(convertible) value */ {
				jsonObj[member.getName()] = member.getCopy(value);
			}
		});
		return jsonObj;
	}

	template <typename Type>
	static void deserialize_value(const json& jsonObj, void* pValue)
	{
		Type& value = *static_cast<Type*>(pValue);
		meta::doForAllProperties<Type>([&value, &jsonObj](auto& member) {
			using MemberType = meta::get_member_type<decltype(member)>;

			if constexpr (meta::isRegistered<MemberType>()) {
				//const json& jsonMember = jsonObj.at(member.getName());
				MemberType memberValue;
				deserialize_value<MemberType>(jsonObj.at(member.getName()), &memberValue);
				member.set(value, memberValue);
			} else if constexpr (std::is_enum_v<MemberType>) {
				auto enumValueName = jsonObj.at(member.getName()).get<std::string>();
				auto memberValue = meta::getValueOfEnumName<MemberType>(enumValueName.c_str());
				member.set(value, memberValue);
			} else /* normal(convertible) value*/ {
				auto&& memberValue = jsonObj.at(member.getName()).get<MemberType>();
				member.set(value, memberValue);
			}

		});
	}
}

