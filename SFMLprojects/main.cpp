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
#include "DebugSystems.hpp"

using namespace std::literals;

#if 0
struct HitBox : public Data<HitBox> {

};

class ColisionSystem : public System {

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
	}

	void fixedUpdate(sf::Time frameTime) {
		bool moved = false;
		auto dt = frameTime.asSeconds();
		float dx = speed * dt;
		float angle = toRadians(transform->getRotation());

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			transform->move(dx * std::cos(angle), dx * std::sin(angle));
			moved = true;
			animation->flipX = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
			moved = true;
			animation->flipX = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
			transform->move(dx * std::sin(angle), -dx * std::cos(angle));
			moved = true;
			animation->flipY = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
			transform->move(-dx * std::sin(angle), dx * std::cos(angle));
			moved = true;
			animation->flipY = false;
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

// adaugat struct TexturedParticles : (?) public PointParticles, Data<TexturedParticles>

int main() // are nevoie de c++17 si SFML 2.5.1
{
	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	VectorEngine::create(VectorEngine::resolutionFourByThree, "Articifii!", sf::seconds(1/60.f), settings);
	VectorEngine::setVSync(false);
	VectorEngine::backGroundColor = sf::Color(50, 50, 50);

	VectorEngine::addSystem(new AnimationSystem());
	//VectorEngine::addSystem(new ParticleSystem());
	VectorEngine::addSystem(new FpsCounterSystem());
	//VectorEngine::addSystem(new DebugEntitySystem());
	//VectorEngine::addSystem(new DebugParticleSystem());

	Entity player;
	player.addComponent<Transform>()->setPosition(VectorEngine::center());
	player.addComponent<Animation>("tux_from_linux.png", sf::Vector2u{3, 9}, sf::milliseconds(200), 0, false);
	player.addScript<MovePlayer>(200, 360);
	player.Register();

	Entity image;
	image.addComponent<Transform>()->setPosition(VectorEngine::center());
	image.addComponent<Mesh>("toaleta.jpg", false);
	image.Register();

	using namespace ParticleScripts;

	std::string prop("");
	//std::getline(std::cin, prop);
	std::vector<Entity> letters = makeLetterEntities(prop);
	registerEntities(letters);

	auto rainbowParticles = getRainbowParticles();
	auto fireParticles = getFireParticles();
	auto greenParticles = getGreenParticles();

	Entity rainbow;
	rainbow.addComponent<Particles>(rainbowParticles);
	rainbow.addScript<SpawnOnRightClick>();
	rainbow.addScript<EmittFromMouse>();

	Entity fire;
	fire.addComponent<Particles>(fireParticles);
	fire.addScript<SpawnOnLeftClick>();
	fire.addScript<EmittFromMouse>();

	Entity grass;
	auto grassP = grass.addComponent<Particles>(greenParticles);
	grassP->spawn = true;
	grass.addScript<DeSpawnOnMouseClick<>>();
	//grass.addScript<ReadColorFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail;
	trail.addComponent<Particles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();

	Entity plimbarica;
	//plimbarica.addComponent<Transform>();
	auto plimb = plimbarica.addComponent<Particles>(rainbowParticles);
	plimb->spawn = true;
	//plimb->emitter = VectorEngine::center();
	//plimb->emitter.x += 100;
	//plimbarica.addScript<Rotate>(360/2, VectorEngine::center());
	plimbarica.addScript<RoatateEmitter>(360, VectorEngine::center(), 100);
	plimbarica.addScript<TraillingEffect>();
	//plimbarica.addScript<ReadColorFromConsole>();

	grass.Register();
	plimbarica.Register();
	trail.Register();
	rainbow.Register();
	fire.Register();

	auto fwEntities = makeFireWorksEntities(100, fireParticles, false);
	for (auto& e : fwEntities) {
		e.addScript<SpawnLater>(20);
		//e.Register();
	}

	auto randomParticles = makeRandomParticlesFountains(50, 5.f, getGreenParticles(), false);
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

	//updateGP.Register();
	//readGMag.Register();

	ParticleSystem::hasUniversalGravity = true;
	ParticleSystem::gravityVector = { 0, 0 };
	ParticleSystem::gravityMagnitude = 100;
	ParticleSystem::gravityPoint = VectorEngine::center();

	VectorEngine::run();
	
	return 0;
}
