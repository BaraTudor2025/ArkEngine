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
#include "ParticleSystem.hpp"
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
#include "VectorEngine.hpp"
#include "Util.hpp"
#include "Entities.hpp"
#include "Scripts.hpp"
#include "AnimationSystem.hpp"

using namespace std::literals;

#if 0
struct RigidBody : public Data<RigidBody> {

};

class ColisionSystem : public System {

};

struct Mesh : public Data<Mesh> {
	std::string fileName;
};

class MeshSystem : public System {

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
	std::vector<sf::Texture*> textures;
};
#endif

/* 
 * TODO: Refactoring
 * Entity won't be the owner of components and scripts
 * Components and scripts manage themselves in their private static vectors
 * Entity has indexes to components and scripts
*/

/* TODO: de adaugat class Scene/World (manager de entitati) */

class MovePlayer : public Script {
	Animation* animation;
	Transform* transform;
	float speed;
	float rotationSpeed;
public:

	MovePlayer(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) { }
	void init() {
		animation = getComponent<Animation>();
		transform = getComponent<Transform>();
		animation->flipped = true;
	}

	void fixedUpdate(sf::Time frameTime) {
		bool moved = false;
		auto dt = frameTime.asSeconds();
		float dx = speed * dt;
		float angle = toRadians(transform->getRotation());

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			transform->move(dx * std::cos(angle), dx * std::sin(angle));
			moved = true;
			animation->flipped = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
			moved = true;
			animation->flipped = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
			//transform->move(dx * std::cos(PI/2 - angle), dx * std::sin(angle - PI/2));
			transform->move(dx * std::sin(angle), -dx * std::cos(angle));
			moved = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
			transform->move(-dx * std::sin(angle), dx * std::cos(angle));
			moved = true;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
			transform->rotate(rotationSpeed * dt);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::E))
			transform->rotate(-rotationSpeed * dt);

		if (moved)
			animation->row = 1;
		else
			animation->row = 0;
	}

};

class FpsCounter : public Script {
	sf::Time fixedUpdateElapsed;
	sf::Time updateElapsed;
	int fixedFPS;
	int updateFPS;
	bool measureFixedUpdate;
	bool measureUpdate;

public:
	FpsCounter(bool measureUpdate, bool measureFixedUpdate)
		:measureFixedUpdate(measureFixedUpdate), 
		measureUpdate(measureUpdate) { }

	void update() {
		if (measureUpdate) {
			updateElapsed += VectorEngine::deltaTime();
			updateFPS += 1;
			if (updateElapsed.asMilliseconds() >= 1000) {
				updateElapsed -= sf::milliseconds(1000);
				log("fps: %d", updateFPS);
				updateFPS = 0;
			}
		}
	}

	void fixedUpdate(sf::Time dt) {
		if (measureFixedUpdate) {
			fixedUpdateElapsed += dt;
			fixedFPS += 1;
			if (fixedUpdateElapsed.asMilliseconds() >= 1000) {
				fixedUpdateElapsed -= sf::milliseconds(1000);
				log("fps: %d", fixedFPS);
				fixedFPS = 0;
			}
		}
	}
};

int main() // are nevoie de c++17 si SFML 2.5.1
{

	sf::ContextSettings settings;
	settings.antialiasingLevel = 16;

	VectorEngine::create(VectorEngine::resolutionFourByThree, "Articifii!", sf::seconds(1/60.f),settings);
	VectorEngine::backGroundColor = sf::Color(50, 50, 50);

	VectorEngine::addSystem(new AnimationSystem());
	VectorEngine::addSystem(new ParticleSystem());
	
	Entity fpsCounter;
	fpsCounter.addScript<FpsCounter>(true, true);
	fpsCounter.Register();

	Entity player;
	player.addComponent<Transform>()->setPosition(VectorEngine::center());
	player.addComponent<Animation>("tux_from_linux.png", sf::Vector2u{3, 9}, sf::milliseconds(200), 0);
	player.addScript<MovePlayer>(200, 360);
	player.Register();

	using namespace ParticleScripts;

	std::string prop("");
	//std::getline(std::cin, prop);
	std::vector<Entity> letters = makeLetterEntities(prop);
	registerEntities(letters);

	auto rainbowParticles = getRainbowParticles();
	auto fireParticles = getFireParticles();
	auto greenParticles = getGreenParticles();

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
	//grass.addScript<ReadColorFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail{ false };
	trail.addComponent<Particles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal );
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();

	Entity plimbarica;
	//plimbarica.addComponent<Transform>();
	plimbarica.addComponent<Particles>(rainbowParticles);
	//plimbarica.addScript<RotateParticles>(4 * 360);
	//plimbarica.addScript<TraillingEffect>();
	//plimbarica.addScript<Rotate>(360/2, VectorEngine::center());
	plimbarica.getComponent<Particles>()->spawn = true;
	//pp->spawn = true;
	//pp->emitter = VectorEngine::center();

	//plimbarica.Register();
	grass.Register();
	fire.Register();
	trail.Register();
	rainbow.Register();

	auto fwEntities = makeFireWorksEntities(10, rainbowParticles);
	for (auto& e : fwEntities) {
		e.addScript<SpawnLater>(5);
		e.Register();
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

	VectorEngine::run();
	
	return 0;
}
