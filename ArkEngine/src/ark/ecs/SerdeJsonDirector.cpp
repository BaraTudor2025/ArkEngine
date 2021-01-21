#include <fstream>
#include "ark/ecs/SerdeJsonDirector.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Scene.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/util/ResourceManager.hpp"
#include "ark/ecs/components/Transform.hpp"

namespace ark::serde
{
	static inline const std::string sEntityFolder = Resources::resourceFolder + "entities/";

	std::string getEntityFilePath(std::string_view name)
	{
		return sEntityFolder + name.data() + ".json";
	}

	void serializeEntity(ark::Entity entity)
	{
		json jsonEntity;

		auto& jsonComps = jsonEntity["components"];

		for (const auto component : entity.runtimeComponentView()) {
			auto mdata = ark::meta::getMetadata(component.type);
			if (auto serialize = mdata->func<nlohmann::json(const void*)>(serviceSerializeName)) {
				jsonComps[mdata->name] = serialize(component.ptr);
			}
		}

		std::ofstream of(getEntityFilePath(entity.getComponent<TagComponent>().name));
		of << jsonEntity.dump(4, ' ', true);
	}

	void deserializeEntity(ark::Entity entity)
	{
		json jsonEntity;
		std::ifstream fin(getEntityFilePath(entity.getComponent<TagComponent>().name));
		fin >> jsonEntity;

		// allocate components and default construct
		auto& jsonComps = jsonEntity.at("components");
		for (const auto& [compName, _] : jsonComps.items()) {
			const auto* mdata = ark::meta::getMetadata(compName);
			entity.addComponent(mdata->type);
		}
		// then initialize
		for (auto component : entity.runtimeComponentView()) {
			auto mdata = ark::meta::getMetadata(component.type);
			if (auto deserialize = mdata->func<void(Entity & e, const nlohmann::json&, void*)>(serviceDeserializeName)) {
				if (auto it = jsonComps.find(mdata->name); it != jsonComps.end())
					deserialize(entity, *it, component.ptr);
				else
					EngineLog(LogSource::Registry, LogLevel::Error, "deser-ing entity (%s) without component (%s)", 
						entity.getComponent<TagComponent>().name, mdata->name);
			}
		}
	}
}