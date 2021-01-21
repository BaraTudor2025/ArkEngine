#pragma once

#include "ark/ecs/SerdeJsonDirector.hpp"

template <typename T>
inline void addSerdeFunctions() {
	auto* type = ark::meta::getMetadata(typeid(T));
	type->func(ark::serde::serviceSerializeName, ark::serde::serialize_value<T>);
	type->func(ark::serde::serviceDeserializeName, ark::serde::deserialize_value<T>);
}

template <typename T>
inline void registerServiceDefault() {
	addSerdeFunctions<T>();
}
