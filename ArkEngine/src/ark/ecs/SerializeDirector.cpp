#include <fstream>
#include "ark/ecs/SerializeDirector.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Scene.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/util/ResourceManager.hpp"

namespace ark
{
	static inline const std::string sEntityFolder = Resources::resourceFolder + "entities/";

	std::string getEntityFilePath(Entity e)
	{
		return sEntityFolder + e.getName() + ".json";
	}

	void SerializeDirector::serializeEntity(Entity& entity)
	{
		auto& entityData = getEntityData(entity.getID());
		json jsonEntity;

		auto& jsonComps = jsonEntity["components"];

		for (auto compData : entityData.components) {
			auto compType = scene.componentTypeFromId(compData.id);
			if (auto serialize = ark::meta::getService<nlohmann::json(const void*)>(compType, serviceSerializeName)) {
				const auto* mdata = ark::meta::getMetadata(compType);
				jsonComps[mdata->name.data()] = serialize(compData.component);
			}
		}

		std::ofstream of(getEntityFilePath(entity));
		of << jsonEntity.dump(4, ' ', true);
	}

	void SerializeDirector::deserializeEntity(Entity& entity)
	{
		auto& entityData = getEntityData(entity.getID());
		json jsonEntity;
		std::ifstream fin(getEntityFilePath(entity));
		fin >> jsonEntity;

		// allocate components and default construct
		auto& jsonComps = jsonEntity.at("components");
		for (const auto& [compName, _] : jsonComps.items()) {
			const auto* mdata = ark::meta::getMetadata(compName);
			entity.addComponent(mdata->type);
		}
		// then initialize
		for (auto compData : entityData.components) {
			const auto compType = scene.componentTypeFromId(compData.id);
			if (auto deserialize = ark::meta::getService<void(const nlohmann::json&, void*)>(compType, serviceDeserializeName)) {
				const auto* mdata = ark::meta::getMetadata(compType);
				deserialize(jsonComps.at(mdata->name.data()), compData.component);
			}
		}
	}
}