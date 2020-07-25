#include <fstream>
#include "ark/ecs/SerializeDirector.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/Scene.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/util/ResourceManager.hpp"

namespace ark
{
	static inline const std::string sEntityFolder = Resources::resourceFolder + "entities/";

	std::string getEntityFilePath(std::string_view name)
	{
		return sEntityFolder + name.data() + ".json";
	}

	void SerdeJsonDirector::serializeEntity(Entity& entity)
	{
		auto& entityData = getEntityData(entity.getID());
		json jsonEntity;

		auto& jsonComps = jsonEntity["components"];

		for (auto compData : entityData.components) {
			if (auto serialize = ark::meta::getService<nlohmann::json(const void*)>(compData.type, serviceSerializeName)) {
				const auto* mdata = ark::meta::getMetadata(compData.type);
				jsonComps[mdata->name.data()] = serialize(compData.pComponent);
			}
		}

		std::ofstream of(getEntityFilePath(entity.getName()));
		of << jsonEntity.dump(4, ' ', true);
	}

	void SerdeJsonDirector::deserializeEntity(Entity& entity)
	{
		auto& entityData = getEntityData(entity.getID());
		json jsonEntity;
		std::ifstream fin(getEntityFilePath(entity.getName()));
		fin >> jsonEntity;

		// allocate components and default construct
		auto& jsonComps = jsonEntity.at("components");
		for (const auto& [compName, _] : jsonComps.items()) {
			const auto* mdata = ark::meta::getMetadata(compName);
			entity.addComponent(mdata->type);
		}
		// then initialize
		for (auto compData : entityData.components) {
			if (auto deserialize = ark::meta::getService<void(Scene&, Entity& e, const nlohmann::json&, void*)>(compData.type, serviceDeserializeName)) {
				const auto* mdata = ark::meta::getMetadata(compData.type);
				deserialize(this->scene, entity, jsonComps.at(mdata->name.data()), compData.pComponent);
			}
		}
	}
}