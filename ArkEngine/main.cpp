
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
#include "State.hpp"
#include "Scene.hpp"
#include "ParticleSystem.hpp"
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
#include "Util.hpp"
#include "Scripts.hpp"
#include "AnimationSystem.hpp"
#include "FpsCounterSystem.hpp"
#include "GuiSystem.hpp"
#include "Gui.hpp"
#include "SceneInspector.hpp"
//#include "ArchetypeManager.hpp"

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

class MovePlayer : public ScriptT<MovePlayer> {
	Animation* animation;
	Transform* transform;
	PixelParticles* runningParticles;
	float rotationSpeed = 180;
	sf::Vector2f particleEmitterOffsetLeft;
	sf::Vector2f particleEmitterOffsetRight;

	void resetOffset()
	{
		particleEmitterOffsetLeft = { -165, 285 };
		{
			auto[x, y] = particleEmitterOffsetLeft;
			particleEmitterOffsetRight = { -x, y };
		}
	}

public:
	float speed = 400;

	MovePlayer() = default;
	MovePlayer(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) { }

	void setScale(sf::Vector2f scale)
	{
		transform->setScale(scale);
		resetOffset();
		particleEmitterOffsetLeft = particleEmitterOffsetLeft * transform->getScale();
		particleEmitterOffsetRight = particleEmitterOffsetRight * transform->getScale();
		runningParticles->size = {60, 30};
		runningParticles->size = runningParticles->size * transform->getScale();
	}

	sf::Vector2f getScale() const
	{
		return transform->getScale();
	}

	void init() override {
		transform = getComponent<Transform>();
		animation = getComponent<Animation>();
		runningParticles = getComponent<PixelParticles>();

		transform->setOrigin(animation->frameSize() / 2.f);
		transform->move(ArkEngine::center());

		auto pp = runningParticles;
		pp->particlesPerSecond = pp->getParticleNumber() / 2;
		pp->size = { 60, 30 };
		pp->speed = this->speed;
		pp->emitter = transform->getPosition() + sf::Vector2f{ 50, 40 };
		pp->gravity = { 0, this->speed };
		auto[w, h] = ArkEngine::windowSize();
		pp->platform = { ArkEngine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };
		this->setScale({0.1, 0.1});
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

namespace meta {
	template <> inline auto registerMembers<MovePlayer>()
	{
		return members(
			member("speed", &MovePlayer::speed),
			member("scale", &MovePlayer::getScale, &MovePlayer::setScale)
		);
	}

	template <> inline auto registerMembers<ParticleScripts::RotateEmitter>()
	{
		using namespace ParticleScripts;
		return members(
			member("distance", &RotateEmitter::distance),
			member("angular_speed", &RotateEmitter::angleSpeed),
			member("origin", &RotateEmitter::around)
		);
	}
}

//enum class GameTag { Bullet, Player, Wall };

/* TODO (general): TexturedParticles */
/* TODO (general): ColisionSystem */
/* TODO (general): TileSystem */
/* TODO (general): Text(with fade), TextBoxe */
/* TODO (general): MusicSystem/SoundSystem */
/* TODO (general): Shaders */
/* TODO (general): ParticleEmmiter with settings*/
/* TODO (general): add a archetype component manager*/
/* TODO (entity cloning): add to the ComponentManager std::map<std::type_index, std::funciton<int(void)>> factories; return:int is the index in the pool */

enum MessageType {
	TestData,
	PodData,
	Count,
};

struct Mesajul {
	std::string msg;
};

struct PodType {
	int i;
};

class TestMessageSystem : public System {
public:
	TestMessageSystem() : System(typeid(TestMessageSystem)) {}

	void handleMessage(const Message& message) override 
	{
		if (message.id == MessageType::TestData) {
			const auto& m = message.data<Mesajul>();
			std::cout << "mesaj: " << m.msg << '\n';
		}
		if (message.id == MessageType::PodData) {
			const auto& m = message.data<PodType>();
			std::cout << "int: " << m.i << "\n";
		}
	}

	void update() override
	{
		auto m = postMessage<Mesajul>(TestData);
		m->msg = "mesaju matiii";
		auto p = postMessage<PodType>(PodData);
		p->i = 5;
	}
};

enum States {
	TestingState
};

class BasicState : public State {

protected:
	Scene scene{getMessageBus()};
	sf::Sprite screen;
	sf::Texture screenImage;
	bool takenSS = false;
	bool pauseScene = false;

public:
	BasicState(MessageBus& bus) : State(bus) { }

	bool handleEvent(const sf::Event& event) override
	{
		if(!pauseScene)
			scene.handleEvent(event);

		if (event.type == sf::Event::KeyPressed)
			if (event.key.code == sf::Keyboard::F1) {
				pauseScene = !pauseScene;
				takenSS = false;
			}

		return false;
	}

	void handleMessage(const Message& message) override
	{
		if(!pauseScene)
			scene.handleMessage(message);
	}

	bool update() override
	{
		if (!pauseScene)
			scene.update();
		else
			scene.processPendingData();
		return false;
	}

	bool render(sf::RenderTarget& target) override
	{
		if (!pauseScene) {
			scene.render(target);
		} else if(!takenSS){
			scene.render(target);
			takenSS = true;
			auto& win = ArkEngine::getWindow();
			auto [x, y] = win.getSize();
			screenImage.create(x, y);
			screenImage.update(win);
			screen.setTexture(screenImage);
			target.draw(screen);
		} else if (takenSS) {
			target.draw(screen);
		}

		return false;
	}
};

// one time use component, it is removed from its entity after thes action is performed
struct DelayedAction : Component<DelayedAction> {

	DelayedAction() = default;
	
	DelayedAction(sf::Time time, std::function<void(Entity)> fun)
	{
		setAction(time, fun);
	}

	void setAction(sf::Time time, std::function<void(Entity)> fun)
	{
		this->time = time;
		action = std::move(fun);
	}

private:
	sf::Time time;
	std::function<void(Entity)> action;

	friend class DelayedActionSystem;
};

class DelayedActionSystem : public SystemT<DelayedActionSystem> {
public:
	void init() override
	{
		requireComponent<DelayedAction>();
	}

	void update() override
	{
		for (auto entity : getEntities()) {
			auto& da = entity.getComponent<DelayedAction>();
			da.time -= ArkEngine::deltaTime();
			if (da.time <= sf::Time::Zero) {
				if (da.action) {
					auto action = std::move(da.action);
					entity.removeComponent<DelayedAction>();
					action(entity);
				}
			}
		}
	}
};

class TestingState : public BasicState {
	Entity player;
	Entity button;
	Entity rainbowPointParticles;
	Entity firePointParticles;
	Entity greenPointParticles;
	Entity mouseTrail;
	Entity rotatingParticles;
	std::vector<Entity> fireWorks;
	std::vector<Entity> particleFountains;
	SceneInspector inspector{scene};

public:
	TestingState(MessageBus& mb) : BasicState(mb) { }

private:

	int getStateId() override { return States::TestingState; }

	void init() override
	{
		scene.addSystem<PointParticleSystem>();
		scene.addSystem<PixelParticleSystem>();
		scene.addSystem<FpsCounterSystem>();
		scene.addSystem<ButtonSystem>();
		scene.addSystem<TextSystem>();
		scene.addSystem<AnimationSystem>();
		scene.addSystem<DelayedActionSystem>();
		addTab("scene inspector", [&]() { inspector.render(); });
		//scene.addSystem<TestMessageSystem>();

		button = scene.createEntity("button");
		player = scene.createEntity("player");
		mouseTrail = scene.createEntity("mouse_trail");
		rainbowPointParticles = scene.createEntity("rainbow_ps");
		greenPointParticles = scene.createEntity("green_ps");
		firePointParticles = scene.createEntity("fire_ps");
		rotatingParticles = scene.createEntity("rotating_ps");

		//fireWorks.resize(0);
		//createEntity(fireWorks);

		player.addComponent<Transform>();
		player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
		//player.addComponent<Animation>("chestie.png", std::initializer_list<uint32_t>{2, 6}, sf::milliseconds(100), 1, false);
		player.addComponent<PixelParticles>(100, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
		player.addScript<MovePlayer>(400, 180);
		player.addComponent<PointParticles>();
		player.removeComponent<PointParticles>();

		//button.addScript<SaveGuiElementPosition<Button>>("mama"s, sf::Keyboard::S);
		button.addComponent<Button>(sf::FloatRect{100, 100, 200, 100});
		auto& b = button.getComponent<Button>();
		b.setFillColor(sf::Color(240, 240, 240));
		b.setOutlineColor(sf::Color::Black);
		b.setOutlineThickness(3);
		b.moveWithMouse = true;
		b.loadPosition("mama");
		b.setOrigin(b.getSize() / 2.f);
		b.onClick = []() { std::cout << "click "; };
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
		rainbowPointParticles.addScript(typeid(EmittFromMouse));

		Entity rainbowClone = rainbowPointParticles.clone();
		rainbowClone.addScript<SpawnOnRightClick>();
		rainbowClone.removeScript(typeid(SpawnOnRightClick));
		//rainbowClone.addScript<SpawnOnLeftClick>()->deactivate();
		rainbowClone.addScript<EmittFromMouse>();
		rainbowClone.addComponent<DelayedAction>(sf::seconds(5), [](Entity e) {
			//e.setScriptActive<SpawnOnLeftClick>(true);
		});

		firePointParticles.addComponent<PointParticles>(getFireParticles(1'000));
		firePointParticles.addScript<SpawnOnLeftClick>();
		firePointParticles.addScript<EmittFromMouse>();
		firePointParticles.addComponent<DelayedAction>(sf::seconds(5), [this](Entity e) {
			scene.destroyEntity(e);
		});

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
		rotatingParticles.addScript<RotateEmitter>(180, ArkEngine::center(), 100);
		rotatingParticles.addScript<TraillingEffect>();
		//plimbarica.addScript<ReadColorFromConsole>();

		//mouseTrail.addComponent<PointParticles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
		mouseTrail.addComponent(typeid(PointParticles));
		auto& mouseParticles = mouseTrail.getComponent<PointParticles>();
		mouseParticles.setParticleNumber(1000);
		mouseParticles.setLifeTime(sf::seconds(5), DistributionType::normal);
		mouseParticles.speedDistribution = {0.f, 2.f};
		mouseParticles.angleDistribution = {0.f, 0.f};
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

	ArkEngine::create(ArkEngine::resolutionFullHD, "Articifii!", sf::seconds(1/120.f), settings);
	ArkEngine::backGroundColor = sf::Color(50, 50, 50);
	ArkEngine::getWindow().setVerticalSyncEnabled(false);
	ArkEngine::registerState<class TestingState>(States::TestingState);
	ArkEngine::pushFirstState(States::TestingState);

	ArkEngine::run();
	
	return 0;
}
