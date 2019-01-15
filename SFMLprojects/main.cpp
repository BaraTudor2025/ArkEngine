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
#include "Entities.hpp"
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

/* 
 * TODO: Refactoring
 * Entity won't be the owner of components and scripts
 * Components and scripts manage themselves in their private static vectors
 * Entity has indexes to components and scripts
*/

template <typename T>
class ReadVarFromConsole : public Script {
	T* var;
	std::string prompt;
public:
	ReadVarFromConsole(T* var, std::string p) : prompt(p), var(var) { }

	void init()
	{
		std::thread t([prompt = prompt, &var = *var]() {
			while (true) {
				std::cout << prompt;
				std::cin >> var;
				std::cout << std::endl;
			}
		});
		t.detach();
		this->seppuku();
	}
};

int main() // are nevoie de c++17 si SFML 2.5.1
{
	auto fullHD = sf::VideoMode(1920, 1080);
	auto normalHD = sf::VideoMode(1280, 720);
	auto fourByThree = sf::VideoMode(1024, 768);
	VectorEngine::create(fourByThree, "Articifii!");

	using namespace ParticlesScripts;

	std::string prop("");
	std::getline(std::cin, prop);
	std::vector<Entity> letters = makeLetterEntities(prop);
	registerEntities(letters);

	auto rainbowParticles = getRainbowParticles();
	auto fireParticles = getFireParticles();
	auto greenParticles = getGreenParticles();
	auto whiteParticles = getWhiteParticles();

	Entity rainbow{ false };
	rainbow.addComponent<Particles>(rainbowParticles);
	rainbow.addScript<SpawnOnRightClick>();
	rainbow.addScript<EmittFromMouse>();

	Entity fire{ false };
	fire.addComponent<Particles>(fireParticles);
	fire.addScript<SpawnOnLeftClick>();
	fire.addScript<EmittFromMouse>();

	Entity grass{ false };
	greenParticles.spawn = true;
	grass.addComponent<Particles>(greenParticles);
	grass.addScript<DeSpawnOnMouseClick<>>();
	grass.addScript<ModifyColorsFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail{ false };
	trail.addComponent<Particles>(whiteParticles);
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();

	//grass.Register();
	//fire.Register();
	//trail.Register();
	//rainbow.Register();

	auto fwEntities = makeFireWorksEntities(10, rainbowParticles);
	for (auto& e : fwEntities) {
		e.addScript<SpawnLater>(5);
		//e.Register();
	}

	auto randomParticles = makeRandomParticlesFountains(50, 1.f, rainbowParticles);
	//registerEntities(randomParticles);

	struct UpdateGravityPoint : public Script {
		void update()
		{
			ParticleSystem::gravityPoint = VectorEngine::mousePositon();
		}
	};

	Entity updateGP;
	updateGP.addScript<UpdateGravityPoint>();
	Entity readGMag;
	readGMag.addScript<ReadVarFromConsole<float>>(&ParticleSystem::gravityMagnitude, "enter gravity magnitude: ");

	//readGMag.Register();
	//updateGP.Register();

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