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
#include "Scripts.hpp"
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

	virtual void render(sf::RenderTarget& target) override
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
	virtual void render(sf::RenderTarget& target) override
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


class RotateParticles : public Script {
	sf::Transform t;
	Particles* p;
	sf::Vector2f offset;
	sf::Vector2f rotateAroundThis;
	float angle;

public:
	RotateParticles(float a) :angle(a) { }

	void init()
	{
		p = getComponent<Particles>();
		rotateAroundThis = VectorEngine::center();
		offset =  p->emitter - rotateAroundThis;
	}

	void update()
	{
		t.rotate(angle * VectorEngine::deltaTime().asSeconds(), rotateAroundThis);
		p->emitter = t.transformPoint(rotateAroundThis + offset);
	}
};

class Rotate : public Script {
	sf::Transform* t;
	float angle;
	sf::Vector2f around;
public:
	Rotate(float a, sf::Vector2f around) : angle(a), around(around) { }
	void init()
	{
		t = getComponent<sf::Transform>();
		auto p = getComponent<Particles>();
		p->emitter = around;
		p->applyTransform = true;
	}
	void update()
	{
		//Particles::components.clear();
		t->rotate(angle * VectorEngine::deltaTime().asSeconds(), around);
	}
};

//#define VENGINE_BUILD_DLL 0
//
//#if VENGINE_BUILD_DLL
//	#define VECTOR_ENGINE_API __declspec(dllexport)
//#else
//	#define VECTOR_ENGINE_API __declspec(dllimport)
//#endif
#define VECTOR_ENGINE_API

// TODO: de inlocuit sf::Tranform cu asta
//struct VECTOR_ENGINE_API Transform : public Data<Transform>, sf::Transformable { };

/* TODO: de adaugat class Scene/World (manager de entitati) */
/* TODO: de facut un script ReadColorDistributionFromConsole & ReadColorFromConsole */

int main() // are nevoie de c++17 si SFML 2.5.1
{
	auto fullHD = sf::VideoMode(1920, 1080);
	auto normalHD = sf::VideoMode(1280, 720);
	auto fourByThree = sf::VideoMode(1024, 768);
	VectorEngine::create(fourByThree, "Articifii!");

	using namespace ParticleScripts;

	std::string prop("");
	//std::getline(std::cin, prop);
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
	grass.addScript<ReadColorFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail{ false };
	trail.addComponent<Particles>(whiteParticles);
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();

	Entity plimbarica;
	plimbarica.addComponent<Particles>(rainbowParticles);
	//plimbarica.addScript<RotateParticles>(4 * 360);
	//plimbarica.addScript<TraillingEffect>();
	plimbarica.addScript<Rotate>(360/2, VectorEngine::center());
	plimbarica.getComponent<Particles>()->spawn = true;
	//pp->spawn = true;
	//pp->emitter = VectorEngine::center();
	plimbarica.Register();

	grass.Register();
	fire.Register();
	trail.Register();
	rainbow.Register();

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
	readGMag.addScript<GeneralScripts::ReadVarFromConsole<float>>(&ParticleSystem::gravityMagnitude, "enter gravity magnitude: ");

	//readGMag.Register();
	//updateGP.Register();

	ParticleSystem::hasUniversalGravity = true;
	ParticleSystem::gravityVector = { 0, 0 };
	ParticleSystem::gravityMagnitude = 100;
	ParticleSystem::gravityPoint = VectorEngine::center();

	bool reg = true;

	VectorEngine::addSystem(&ParticleSystem::instance);
	VectorEngine::run();
	
	return 0;
}
