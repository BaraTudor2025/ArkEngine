#include <fstream>
#include "ark/ecs/SerdeJsonDirector.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/EntityManager.hpp"
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

		for (const RuntimeComponent component : entity.eachComponent()) {
			auto mdata = ark::meta::resolve(component.type);
			if (auto serialize = mdata->func<nlohmann::json(const void*)>(serviceSerializeName)) {
				jsonComps[mdata->name] = serialize(component.ptr);
			}
		}

		std::ofstream of(getEntityFilePath(entity.get<TagComponent>().name));
		of << jsonEntity.dump(4, ' ', true);
	}

	void deserializeEntity(ark::Entity entity)
	{
		json jsonEntity;
		std::ifstream fin(getEntityFilePath(entity.get<TagComponent>().name));
		fin >> jsonEntity;

		// allocate components and default construct
		auto& jsonComps = jsonEntity.at("components");
		for (const auto& [compName, _] : jsonComps.items()) {
			const auto* mdata = ark::meta::resolve(compName);
			entity.add(mdata->type);
		}

		// then initialize
		for (auto component : entity.eachComponent()) {
			auto mdata = ark::meta::resolve(component.type);
			if (auto deserialize = mdata->func<void(Entity&, const nlohmann::json&, void*)>(serviceDeserializeName)) {
				if (auto it = jsonComps.find(mdata->name); it != jsonComps.end())
					deserialize(entity, *it, component.ptr);
				else
					EngineLog(LogSource::Registry, LogLevel::Error, "deser-ing entity (%s) without component (%s)", 
						entity.get<TagComponent>().name, mdata->name);
			}
		}
	}
}