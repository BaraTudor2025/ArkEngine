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
		transform->move(VectorEngine::center());

		particleEmitterOffsetLeft = { -165, 285 };
		{
			auto[x, y] = particleEmitterOffsetLeft;
			particleEmitterOffsetRight = { -x, y };
		}

		this->scale(0.10);

		runningParticles = getComponent<PixelParticles>();
		auto pp = runningParticles;
		pp->particlesPerSecond = pp->count / 2;
		pp->size = { 1, 1 };
		pp->speed = this->speed;
		pp->emitter = transform->getPosition() + sf::Vector2f{ 50, 40 };
		pp->gravity = { 0, this->speed };
		auto[w, h] = VectorEngine::windowSize();
		pp->platform = { VectorEngine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };
	}

	void fixedUpdate() override {
		bool moved = false;
		auto dt = VectorEngine::fixedTime().asSeconds();
		float dx = speed * dt;
		float angle = toRadians(transform->getRotation());

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

/* TODO: TexturedParticles */
/* TODO: Entity.children : vector<Entity*> */
/* TODO: TileSystem */
/* TODO: Text(with fade), TextBoxe */
/* TODO: MusicSystem/SoundSystem */
/* TODO: Shaders */

namespace priv {
	template <typename T>
	struct SpecializedSystemImpl {
		virtual void init(T&) = 0;
		virtual void update(T&) = 0;
	};
}

template <typename... Components>
class SpecializedSystem : public System, priv::SpecializedSystemImpl<Components>... {

	void init() override
	{
		((this->forEach<Components>(this->init, this)),...);
	}

	void update() override
	{
		((this->forEach<Components>(this->update, this)), ...);
	}

	void fixedUpdate() override
	{
		((this->forEach<Components>(this->fixedUpdate, this)), ...);
	}

	void render(sf::RenderTarget& target) override
	{
		((this->forEach<Components>(this->render, this, target)), ...);
	}

};

template <typename... Components>
class PackSystem : public System {

};


struct SystemBase {
	virtual void init() = 0;
	virtual void update() = 0;
};

template <typename T>
struct SystemIndividualImpl {
	virtual void init(T&) = 0;
	virtual void update(T&) = 0;
};

template <typename... Ts>
struct SystemPackImpl {
	std::vector<std::tuple<Ts*...>> comps;
	virtual void init(Ts&...) = 0;
	virtual void update(Ts&...) = 0;

	template <typename T, typename F>
	void forEach(F f)
	{
		//auto removePtr = [](std::tuple<Ts*...> tuple) { return std::apply(); };
		//for (auto& c : comps)
		//	std::apply(f, removePtr(c));
	}
};

enum class SystemConfig { Simple, Individual, Pack };

template <typename...Ts>
struct SystemIndividualImplExpand : SystemIndividualImpl<Ts>... { };

template <SystemConfig SysConf, typename... Ts>
struct TestSystem : SystemBase, std::conditional_t<SysConf == SystemConfig::Individual, SystemIndividualImplExpand<Ts...>, SystemPackImpl<Ts...>>
{
	static inline constexpr int componentNum = sizeof...(Ts);
	virtual void init()
	{
		if constexpr (SysConf == SystemConfig::Pack)
		{
			//std::cout << "mama";
			//for (auto& e : this->getEntities()) {
			//	std::tuple<Ts*...> pack = std::make_tuple(e.getComponent<Ts>()...);
			//	if (std::apply([](auto... c) { return c != nullptr && ...; }, pack)) {
			//		this->componentNum.push_back(pack);
			//	}
			//}
		}
		//(forEach<T>(initComponent<T>), ...);
	}
	virtual void update()
	{
	}
};

struct TestParticle { static inline int id = 1; };
struct TestParticle2 { static inline int id = 2; };

struct TestParticleSystem : public TestSystem<SystemConfig::Individual, TestParticle, TestParticle2> {
	//void init() override { }
	void init(TestParticle& p) override
	{

	}
	void init(TestParticle2& ) override { }
	void update(TestParticle&) override { }
	void update(TestParticle2&) override { }
};

struct TestParticleSystemPack : public TestSystem<SystemConfig::Pack, TestParticle, TestParticle2> {
	void init(TestParticle&, TestParticle2&) override { }
	void update(TestParticle&, TestParticle2&) override { }
};

class TestingEngineScene : public Scene {

	void init()
	{
		TestParticleSystem sys;
		TestParticleSystemPack sysPack;
		TestParticle ps;
		TestParticle2 ps2;
		sys.init(ps);
		sysPack.init(ps, ps2);
		//std::cout << sys.componentIDs.size() << '\n';
		addSystem<ParticleSystem>();
		addSystem<FpsCounterSystem>(sf::Color::White);
		addSystem<GuiSystem>();
		addSystem<AnimationSystem>();

		addComponentType<Transform>();
		addComponentType<Mesh, Animation>();
		addComponentType<PixelParticles, PointParticles>();
		addComponentType<Button, Text>();

		registerEntity(button);
		registerEntity(player);
		registerEntity(mouseTrail);
		registerEntity(rainbowPointParticles);
		registerEntity(greenPointParticles);
		registerEntity(firePointParticles);
		registerEntity(rotatingParticles);
		fireWorks.resize(0);
		for (auto& fw : fireWorks)
			registerEntity(fw);

		player.addComponent<Transform>();
		player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
		player.addComponent<PixelParticles>(30, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
		player.addScript<MovePlayer>(400, 180);
		//player.tag = "player";

		//button.addScript<SaveGuiElementPosition<Button>>("mama"s, sf::Keyboard::S);
		//button.tag = "button";
		auto b = button.addComponent<Button>(sf::FloatRect{100, 100, 200, 100});
		b->setFillColor(sf::Color(240, 240, 240));
		b->setOutlineColor(sf::Color::Black);
		b->setOutlineThickness(3);
		b->moveWithMouse = true;
		b->loadPosition("mama");
		b->setOrigin(b->getSize() / 2.f);
		b->onClick = []() { std::cout << "\nclick "; };
		auto t = button.addComponent<Text>();
		t->setOrigin(b->getOrigin());
		t->setPosition(b->getPosition() + b->getSize() / 3.5f);
		t->setFillColor(sf::Color::Black);

		using namespace ParticleScripts;
		auto rainbowParticles = getRainbowParticles();
		auto fireParticles = getFireParticles();
		auto greenParticles = getGreenParticles();

		//rainbowPointParticles.tag = "rainbow particles";
		rainbowPointParticles.addComponent<PointParticles>(rainbowParticles);
		rainbowPointParticles.addScript<SpawnOnRightClick>();
		rainbowPointParticles.addScript<EmittFromMouse>();

		//firePointParticles.tag = "fire particles";
		firePointParticles.addComponent<PointParticles>(getFireParticles(1'000));
		firePointParticles.addScript<SpawnOnLeftClick>();
		firePointParticles.addScript<EmittFromMouse>();

		auto grassP = greenPointParticles.addComponent<PointParticles>(greenParticles);
		grassP->spawn = true;
		greenPointParticles.addScript<DeSpawnOnMouseClick<>>();
		//grass.addScript<ReadColorFromConsole>();
		greenPointParticles.addScript<EmittFromMouse>();

		rotatingParticles.addComponent<Transform>();
		auto plimb = rotatingParticles.addComponent<PointParticles>(rainbowParticles);
		plimb->spawn = true;
		//plimb->emitter = VectorEngine::center();
		//plimb->emitter.x += 100;
		//plimbarica.addScript<Rotate>(360/2, VectorEngine::center());
		rotatingParticles.addScript<RoatateEmitter>(360, VectorEngine::center(), 100);
		rotatingParticles.addScript<TraillingEffect>();
		//plimbarica.addScript<ReadColorFromConsole>();

		mouseTrail.addComponent<PointParticles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
		mouseTrail.addScript<EmittFromMouse>();
		mouseTrail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
		mouseTrail.addScript<TraillingEffect>();

		std::vector<PointParticles> fireWorksParticles(fireWorks.size(), fireParticles);
		for (auto& fw : fireWorksParticles) {
			auto[width, height] = VectorEngine::windowSize();
			float x = RandomNumber<int>(50, width - 50);
			float y = RandomNumber<int>(50, height - 50);
			fw.count = 1000;
			fw.emitter = { x,y };
			fw.spawn = false;
			fw.fireworks = true;
			fw.getColor = makeColorsVector[RandomNumber<size_t>(0, makeColorsVector.size() - 1)];
			fw.lifeTime = sf::seconds(RandomNumber<float>(2, 8));
			fw.speedDistribution = { 0, RandomNumber<float>(40, 70), DistributionType::uniform };
		}

		for (int i = 0; i < fireWorks.size();i++) {
			fireWorks[i].setAction(Action::SpawnLater, 10);
			fireWorks[i].addComponent<PointParticles>(fireWorksParticles[i]);
		}

		ParticleSystem::hasUniversalGravity = true;
		ParticleSystem::gravityVector = { 0, 0 };
		ParticleSystem::gravityMagnitude = 100;
		ParticleSystem::gravityPoint = VectorEngine::center();
		
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

int main() // are nevoie de c++17 si SFML 2.5.1
{
	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	VectorEngine::create(VectorEngine::resolutionFourByThree, "Articifii!", sf::seconds(1/60.f), settings);
	VectorEngine::setVSync(false);
	VectorEngine::backGroundColor = sf::Color(50, 50, 50);
	VectorEngine::setScene<TestingEngineScene>();

	VectorEngine::run();

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
