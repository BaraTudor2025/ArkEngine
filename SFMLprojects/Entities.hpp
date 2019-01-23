#pragma once
#include <vector>
#include "VectorEngine.hpp"
#include "ParticleSystem.hpp"
#include "Util.hpp"
#include "ParticleScripts.hpp"
#include "Scripts.hpp"

static std::pair<Entity, Entity> makeMouseRecordingEntities(std::string fileToSave)
{
	using namespace ParticleScripts;

	Particles letterParticles(200, sf::seconds(10), { 0, 2 });
	Entity letter, whiteP;

	letter.addComponent<Particles>(letterParticles);
	letter.addScript<EmittFromMouse>();
	letter.addScript<GeneralScripts::RegisterMousePath>(fileToSave);
	letter.addScript<SpawnOnLeftClick>();

	whiteP.addComponent<Particles>(letterParticles);
	whiteP.addScript<EmittFromMouse>();
	whiteP.addScript<SpawnOnRightClick>();

	return { std::move(letter), std::move(whiteP) };
}

static std::vector<Entity> makeLetterEntities(std::string prop)
{
	using namespace std::literals;
	std::vector<Entity> letters(prop.size() - std::count(prop.begin(), prop.end(), ' '));
	auto cuvinte = splitOnSpace(prop);
	auto litera = cuvinte.begin();
	int i = 0;
	int rand = 0;
	int col = 0;
	Particles letterParticles(200, sf::seconds(10), { 0, 2 });
	for (auto& cuvant : cuvinte) {
		col = 0;
		for (auto litera : cuvant) {
			letters[i].addComponent<Particles>(letterParticles);
			letters[i].addScript<ParticleScripts::PlayModel>("./res/litere/letter"s + litera + ".txt"s, sf::Vector2f{ 150.f * col, 150.f * rand });
			i++;
			col++;
		}
		rand++;
	}
	return letters;
}

static std::vector<Entity> makeFireWorksEntities(int count, const Particles& templateParticles, bool spawn)
{
	std::vector<std::unique_ptr<Particles>> fireWorks;
	constructVector(fireWorks, count, templateParticles);

	for (auto& fw : fireWorks) {
		auto[width, height] = VectorEngine::windowSize();
		float x = RandomNumber<int>(50, width - 50);
		float y = RandomNumber<int>(50, height - 50);
		fw->count = 1000;
		fw->emitter = { x,y };
		fw->spawn = spawn;
		fw->fireworks = true;
		fw->getColor = makeColorsVector[RandomNumber<size_t>(0, makeColorsVector.size() - 1)];
		fw->lifeTime = sf::seconds(RandomNumber<float>(2, 8));
		fw->speedDistribution = { 0, RandomNumber<float>(40, 70), DistributionType::uniform };
	}

	return makeEntitiesFromComponents<Particles>(std::move(fireWorks));
}

static std::vector<Entity> makeRandomParticlesFountains(int count, float life, const Particles& templateParticles, bool spawn)
{
	std::vector<std::unique_ptr<Particles>> particles;
	constructVector(particles, count, templateParticles);

	for (auto& ps : particles) {
		auto[width, height] = VectorEngine::windowSize();
		float x = RandomNumber<int>(50, width - 50);
		float y = RandomNumber<int>(50, height - 50);
		ps->spawn = spawn;
		ps->count = 1000;
		ps->emitter = { x,y };
		ps->lifeTime = sf::seconds(2);
		ps->makeLifeTimeDistUniform();
		ps->speedDistribution = { 0, 100 , DistributionType::uniform};
	}

	return makeEntitiesFromComponents(std::move(particles));
}
