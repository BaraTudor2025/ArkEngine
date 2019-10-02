
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

#include "Engine.hpp"
#include "Scene.hpp"
#include "ParticleSystem.hpp"
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
#include "Util.hpp"
#include "Scripts.hpp"
#include "AnimationSystem.hpp"
#include "DebugSystems.hpp"
#include "GuiSystem.hpp"

using namespace std::literals;

#if 0

struct HitBox : public Component<HitBox> {
	std::vector<sf::IntRect> hitBoxes;
	std::function<void(Entity&, Entity&)> onColide = nullptr;
	std::optional<int> mass; // if mass is undefined object is imovable
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
	sf::Vector2f particleEmitterOffsetLeft;
	sf::Vector2f particleEmitterOffsetRight;
public:

	MovePlayer(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) { }

	void scale(float factor)
	{
		transform->scale(factor, factor);
		particleEmitterOffsetLeft = particleEmitterOffsetLeft * transform->getScale();
		particleEmitterOffsetRight = particleEmitterOffsetRight * transform->getScale();
	}

	void init() override {
		animation = getComponent<Animation>();

		transform = getComponent<Transform>();
		transform->setOrigin(animation->frameSize() / 2.f);
		transform->move(ArkEngine::center());

		particleEmitterOffsetLeft = { -165, 285 };
		{
			auto[x, y] = particleEmitterOffsetLeft;
			particleEmitterOffsetRight = { -x, y };
		}

		this->scale(0.10);

		runningParticles = getComponent<PixelParticles>();
		auto pp = runningParticles;
		pp->particlesPerSecond = pp->count / 2;
		pp->size = { 6, 3 };
		pp->speed = this->speed;
		pp->emitter = transform->getPosition() + sf::Vector2f{ 50, 40 };
		pp->gravity = { 0, this->speed };
		auto[w, h] = ArkEngine::windowSize();
		pp->platform = { ArkEngine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };
	}

	void update() override {
		bool moved = false;
		auto dt = ArkEngine::deltaTime().asSeconds();
		float dx = speed * dt;
		float angle = Util::toRadians(transform->getRotation());

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			transform->move(dx * std::cos(angle), dx * std::sin(angle));
			runningParticles->angleDistribution = { PI + PI/4, PI / 10, DistributionType::normal };
			runningParticles->emitter = transform->getPosition() + particleEmitterOffsetLeft;
			moved = true;
			animation->flipX = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
			runningParticles->angleDistribution = { -PI/4, PI / 10, DistributionType::normal };
			runningParticles->emitter = transform->getPosition() + particleEmitterOffsetRight;
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
		} else {
			animation->frameTime = sf::milliseconds(125);
			animation->row = 1;
			runningParticles->spawn = false;
		}
	}
};

//enum class GameTag { Bullet, Player, Wall };

/* TODO (general): TexturedParticles */
/* TODO (general): ColisionSystem */
/* TODO (general): TileSystem */
/* TODO (general): Text(with fade), TextBoxe */
/* TODO (general): MusicSystem/SoundSystem */
/* TODO (general): Shaders */
/* TODO (general): ParticleEmmiter with settings*/
/* TODO (general): State Stack*/
/* TODO (general): choose with a macro if the engine runs at a fixed frame rate or delta time/fluctuating frame rate; always use deltaTime for debug */
/* TODO (general): add a archetype component manager*/
/* TODO (entity cloning): add to the ComponentManager std::map<std::type_index, std::funciton<int(void)>> factories; return:int is the index in the pool */

class TestingEngineScene : public Scene {

	void init() override
	{
		addSystem<PointParticleSystem>();
		addSystem<PixelParticleSystem>();
		addSystem<FpsCounterSystem>();
		addSystem<ButtonSystem>();
		addSystem<TextSystem>();
		addSystem<AnimationSystem>();

		button = createEntity("button");
		player = createEntity("player");
		mouseTrail = createEntity("mouseTrail");
		rainbowPointParticles = createEntity("rainbow ps");
		greenPointParticles = createEntity("green ps");
		firePointParticles = createEntity("fire ps");
		rotatingParticles = createEntity("rotating ps");

		//fireWorks.resize(0);
		//createEntity(fireWorks);

		player.addComponent<Transform>();
		player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
		//player.addComponent<Animation>("chestie.png", std::initializer_list<uint32_t>{2, 6}, sf::milliseconds(100), 1, false);
		player.addComponent<PixelParticles>(100, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
		player.addScript<MovePlayer>(400, 180);

		//button.addScript<SaveGuiElementPosition<Button>>("mama"s, sf::Keyboard::S);
		button.addComponent<Button>(sf::FloatRect{100, 100, 200, 100});
		auto& b = button.getComponent<Button>();
		b.setFillColor(sf::Color(240, 240, 240));
		b.setOutlineColor(sf::Color::Black);
		b.setOutlineThickness(3);
		b.moveWithMouse = true;
		b.loadPosition("mama");
		b.setOrigin(b.getSize() / 2.f);
		b.onClick = []() { std::cout << "\nclick "; };
		auto& t = button.addComponent<Text>();
		t.setString("buton");
		t.setOrigin(b.getOrigin());
		t.setPosition(b.getPosition() + b.getSize() / 3.5f);
		t.setFillColor(sf::Color::Black);

		using namespace ParticleScripts;
		//auto rainbowParticles = getRainbowParticles();
		//auto fireParticles = getFireParticles();
		//auto greenParticles = getGreenParticles();

		rainbowPointParticles.addComponent<PointParticles>(getRainbowParticles());
		rainbowPointParticles.addScript<SpawnOnRightClick>();
		rainbowPointParticles.addScript<EmittFromMouse>();

		firePointParticles.addComponent<PointParticles>(getFireParticles(1'000));
		firePointParticles.addScript<SpawnOnLeftClick>();
		firePointParticles.addScript<EmittFromMouse>();

		auto& grassP = greenPointParticles.addComponent<PointParticles>(getGreenParticles());
		grassP.spawn = true;
		greenPointParticles.addScript<DeSpawnOnMouseClick<>>();
		//grass.addScript<ReadColorFromConsole>();
		greenPointParticles.addScript<EmittFromMouse>();

		rotatingParticles.addComponent<Transform>();
		auto& plimb = rotatingParticles.addComponent<PointParticles>(getRainbowParticles());
		plimb.spawn = true;
		//plimb->emitter = ArkEngine::center();
		//plimb->emitter.x += 100;
		//plimbarica.addScript<Rotate>(360/2, ArkEngine::center());
		rotatingParticles.addScript<RoatateEmitter>(180, ArkEngine::center(), 100);
		rotatingParticles.addScript<TraillingEffect>();
		//plimbarica.addScript<ReadColorFromConsole>();

		mouseTrail.addComponent<PointParticles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
		mouseTrail.addScript<EmittFromMouse>();
		mouseTrail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
		mouseTrail.addScript<TraillingEffect>();

		//std::vector<PointParticles> fireWorksParticles(fireWorks.size(), fireParticles);
		//for (auto& fw : fireWorksParticles) {
		//	auto[width, height] = ArkEngine::windowSize();
		//	float x = RandomNumber<int>(50, width - 50);
		//	float y = RandomNumber<int>(50, height - 50);
		//	fw.count = 1000;
		//	fw.emitter = { x,y };
		//	fw.spawn = false;
		//	fw.fireworks = true;
		//	fw.getColor = makeColorsVector[RandomNumber<size_t>(0, makeColorsVector.size() - 1)];
		//	fw.lifeTime = sf::seconds(RandomNumber<float>(2, 8));
		//	fw.speedDistribution = { 0, RandomNumber<float>(40, 70), DistributionType::uniform };
		//}

		//for (int i = 0; i < fireWorks.size();i++) {
		//	fireWorks[i]->setAction(Action::SpawnLater, 10);
		//	fireWorks[i]->addComponent<PointParticles>(fireWorksParticles[i]);
		//}

		PointParticleSystem::hasUniversalGravity = true;
		PointParticleSystem::gravityVector = { 0, 0 };
		PointParticleSystem::gravityMagnitude = 100;
		PointParticleSystem::gravityPoint = ArkEngine::center();

		PixelParticleSystem::hasUniversalGravity = true;
		PixelParticleSystem::gravityVector = { 0, 0 };
		PixelParticleSystem::gravityMagnitude = 100;
		PixelParticleSystem::gravityPoint = ArkEngine::center();
		
		//registerEntity(fireWorks);
		//registerEntity(particleFountains);
	}

	Entity player;
	Entity button;
	Entity rainbowPointParticles;
	Entity firePointParticles;
	Entity greenPointParticles;
	Entity mouseTrail;
	Entity rotatingParticles;
	std::vector<Entity> fireWorks;
	std::vector<Entity> particleFountains;
};

#if 0
class ChildTestScene : public TemplateScene {

	void init()
	{
		this->setup();
		createEntities({parent, child1, child2, grandson});

		parent.addChild(child1);
		parent.addChild(child2);
		child1.addChild(grandson);

		child1.addComponent<PointParticles>(getFireParticles());
		child2.addComponent<PointParticles>(getGreenParticles());
		grandson.addComponent<PointParticles>(getRainbowParticles());
		grandson.addComponent<Transform>();

		std::vector<PointParticles*> pps = parent->getChildrenComponents<PointParticles>();
		std::cout << pps.size() << std::endl; // 3
		auto cs = parent->getChildrenComponents<Transform>();
		std::cout << cs.size() << std::endl; // 1
		auto none = parent->getChildrenComponents<Mesh>();
		std::cout << none.size() << std::endl; // 0
		auto none2 = child2->getChildrenComponents<PointParticles>();
		std::cout << none2.size() << std::endl; // 0
	}

	Entity* parent;
	Entity* child1;
	Entity* child2;
	Entity* grandson;
};
#endif

int main() // are nevoie de c++17 si SFML 2.5.1
{
	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	ArkEngine::create(ArkEngine::resolutionFourByThree, "Articifii!", sf::seconds(1/120.f), settings);
	ArkEngine::backGroundColor = sf::Color(50, 50, 50);
	ArkEngine::getWindow().setVerticalSyncEnabled(false);
	ArkEngine::setScene<TestingEngineScene>();
	//ArkEngine::setScene<ChildTestScene>();

	ArkEngine::run();

	//std::string prop("");
	//std::getline(std::cin, prop);
	//std::vector<Entity> letters = makeLetterEntities(prop);
	//registerEntities(letters);

	//auto randomParticles = makeRandomParticlesFountains(50, 5.f, getGreenParticles(), false);
	//registerEntities(randomParticles);

	//Entity readGMag;
	//readGMag.addScript<GeneralScripts::ReadVarFromConsole<float>>(&ParticleSystem::gravityMagnitude, "enter gravity magnitude: ");

	
	return 0;
}
