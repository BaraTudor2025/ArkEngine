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
	std::vector<sf::IntRect> hitBoxes;
	std::function<void(Entity&, Entity&)> onColide = nullptr;
};

struct Wall : public Data<Wall> {

};

class ColisionSystem : public System {

};

#endif

class MovePlayer : public Script {
	Animation* animation;
	Transform* transform;
	PixelParticles* runningParticles;
	float speed;
	float rotationSpeed;
public:

	MovePlayer(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) { }

	void init() {
		animation = getComponent<Animation>();

		transform = getComponent<Transform>();
		transform->setOrigin(animation->frameSize() / 2.f);
		transform->move(VectorEngine::center());
		transform->scale(0.15, 0.15);

		runningParticles = getComponent<PixelParticles>();
		auto pp = runningParticles;
		pp->spawn = true;
		pp->speed = this->speed;
		pp->emitter = transform->getPosition() + sf::Vector2f{ 50, 40 };
		pp->gravity = { 0, this->speed };
		auto[w, h] = VectorEngine::windowSize();
		pp->platform = { VectorEngine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };
	}

	void fixedUpdate(sf::Time frameTime) {
		bool moved = false;
		auto dt = frameTime.asSeconds();
		float dx = speed * dt;
		float angle = toRadians(transform->getRotation());

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			transform->move(dx * std::cos(angle), dx * std::sin(angle));
			runningParticles->angleDistribution = { PI + PI/4, PI / 10, DistributionType::normal };
			runningParticles->emitter = transform->getPosition() + sf::Vector2f{ -25, 40 };
			moved = true;
			animation->flipX = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
			runningParticles->angleDistribution = { -PI/4, PI / 10, DistributionType::normal };
			runningParticles->emitter = transform->getPosition() + sf::Vector2f{ 25, 40 };
			moved = true;
			animation->flipX = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
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

		if (moved) {
			animation->row = 0;
			animation->frameTime = sf::milliseconds(75);
			runningParticles->spawn = true;
		}
		else {
			animation->frameTime = sf::milliseconds(125);
			animation->row = 1;
			runningParticles->spawn = false;
		}
	}
};

/* 
 * TODO: Refactoring?
 * Entity won't be the owner of components
 * Components manage themselves (or systems manage them) in private static vectors
 * Entity has indexes to components
*/

//enum class GameTag { Bullet, Player, Wall };

/* TODO: TexturedParticles */
/* TODO: Entity.children : vector<Entity*> */
/* TODO: TileSystem */
/* TODO: GuiSystem, Button, Text(with fade), TextBox + remove systems from engine */
/* TODO: MusicSystem/SoundSystem */
/* TODO: de adaugat class Scene/World (manager de entitati) */

int main() // are nevoie de c++17 si SFML 2.5.1
{
	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	VectorEngine::create(VectorEngine::resolutionFourByThree, "Articifii!", sf::seconds(1/60.f), settings);
	VectorEngine::setVSync(false);
	VectorEngine::backGroundColor = sf::Color(50, 50, 50);

	VectorEngine::addSystem(new AnimationSystem());
	VectorEngine::addSystem(new FpsCounterSystem(sf::Color::White));
	VectorEngine::addSystem(new ParticleSystem());
	//VectorEngine::addSystem(new DebugEntitySystem());
	//VectorEngine::addSystem(new DebugParticleSystem());

	Entity player;
	player.addComponent<Transform>();
	player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
	player.addComponent<PixelParticles>(30, sf::seconds(1.5), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
	player.addScript<MovePlayer>(400, 180);
	player.Register();

	Entity image;
	image.addComponent<Transform>()->setPosition(VectorEngine::center());
	image.addComponent<Mesh>("toaleta.jpg", false);
	//image.Register();

	using namespace ParticleScripts;

	std::string prop("");
	//std::getline(std::cin, prop);
	std::vector<Entity> letters = makeLetterEntities(prop);
	registerEntities(letters);

	auto rainbowParticles = getRainbowParticles();
	auto fireParticles = getFireParticles();
	auto greenParticles = getGreenParticles();

	Entity rainbow;
	rainbow.addComponent<PointParticles>(rainbowParticles);
	rainbow.addScript<SpawnOnRightClick>();
	rainbow.addScript<EmittFromMouse>();

	Entity fire;
	fire.addComponent<PointParticles>(fireParticles);
	fire.addScript<SpawnOnLeftClick>();
	fire.addScript<EmittFromMouse>();

	Entity grass;
	auto grassP = grass.addComponent<PointParticles>(greenParticles);
	grassP->spawn = true;
	grass.addScript<DeSpawnOnMouseClick<>>();
	//grass.addScript<ReadColorFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail;
	trail.addComponent<PointParticles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();

	Entity plimbarica;
	plimbarica.addComponent<Transform>();
	auto plimb = plimbarica.addComponent<PointParticles>(rainbowParticles);
	plimb->spawn = true;
	//plimb->emitter = VectorEngine::center();
	//plimb->emitter.x += 100;
	//plimbarica.addScript<Rotate>(360/2, VectorEngine::center());
	plimbarica.addScript<RoatateEmitter>(360, VectorEngine::center(), 100);
	plimbarica.addScript<TraillingEffect>();
	//plimbarica.addScript<ReadColorFromConsole>();

	//grass.Register();
	plimbarica.Register();
	//trail.Register();
	//rainbow.Register();
	//fire.Register();

	auto fwEntities = makeFireWorksEntities(100, fireParticles, false);
	for (auto& e : fwEntities) {
		e.setAction(Action::SpawnLater, 10);
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
