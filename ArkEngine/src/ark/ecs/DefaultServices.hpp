#pragma once

#include "ark/ecs/SceneInspector.hpp"
#include "ark/ecs/SerdeJsonDirector.hpp"

#define ARK_SERVICE_INSPECTOR ark::meta::service<Type>(ark::SceneInspector::serviceName, &ark::SceneInspector::renderPropertiesOfType<Type>)

#define ARK_SERVICE_SERDE \
	ark::meta::service<Type>(ark::serde::serviceSerializeName, ark::serde::serialize_value<Type>), \
	ark::meta::service<Type>(ark::serde::serviceDeserializeName, ark::serde::deserialize_value<Type>)

#define ARK_DEFAULT_SERVICES \
	ARK_SERVICE_INSPECTOR, ARK_SERVICE_SERDE

template <typename T>
inline void registerServiceInspector() {
	ark::meta::service<T>(ark::SceneInspector::serviceName, &ark::SceneInspector::renderPropertiesOfType<T>);
}

template <typename T>
inline void registerServiceSerde() {
	ark::meta::service<T>(ark::serde::serviceSerializeName, ark::serde::serialize_value<T>);
	ark::meta::service<T>(ark::serde::serviceDeserializeName, ark::serde::deserialize_value<T>);
}

template <typename T>
inline void registerServiceDefault() {
	registerServiceInspector<T>();
	registerServiceSerde<T>();
}
