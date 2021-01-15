#pragma once

#include "ark/ecs/SceneInspector.hpp"
#include "ark/ecs/SerdeJsonDirector.hpp"

template <typename T>
inline void registerServiceInspector() {
	ark::meta::service<T>(ark::SceneInspector::serviceName, &ark::SceneInspector::renderPropertiesOfType<T>);
}

template <typename T>
inline void registerServiceInspectorOptions(std::vector<ark::EditorOptions> options) {
	ark::meta::service<T, std::vector<ark::EditorOptions>>(ark::SceneInspector::serviceOptions,  options );
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
