#include <SFML/Graphics.hpp>
#include <SFML/System/String.hpp>
#include <functional>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
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

std::vector<Entity> makeRandomParticlesFountains(int count, const Particles& templateParticles)
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
		ps->lifeTime = sf::seconds(1.3);
		ps->lifeTimeDistribution =
		{ { ps->lifeTime.asMilliseconds() / 4, ps->lifeTime.asMilliseconds() },
			DistributionType::uniform };
		ps->speedDistribution.values = { 0, 100 };
		ps->speedDistribution.type = DistributionType::uniform;
	}

	return makeEntitiesFromComponents(std::move(particles));
}

std::vector<Entity> makeFireWorksEntities(int count, const Particles& templateParticles)
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

/* 
 * TODO: Refactoring
 * Entity won't be the owner of components and scripts
 * Components and scripts manage themselves in their private static vectors
 * Entity has indexes to components and scripts
*/

std::vector<std::string> splitOnSpace(std::string string)
{
	std::stringstream ss(string);
	std::vector<std::string> v;
	std::string s;
	while (ss >> s)
		v.push_back(s);
	return v;
}

int main() // are nevoie de c++17 si SFML 2.5.1
{
	VectorEngine::create(sf::VideoMode(1920, 1080), "Articifii!");

	//auto rainbowParticles = getRainbowParticles();
	auto whiteParticles = getWhiteParticles(200, 10);
	//auto fireParticles = getFireParticles();
	//auto greenParticles = getGreenParticles();

	using namespace ParticlesScripts;

	std::string prop;
	std::getline(std::cin, prop);
	auto cuvinte = splitOnSpace(prop);
	std::vector<Entity> letters(prop.size() - std::count(prop.begin(), prop.end(), ' '));
	auto litera = cuvinte.begin();
	int i = 0;
	int rand = 0;
	int col = 0;
	for (auto& cuvant : cuvinte) {
		col = 0;
		for (auto litera : cuvant) {
			letters[i].addComponent<Particles>(whiteParticles);
			letters[i].addScript<ParticlesScripts::PlayModel>("./res/litere/letter"s + litera + ".txt"s, sf::Vector2f{ 150.f * col, 150.f * rand });
			letters[i].Register();
			i++;
			col++;
		}
		rand++;
	}

	//Entity letter(true), whiteP(true);

	//letter.addComponent<Particles>(whiteParticles);
	//letter.addScript<EmittFromMouse>();
	//letter.addScript<RegisterMousePath>("./res/litere/letterZ.txt");
	//letter.addScript<SpawnOnLeftClick>();

	//whiteP.addComponent<Particles>(whiteParticles);
	//whiteP.addScript<EmittFromMouse>();
	//whiteP.addScript<SpawnOnRightClick>();

	//Entity rainbow{ false };
	//rainbow.addComponent<Particles>(rainbowParticles);
	//rainbow.addScript<SpawnOnRightClick>();
	//rainbow.addScript<EmittFromMouse>();

	//Entity fire{ false };
	//fire.addComponent<Particles>(fireParticles);
	//fire.addScript<SpawnOnLeftClick>();
	//fire.addScript<EmittFromMouse>();

	//Entity grass{ false };
	//greenParticles.spawn = true;
	//grass.addComponent<Particles>(greenParticles);
	//grass.addScript<DeSpawnOnMouseClick<>>();
	//grass.addScript<ModifyColorsFromConsole>();
	//grass.addScript<EmittFromMouse>();

	//Entity trail{ false };
	//trail.addComponent<Particles>(whiteParticles);
	//trail.addScript<EmittFromMouse>();
	//trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	//trail.addScript<TraillingEffect>();

	//auto fwEntities = makeFireWorksEntities(10, rainbowParticles);
	//for (auto& e : fwEntities) {
	//	e.addScript<SpawnLater>(5);
	//	//e.Register();
	//}

	//auto randomParticles = makeRandomParticlesFountains(1, rainbowParticles);
	//for (auto& r : randomParticles) {
	//	//r.Register();
	//}

	//sf::Transform t;

	ParticleSystem::hasUniversalGravity = true;
	ParticleSystem::gravityVector = { 0, 0 };
	ParticleSystem::gravityMagnitude = 100;
	ParticleSystem::gravityPoint = static_cast<sf::Vector2f>(VectorEngine::windowSize()) / 2.f;

	bool reg = true;

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