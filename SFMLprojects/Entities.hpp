#pragma once
#include <vector>
#include "VectorEngine.hpp"
#include "Particles.hpp"
#include "Util.hpp"
#include "ParticleScripts.hpp"

static std::pair<Entity, Entity> makeMouseRecordingEntities(std::string fileToSave)
{
	using namespace ParticlesScripts;

	auto letterParticles = getWhiteParticles(200, 10);
	Entity letter, whiteP;

	letter.addComponent<Particles>(letterParticles);
	letter.addScript<EmittFromMouse>();
	letter.addScript<RegisterMousePath>(fileToSave);
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
	auto letterParticles = getWhiteParticles(200, 10);
	for (auto& cuvant : cuvinte) {
		col = 0;
		for (auto litera : cuvant) {
			letters[i].addComponent<Particles>(letterParticles);
			letters[i].addScript<ParticlesScripts::PlayModel>("./res/litere/letter"s + litera + ".txt"s, sf::Vector2f{ 150.f * col, 150.f * rand });
			i++;
			col++;
		}
		rand++;
	}
	return letters;
}

static std::vector<Entity> makeFireWorksEntities(int count, const Particles& templateParticles)
{
	std::vector<std::unique_ptr<Particles>> fireWorks;
	constructVector(fireWorks, count, templateParticles);

	for (auto& fw : fireWorks) {
		auto[width, height] = VectorEngine::windowSize();
		auto x = RandomNumber<float>(50, width - 50);
		auto y = RandomNumber<float>(50, height - 50);
		fw->count = 1000;
		fw->emitter = { x,y };
		fw->fireworks = true;
		fw->lifeTime = sf::seconds(RandomNumber<float>(2, 8));
		fw->speedDistribution.values = { 0, RandomNumber<int>(40, 70) };
		fw->speedDistribution.type = DistributionType::uniform;
	}

	return makeEntitiesFromComponents<Particles>(std::move(fireWorks));
}

static std::vector<Entity> makeRandomParticlesFountains(int count, float life, const Particles& templateParticles)
{
	std::vector<std::unique_ptr<Particles>> particles;
	constructVector(particles, count, templateParticles);

	for (auto& ps : particles) {
		auto[width, height] = VectorEngine::windowSize();
		auto x = RandomNumber<float>(50, width - 50);
		auto y = RandomNumber<float>(50, height - 50);
		ps->spawn = true;
		ps->count = 1000;
		ps->emitter = { x,y };
		ps->lifeTime = sf::seconds(2);
		ps->lifeTimeDistribution =
		{ { ps->lifeTime.asMilliseconds() / 4, ps->lifeTime.asMilliseconds() },
			DistributionType::uniform };
		ps->speedDistribution.values = { 50, 50 };
		ps->speedDistribution.type = DistributionType::normal;
	}

	return makeEntitiesFromComponents(std::move(particles));
}
