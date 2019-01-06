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
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
#include "VectorEngine.hpp"
#include "Util.hpp"
using namespace std::literals;

#if 0
struct Animation : public Data<Animation> {
	int row;
	int columns;
	std::string fileName;
};

class AnimationSystem : public System {

public:
	void init() override {
		
	}
	void update() override
	{

	}

	void add(Component*) override
	{

	}

	virtual void render(sf::RenderWindow &) override
	{
	}

	virtual void remove(Component *) override
	{
	}

private:
	std::vector<Animation*> animationData;
};


struct Mesh : public Data<Mesh> {
	std::string fileName;
};

class MeshRendered : public System {

public:
	virtual void update() override
	{
	}
	virtual void render(sf::RenderWindow &) override
	{
	}
	virtual void add(Component *) override
	{
	}
	virtual void remove(Component *) override
	{
	}

private:
	std::vector<Mesh*> meshData;
};
#endif

std::vector<Entity> makeFireWorksEntity(int count, const Particles& templateParticles)
{
	std::vector<std::unique_ptr<Particles>> fireWorks;
	constructVector(fireWorks, count, templateParticles);

	for (auto& fw : fireWorks) {
		auto x = RandomNumber<float>(50, 750);
		auto y = RandomNumber<float>(50, 550);
		fw->count = 1000;
		fw->emitter = { x,y };
		fw->fireworks = true;
		fw->lifeTime = sf::seconds(RandomNumber<float>(2, 8));
		fw->speedDistribution.values = { 0, RandomNumber<int>(40, 70) };
		fw->speedDistribution.type = DistributionType::uniform;
	}

	std::vector<Entity> entities(count);
	for (auto i : range(0, count))
		entities[i].addComponent(std::move(fireWorks[i]));
	return entities;
}

/* 
 * TODO: Refactoring
 * Entity won't be the owner of components and scripts
 * Components and scripts manage themselves in their private static vectors
 * Entity has indexes to components and scripts
*/

int main() // are nevoie de c++17 si SFML 2.5.1
{
	VectorEngine::create(sf::VideoMode(800, 600), "Articifii!");

	auto rainbowParticles = getRainbowParticles();
	auto whiteParticles = getWhiteParticles();
	auto fireParticles = getFireParticles();
	auto greenParticles = getGreenParticles();

	using namespace ParticlesScripts;

	Entity letterM{ true }, letterU{ true }, letterI{ true }, letterE{ true };

	letterM.addComponentCopy(whiteParticles);
	letterU.addComponentCopy(whiteParticles);
	letterI.addComponent<Particles>(whiteParticles);
	letterE.addComponent<Particles>(whiteParticles);

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

	Entity grass{ false };
	greenParticles.spawn = true;
	grass.Register();
	grass.addComponent<Particles>(greenParticles);
	grass.addScript<DeSpawnOnMouseClick<>>();
	grass.addScript<ModifyColorsFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail{ false };
	trail.addComponent<Particles>(whiteParticles);
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();
	trail.Register();

	auto fwEntities = makeFireWorksEntity(10, rainbowParticles);
	for (auto& e : fwEntities) {
		e.addScript<SpawnLater>(5);
		e.Register();
	}

	ParticleSystem::gravityVector = { 0, 0 };

	bool reg = true;

	//std::ifstream fin("./res/colors.txt");
	//if (!fin.is_open())
	//	throw std::runtime_error("");
	//auto grassP = grass.getComponent<Particles>();
	//std::thread modifyVarsThread([&]() {
	//	while (true) {
	//		std::cin.get();
	//		grassP->getColor = []() { return sf::Color::White; };
	//		//std::cout << " mama";
	//		//int r1, r2, g1, g2, b1, b2;
	//		//fin >> r1 >> r2 >> g1 >> g2 >> b1 >> b2;
	//		//greenParticles.getColor = [=]() {
	//		//	auto r = RandomNumber<int>(r1, r2);
	//		//	auto g = RandomNumber<int>(g1, g2);
	//		//	auto b = RandomNumber<int>(b1, b2);
	//		//	return sf::Color::White;
	//		//};
	//	}
	//});
	//modifyVarsThread.detach();

	VectorEngine::addSystem(&ParticleSystem::instance);
	VectorEngine::run();
	
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