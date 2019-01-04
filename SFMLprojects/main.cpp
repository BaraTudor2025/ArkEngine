#include <SFML/Graphics.hpp>
#include <SFML/System/String.hpp>
#include <functional>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
#include <thread>
#include "Particles.hpp"
#include "Util.hpp"
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
#include "VectorEngine.hpp"
using namespace std::literals;

Entity makeFireWorksEntity(int count, const Particles& particlesTemplate)
{
	std::vector<Particles> fireWorks(count, particlesTemplate);
	
	for (auto& fw : fireWorks) {
		auto x = RandomNumber<float>(50, 750);
		auto y = RandomNumber<float>(50, 550);
		fw.count = 1000;
		fw.emitter = { x,y };
		fw.fireworks = true;
		fw.lifeTime = sf::seconds(RandomNumber<float>(2, 8));
		fw.speedDistribution.values = { 0, RandomNumber<int>(40, 70) };
		fw.speedDistribution.type = DistributionType::uniform;
	}

	Entity fireWorksEntity;
	fireWorksEntity.addComponents(fireWorks);
	return fireWorksEntity;
}

int main() // are nevoie de c++17 si SFML 2.5.1
{
	VectorEngine game(sf::VideoMode(800, 600), "Articifii!");

	Particles fireParticles(1'000, sf::seconds(2), { 2, 100 }, { 0, 2 * PI }, DistributionType::uniform);
	fireParticles.speedDistribution.type = DistributionType::uniform;
	fireParticles.getColor = []() {
		auto green = RandomNumber<uint32_t>(0, 150);
		return sf::Color(255, green, 0);
	};

	Particles whiteParticles(1000, sf::seconds(6), { 0, 5 });
	whiteParticles.speedDistribution.type = DistributionType::uniform;
	whiteParticles.angleDistribution.type = DistributionType::uniform;

	Particles rainbowParticles(2000, sf::seconds(3), { 0, 100 }, {0,2*PI}, DistributionType::uniform);
	rainbowParticles.getColor = []() {
		return sf::Color(RandomNumber<uint32_t>(0x000000ff, 0xffffffff));
	};

	Entity letterM{ true }, letterU{ true }, letterI{ true }, letterE{ true };

	letterM.addComponentCopy(whiteParticles);
	letterU.addComponentCopy(whiteParticles);
	letterI.addComponent<Particles>(whiteParticles);
	letterE.addComponent<Particles>(whiteParticles);

	using namespace ParticlesScripts;
	letterM.addScript<PlayModel>("./res/litere/letterM.txt");
	letterU.addScript<PlayModel>("./res/litere/letterU.txt");
	letterI.addScript<PlayModel>("./res/litere/letterI.txt");
	letterE.addScript<PlayModel>("./res/litere/letterE.txt");

	Entity rainbow{ true };
	rainbow.addComponent<Particles>(rainbowParticles);
	rainbow.addScript<SpawnOnRightClick>();
	rainbow.addScript<EmittFromMouse>();

	Entity fire{ true };
	fire.addComponent<Particles>(fireParticles);
	fire.addScript<SpawnOnLeftClick>();
	fire.addScript<EmittFromMouse>();
	
	// TODO: de parametrizat sistemele

	Entity trail{ false };
	trail.Register();
	trail.addComponent<Particles>(whiteParticles);
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();
	trail.addScript<EmittFromMouse>();

	auto fireWorksEntity = std::make_unique<Entity>(makeFireWorksEntity(30, rainbowParticles));
	fireWorksEntity->addScript<SpawnLater>(5);
	fireWorksEntity->Register();

	ParticleSystem::gravityVector = { 0, 0 };

	//std::thread modifyVarsThread([&]() {
	//	auto&[x, y] = ParticleSystem::gravityVector;
	//	while (true) {
	//		std::cout << "enter gravity x and y: ";
	//		std::cin >> x >> y;
	//		std::cout << std::endl;
	//		//fireWorksEntity->addScript<SpawnParticlesLater>(5);
	//	}
	//});
	//modifyVarsThread.detach();

	game.addSystem(&ParticleSystem::instance);
	game.run();
	
	return 0;
}

int mainRandom()
{
	std::map<int, int> nums;
	auto args = std::pair(0.f, 2*PI);
	int n = 100'000;
	for (int i = 0; i < n; i++) {
		auto num = RandomNumber(args, DistributionType::normal);
		nums[std::round(num)]++;
	}
	for (auto [num, count] : nums) {
		std::cout << num << ':' << count << '\n';
	}
	return 0;
}