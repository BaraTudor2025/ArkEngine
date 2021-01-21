#include <functional>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <thread>
#include <memory_resource>
#include <concepts>

#include <SFML/Graphics.hpp>
#include <SFML/System/String.hpp>

#include <ark/core/Engine.hpp>
#include <ark/core/State.hpp>
#include <ark/ecs/Scene.hpp>
#include <ark/ecs/SceneInspector.hpp>
#include <ark/util/Util.hpp>
#include <ark/util/RandomNumbers.hpp>
#include <ark/gui/Gui.hpp>

#include "Scripts.hpp"
#include "ParticleSystem.hpp"
#include "ParticleScripts.hpp"
#include "AnimationSystem.hpp"
#include "FpsCounterDirector.hpp"
#include "GuiSystem.hpp"
#include "ScriptingSystem.hpp"
#include "LuaScriptingSystem.hpp"
#include "Allocators.hpp"
#include "DrawableSystem.hpp"

using namespace std::literals;
using namespace ark;

#if 0

struct HitBox : public Component<HitBox> {
	std::vector<sf::IntRect> hitBoxes;
	std::function<void(Entity&, Entity&)> onColide = nullptr;
	std::optional<int> mass; // if mass is undefined object is imovable
};

class ColisionSystem : public System {

};

#endif

class MoveAnimatedPlayer : public ScriptT<MoveAnimatedPlayer, true> {
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
			auto [x, y] = particleEmitterOffsetLeft;
			particleEmitterOffsetRight = { -x, y };
		}
	}

public:
	float speed = 400;

	MoveAnimatedPlayer() = default;
	MoveAnimatedPlayer(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) {}

	void setScale(sf::Vector2f scale)
	{
		transform->setScale(scale);
		resetOffset();
		particleEmitterOffsetLeft = particleEmitterOffsetLeft * transform->getScale();
		particleEmitterOffsetRight = particleEmitterOffsetRight * transform->getScale();
		runningParticles->size = sf::Vector2f{ 60, 30 } * transform->getScale();
	}

	sf::Vector2f getScale() const
	{
		return transform->getScale();
	}

	void bind() noexcept override
	{
		transform = getComponent<Transform>();
		animation = getComponent<Animation>();
		runningParticles = getComponent<PixelParticles>();
		setScale(transform->getScale());
	}

	void update() noexcept override
	{
		bool moved = false;
		auto dt = Engine::deltaTime().asSeconds();
		float dx = speed * dt;
		float angle = Util::toRadians(transform->getRotation());

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			transform->move(dx * std::cos(angle), dx * std::sin(angle));
			runningParticles->angleDistribution = { PI + PI / 4, PI / 10, DistributionType::normal };
			runningParticles->emitter = transform->getPosition() + particleEmitterOffsetLeft;
			moved = true;
			animation->flipX = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
			runningParticles->angleDistribution = { -PI / 4, PI / 10, DistributionType::normal };
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
		}
		else {
			animation->frameTime = sf::milliseconds(125);
			animation->row = 1;
			runningParticles->spawn = false;
		}
	}
};

ARK_REGISTER_TYPE(gScriptGroupName, MoveAnimatedPlayer, registerServiceDefault<MoveAnimatedPlayer>())
{
	auto* type = ark::meta::getMetadata(typeid(MoveAnimatedPlayer));
	type->data<ark::SceneInspector::VectorOptions>(ark::SceneInspector::serviceOptions, {
			{.property_name = "scale", .drag_speed = 0.001}
		});
	return members<MoveAnimatedPlayer>(
		member_property("speed", &MoveAnimatedPlayer::speed),
		member_property("scale", &MoveAnimatedPlayer::getScale, &MoveAnimatedPlayer::setScale)
	);
}

ARK_REGISTER_MEMBERS(ParticleScripts::RotateEmitter)
{
	using namespace ParticleScripts;
	return members<ParticleScripts::RotateEmitter>(
		member_property("distance", &RotateEmitter::distance),
		member_property("angular_speed", &RotateEmitter::angleSpeed),
		member_property("origin", &RotateEmitter::around)
	);
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

// TODO (design): remove entity class as a wrapper and only use entity-manager

/* TODO:
	* small vector
	* optional ref
	* refactor Util::find
	* - optimize rng
	* - add 'ark' namespace
	* - add serialization(nlohmann json)
	* add more options to gui editor
	* add scripting with lua(sol3) and/or chaiscript
	* custom allocator for lua/scripts/systems
	* dynamic aabb tree for colision
	* ParticleSystem shaders with point size
*/

struct Mesajul {
	std::string msg;
};

struct PodType {
	int i;
};

class TestMessageSystem : public ark::System {
public:
	TestMessageSystem() : ark::System(typeid(TestMessageSystem)) {}

	void handleMessage(const ark::Message& message) override
	{
		if (message.is<Mesajul>()) {
			const auto& m = message.data<Mesajul>();
			std::cout << "mesaj: " << m.msg << '\n';
		}
		if (message.is<PodType>()) {
			const auto& m = message.data<PodType>();
			std::cout << "int: " << m.i << "\n";
		}
	}

	void update() override
	{
		auto m = postMessage<Mesajul>("mesaju matiii");
		auto p = postMessage<PodType>(5);
		p->i = 5;
	}
};

enum States {
	TestingState,
	ImGuiLayer,
	ChessState
};

class BasicState : public ark::State {

protected:
	ark::SystemManager systems;
	ark::Registry manager;
	sf::Sprite screen;
	sf::Texture screenImage;
	bool takenSS = false;
	bool pauseScene = false;

public:
	BasicState(ark::MessageBus& bus, std::pmr::memory_resource* res = std::pmr::new_delete_resource()) 
		: ark::State(bus), manager(res), systems(bus, manager) {}

	void handleEvent(const sf::Event& event) override
	{
		if (!pauseScene)
			systems.handleEvent(event);

		if (event.type == sf::Event::KeyPressed)
			if (event.key.code == sf::Keyboard::F1) {
				pauseScene = !pauseScene;
				takenSS = false;
			}
	}

	void handleMessage(const ark::Message& message) override
	{
		if (!pauseScene)
			systems.handleMessage(message);
	}

	void update() override
	{
		manager.update();
		if (!pauseScene)
			systems.update();
	}

	void render(sf::RenderTarget& target) override
	{
		if (!pauseScene) {
			systems.render(target);
		}
		else if (!takenSS) {
			systems.render(target);
			takenSS = true;
			auto& win = Engine::getWindow();
			auto [x, y] = win.getSize();
			screenImage.create(x, y);
			screenImage.update(win);
			screen.setTexture(screenImage);
			target.draw(screen);
		}
		else if (takenSS) {
			target.draw(screen);
		}
	}
};

// one time use component, it is removed from its entity after thes action is performed
struct DelayedAction {

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
ARK_REGISTER_COMPONENT(DelayedAction, registerServiceDefault<DelayedAction>()) { return members<DelayedAction>(); }

class DelayedActionSystem : public ark::SystemT<DelayedActionSystem> {
public:
	void init() override
	{
		querry = entityManager.makeQuerry<DelayedAction>();
	}

	void update() override
	{
		for (auto entity : getEntities()) {
			auto& da = entity.getComponent<DelayedAction>();
			da.time -= Engine::deltaTime();
			if (da.time <= sf::Time::Zero) {
				if (da.action) {
					auto action = std::move(da.action);
					entityManager.safeRemoveComponent(entity, typeid(DelayedAction));
					action(entity);
				}
			}
		}
	}
};

class EmittFromMouseTest : public ScriptT<EmittFromMouseTest> {
	PointParticles* p;
public:
	void bind() noexcept override
	{
		p = getComponent<PointParticles>();
	}
	void update() noexcept override
	{
		p->emitter = Engine::mousePositon();
	}
};

class SaveEntityScript : public ScriptT<SaveEntityScript> {
public:
	SaveEntityScript() = default;

	char key = 'k';

	void bind() noexcept override {
	}

	void handleEvent(const sf::Event& ev) noexcept override
	{
		if (ev.type == sf::Event::KeyPressed) {
			if (ev.key.code == std::tolower(key) - 'a'){
				auto e = entity();
				ark::serde::serializeEntity(e);
				GameLog("just serialized entity %s", e.getComponent<TagComponent>().name);
			}
		}
	}
};
ARK_REGISTER_MEMBERS(SaveEntityScript) { return members<SaveEntityScript>(member_property("key", &SaveEntityScript::key)); }

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

public:
	TestingState(ark::MessageBus& mb) : BasicState(mb) {}

private:

	int getStateId() override { return States::TestingState; }

	ark::Entity makeEntity(std::string name)
	{
		Entity e = manager.createEntity();
		e.getComponent<TagComponent>().name = name;
		return e;
	}

	Registry cloneRegistry() {
		auto newManager = Registry();
		for (auto type : this->manager.getComponentTypes())
			newManager.addComponentType(type);
		// TODO
		//for (auto type : this->manager.getDefaultComponentTypes())
			//newManager.addDefaultComponent(type);

		// tre facut MANUAL?!
		//manager.onConstructionTable = this->onConstructionTable;
		//for (Entity entity : this->entitiesView()) {
		//	Entity clone = manager.createEntity();
		//	for (auto [type, ptr] : entity.runtimeComponentView()) {
		//		auto [newComp, /*isAlready*/_] = manager.implAddComponentOnEntity(clone, type, false, ptr);
		//		manager.onConstruction(type, newComp, clone);
		//		// ar trebui si asta pe langa .onCtor()
		//		//manager.onCopy(type, newComp, clone, ptr);
		//	}
		//}
		return std::move(newManager);
	}

	void init() override
	{
		manager.addDefaultComponent<ark::TagComponent>();
		manager.addDefaultComponent<ark::Transform>();
		manager.addAllComponentTypesFromMetaGroup();

		systems.addSystem<PointParticleSystem>();
		systems.addSystem<PixelParticleSystem>();
		systems.addSystem<ButtonSystem>();
		systems.addSystem<TextSystem>();
		systems.addSystem<AnimationSystem>();
		systems.addSystem<DelayedActionSystem>();
		systems.addSystem<ScriptingSystem>();

		systems.addSystem<FpsCounterDirector>();
		auto* inspector = systems.addSystem<SceneInspector>();
		ImGuiLayer::addTab({ "registry inspector", [=]() { inspector->renderSystemInspector(); } });

		manager.onConstruction<TagComponent>(TagComponent::onConstruction());
		manager.onConstruction<ScriptingComponent>();
		manager.onConstruction<LuaScriptingComponent>(systems.addSystem<LuaScriptingSystem>());
		manager.onCopy<ScriptingComponent>(ScriptingComponent::onCopy);

		button = makeEntity("button");
		mouseTrail = makeEntity("mouse_trail");
		rainbowPointParticles = makeEntity("rainbow_ps");
		greenPointParticles = makeEntity("green_ps");
		firePointParticles = makeEntity("fire_ps");
		rotatingParticles = makeEntity("rotating_ps");

		player = makeEntity("player");
		ark::serde::deserializeEntity(player);
		
		//player.addComponent<Animation>("chestie.png", std::initializer_list<uint32_t>{2, 6}, sf::milliseconds(100), 1, false);
		/*
		auto& transform = player.addComponent<Transform>();
		auto& animation = player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
		auto& pp = player.addComponent<PixelParticles>(100, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
		auto& playerScripting = player.addComponent<ScriptingComponent>();
		auto moveScript = playerScripting.addScript<MoveAnimatedPlayer>(400, 180);

		transform.move(Engine::center());
		transform.setOrigin(animation.frameSize() / 2.f);

		pp.particlesPerSecond = pp.getParticleNumber() / 2;
		pp.size = { 60, 30 };
		pp.speed = moveScript->speed;
		pp.emitter = transform.getPosition() + sf::Vector2f{ 50, 40 };
		pp.gravity = { 0, moveScript->speed };
		auto[w, h] = Engine::windowSize();
		pp.platform = { Engine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };

		moveScript->setScale({0.1, 0.1});

		player.addComponent<DelayedAction>(sf::seconds(4), [](Entity e) {
			GameLog("just serialized entity %s", e.getComponent<TagComponent>().name);
			ark::serde::serializeEntity(e);
		});
		/**/

		button.addComponent<Button>(sf::FloatRect{ 100, 100, 200, 100 });
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

		rainbowPointParticles.addComponent<PointParticles>(getRainbowParticles());
		{
			auto& scripts = rainbowPointParticles.addComponent<ScriptingComponent>();
			scripts.addScript<SpawnOnRightClick>();
			scripts.addScript(typeid(EmittFromMouse));
		}

#if 1
		ark::Entity rainbowClone = manager.cloneEntity(rainbowPointParticles);
		rainbowClone.getComponent<TagComponent>().name = "rainbow_clone";
		{
			auto& scripts = rainbowClone.getComponent<ScriptingComponent>();
			scripts.getScript<SpawnOnRightClick>()->setActive(false);;
			//auto& scripts = rainbowClone.addComponent<ScriptingComponent>();
			//scripts.addScript<SpawnOnRightClick>();
			//scripts.removeScript(typeid(SpawnOnRightClick));
			//scripts.addScript<SpawnOnLeftClick>()->setActive(false);
			//scripts.addScript<EmittFromMouse>();
		}
		rainbowClone.addComponent<DelayedAction>(sf::seconds(5), [](ark::Entity e) {
			e.getComponent<ScriptingComponent>().getScript<SpawnOnRightClick>()->setActive(true);
		});
#endif

		firePointParticles.addComponent<PointParticles>(getFireParticles(1'000));
		{
			auto& scripts = firePointParticles.addComponent<ScriptingComponent>();
			scripts.addScript<SpawnOnLeftClick>();
			scripts.addScript<EmittFromMouse>();
		}
		firePointParticles.addComponent<DelayedAction>(sf::seconds(5), [this](ark::Entity e) {
			manager.destroyEntity(e);
		});


		auto& grassP = greenPointParticles.addComponent<PointParticles>(getGreenParticles());
		grassP.spawn = true;
		{
			auto& scripts = greenPointParticles.addComponent<ScriptingComponent>();
			scripts.addScript<DeSpawnOnMouseClick<>>();
			scripts.addScript<EmittFromMouse>();
			auto& luaScripts = greenPointParticles.addComponent<LuaScriptingComponent>();
			luaScripts.addScript("test.lua");
		}

		rotatingParticles.addComponent<Transform>();
		auto& plimb = rotatingParticles.addComponent<PointParticles>(getRainbowParticles());
		plimb.spawn = true;
		//plimb->emitter = Engine::center();
		//plimb->emitter.x += 100;
		//plimbarica.addScript<Rotate>(360/2, Engine::center());
		{
			auto& scripts = rotatingParticles.addComponent<ScriptingComponent>();
			scripts.addScript<RotateEmitter>(180, Engine::center(), 100);
			scripts.addScript<TraillingEffect>();
		}


		//mouseTrail.addComponent<PointParticles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
		mouseTrail.addComponent(typeid(PointParticles));
		auto& mouseParticles = mouseTrail.getComponent<PointParticles>();
		mouseParticles.setParticleNumber(1000);
		mouseParticles.setLifeTime(sf::seconds(5), DistributionType::normal);
		mouseParticles.speedDistribution = { 0.f, 2.f };
		mouseParticles.angleDistribution = { 0.f, 0.f };
		{
			auto& scripts = mouseTrail.addComponent<ScriptingComponent>();
			scripts.addScript<EmittFromMouseTest>();
			scripts.addScript<TraillingEffect>();
			scripts.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
		}

		//std::vector<PointParticles> fireWorksParticles(fireWorks.size(), fireParticles);
		//for (auto& fw : fireWorksParticles) {
		//	auto[width, height] = Engine::windowSize();
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
		PointParticleSystem::gravityPoint = Engine::center();

		PixelParticleSystem::hasUniversalGravity = true;
		PixelParticleSystem::gravityVector = { 0, 0 };
		PixelParticleSystem::gravityMagnitude = 100;
		PixelParticleSystem::gravityPoint = Engine::center();
	}
};

#if 0
class ChildTestScene : public TemplateScene {

	void init()
	{
		this->setup();
		createEntities({ parent, child1, child2, grandson });

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
		auto none = parent->getChildrenComponents<MeshComponent>();
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

struct erased_function {
	template <typename T, typename F>
	void ctor(F&& f)
	{
		fun = [](void* arg) {
			f(*static_cast<T*>(arg));
		};
	}

	template <typename T>
	void call(T& arg)
	{
		fun(&arg);
	}

	void(*fun)(void*) = nullptr;
};


/**/

class MoveEntityScript : public ScriptT<MoveEntityScript, true> {
	Transform* transform;

public:
	float speed = 400;
	float rotationSpeed = 180;

	MoveEntityScript() = default;
	MoveEntityScript(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) {}

	void bind() noexcept override
	{
		transform = getComponent<Transform>();
	}

	void update() noexcept override
	{
		bool moved = false;
		auto dt = Engine::deltaTime().asSeconds();
		float dx = speed * dt;
		float angle = Util::toRadians(transform->getRotation());

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
			transform->move(dx * std::cos(angle), dx * std::sin(angle));
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
			transform->move(dx * std::sin(angle), -dx * std::cos(angle));
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
			transform->move(-dx * std::sin(angle), dx * std::cos(angle));

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
			transform->rotate(rotationSpeed * dt);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::E))
			transform->rotate(-rotationSpeed * dt);
	}
};

ARK_REGISTER_TYPE(gScriptGroupName, MoveEntityScript, registerServiceDefault<MoveEntityScript>())
{
	return members<MoveEntityScript>(
		member_property("speed", &MoveEntityScript::speed),
		member_property("rotation_speed", &MoveEntityScript::rotationSpeed)
	);
}

struct ChessPlayer {
	std::string name;
	std::string color;
};


struct BoardPositionComponent {
	int x, y;
};

struct ChessPieceComponent {
	//int x, y; // position on board
	int type;
	int dx, dy;
	std::function<bool(int, int)> canMoveTo;
	ChessPlayer* player;
	sf::FloatRect selectArea;
};

struct ChessBoard {
	std::vector<std::vector<ChessPieceComponent>> board;
};

ARK_REGISTER_COMPONENT(ChessPieceComponent, registerServiceDefault<ChessPieceComponent>()) { 
	return members<ChessPieceComponent>(
		member_property("selectArea", &ChessPieceComponent::selectArea)
	); 
}

class ChessSystem : public ark::SystemT<ChessSystem> {
	std::vector<std::vector<ChessPieceComponent>> board = { { } };
	ChessPlayer playerInTurn;
	ark::Entity selectedPiece;
public:

	constexpr static inline int kSize = 75;

	void init() override {
		querry = entityManager.makeQuerry<Transform, ChessPieceComponent>();
		//querry.onEntityAdd([](ark::Entity entity) {
		//});
	}

	void handleEvent(sf::Event ev) override {
		if (ev.type == sf::Event::MouseButtonPressed 
			&& ev.mouseButton.button == sf::Mouse::Button::Left) {
			for (Entity entity : querry.getEntities()) {
				auto& piece = entity.getComponent<ChessPieceComponent>();
				if (piece.selectArea.contains(ev.mouseButton.x, ev.mouseButton.y)) {
					if (true/*daca poate fi mutat*/) {
						selectedPiece = entity;
						auto [x, y] = entity.getComponent<Transform>().getPosition();
						piece.dx = ev.mouseButton.x - x;
						piece.dy = ev.mouseButton.y - y;
					}
				}
			}
		}
		else if (ev.type == sf::Event::MouseButtonReleased) {
			if (selectedPiece) {
				auto& trans = selectedPiece.getComponent<Transform>();
				auto p = trans.getPosition() + sf::Vector2f{ kSize / 2, kSize / 2 };
				trans.setPosition(kSize * int(p.x / kSize), kSize * int(p.y / kSize));
				selectedPiece = {};
			}
		}
	}

	void update() override {
		if (selectedPiece) {
			auto& trans = selectedPiece.getComponent<Transform>();
			auto& piece = selectedPiece.getComponent<ChessPieceComponent>();
			auto [x, y] = ark::Engine::mousePositon();
			trans.setPosition(x - piece.dx, y - piece.dy);
		}
		for (Entity entity : querry.getEntities()) {
			auto [trans, piece] = entity.getComponents<Transform, ChessPieceComponent>();
			piece.selectArea.left = trans.getPosition().x;
			piece.selectArea.top = trans.getPosition().y;
		}
	}
};

struct TestProp {
	int type;
	const int& getType() const {
	//int getType() const {
		return type;
	}
	void setType(const int& type) {
	//void setType(int type) {
		this->type = type;
	}

	std::string name;
	const std::string& getName() const {
		return name;
	}
	void setName(const std::string& name) {
		this->name = name;
	}
};

class ChessState : public BasicState {

public:
	ChessState(ark::MessageBus& mb) : BasicState(mb) {}

	int getStateId() override { return States::ChessState; }
	
	std::vector<std::vector<int>> board = 
	{	{-1, -2, -3, -4, -5, -3, -2, -1},
		{-6, -6, -6, -6, -6, -6, -6, -6},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{6, 6, 6, 6, 6, 6, 6, 6},
		{1, 2, 3, 4, 5, 3, 2, 1}
	};

	void init() override {
		{
			//auto prop = ark::meta::RuntimeProperty("type", &TestProp::getType, &TestProp::setType);
			//ark::meta::registerRuntimeProperties<TestProp>(ark::meta::RuntimeProperty("type", &TestProp::getType, &TestProp::setType));
			//auto prop = ark::meta::getRuntimeProperties<TestProp>()->front();
			auto prop = ark::meta::RuntimeProperty("type", &TestProp::getType, &TestProp::setType);
			auto comp = TestProp();
			comp.type = 10;
			int type;
			prop.get(&comp, &type);
			std::cout << "\n property read:" << type;
			type = 50;
			prop.set(&comp, &type);
			std::cout << "\n property write: " << comp.type;

			auto any = prop.get(&comp);
			int& ref_type = std::any_cast<int&>(any);
			std::cout << "\n property read:" << ref_type;
			ref_type = 100;
			prop.set(&comp, any);
			std::cout << "\n property write: " << comp.type;
			std::cout << '\n';

			auto propStr = ark::meta::RuntimeProperty("str", &TestProp::getName, &TestProp::setName);
			comp.name = "init name";
			any = propStr.get(&comp);
			std::string& refStr = std::any_cast<std::string&>(any);
			std::cout << "\n property read:" << refStr;
			refStr = "schimbare 1";
			propStr.set(&comp, any);
			std::cout << "\n property write: " << comp.name;
			std::cout << '\n';
			//std::string testStr = *(std::string*)propStr.fromAny(any);
			void* vptr = (void*)&std::any_cast<std::string&>(any);
			std::string testStr = *(std::string*)vptr;
			std::cout << "\n property conv: " << testStr;
			std::cout << '\n';
		}

		manager.addDefaultComponent<TagComponent>();
		manager.addDefaultComponent<Transform>();

		systems.addSystem<MeshSystem>();
		systems.addSystem<ChessSystem>();
		systems.addSystem<SceneInspector>();
		systems.addSystem<FpsCounterDirector>();
		systems.addSystem<ScriptingSystem>();
		systems.addSystem<RenderSystem>();

		manager.onConstruction<TagComponent>();
		manager.onCopy<TagComponent>(TagComponent::onCopy);
		manager.onConstruction<ScriptingComponent>();
		manager.onCopy<ScriptingComponent>(ScriptingComponent::onCopy);
		//manager.onConstruction<LuaScriptingComponent>(this->systems.getSystem<LuaScriptingSystem>());
		const int size = ChessSystem::kSize;

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				if (board[i][j] == 0)
					continue;
				int x = std::abs(board[i][j]) - 1;
				int y = board[i][j] > 0 ? 1 : 0;

				Entity piesa = manager.createEntity();

				auto& trans = piesa.getComponent<Transform>();
				trans.setPosition(size * j, size * i);
				//trans.setOrigin(0, 0);
				//trans.setPosition(400, 400);

				auto& mesh = piesa.addComponent<MeshComponent>("chess_pieces.png", true);
				mesh.setTextureRect(sf::IntRect{ size * x, size * y, size, size });

				auto& piece = piesa.addComponent<ChessPieceComponent>();
				piece.selectArea.width = size;
				piece.selectArea.height = size;
				//piece.x = i;
				//piece.y = j;
			}
		}
	}
};
/**/

//template<typename... Component>
//[[nodiscard]] bool has(const entity_type entity) const {
//	ENTT_ASSERT(valid(entity));
//	return [entity](auto *... cpool) { return ((cpool && cpool->contains(entity)) && ...); }(assure<Component>()...);
//}

template <typename F, typename... Args, std::size_t... Indexes>
//decltype(auto) 
std::any arkImplInvoke(F fun, std::any* args, std::index_sequence<Indexes...>) {
	return std::invoke(fun, std::any_cast<Args>(args[Indexes])...);
}

template <typename F, typename... Args>
void call(F fun, Args&&... args) {
	std::array<std::any, sizeof...(Args)> anyArgs{std::forward<Args>(args)...};
	fun(anyArgs.data());
}

template <typename F, typename... Args>
auto decalre(F fun) {
	return[fun](std::any* args) {
		return arkImplInvoke<decltype(fun), Args...>(fun, args, std::index_sequence_for<sizeof...(Args)>);
	};
}


int main() // are nevoie de c++17 si SFML 2.5.1
{
	auto default_memory_res = makePrintingMemoryResource("Rogue PMR Allocation!", std::pmr::null_memory_resource());
	std::pmr::set_default_resource(&default_memory_res);

	//auto buff_size = 1000;
	//auto buffer = std::make_unique<std::byte[]>(buff_size);
	//auto track_new_del = makePrintingMemoryResource("new-del", std::pmr::new_delete_resource());
	//auto monotonic_res = std::pmr::monotonic_buffer_resource(buffer.get(), buff_size, &track_new_del);
	//auto track_monoton = makePrintingMemoryResource("monoton", &monotonic_res);
	////auto uptr = makeUniqueFromResource<int>(&monotonic_res);
	////auto pool = std::pmr::unsynchronized_pool_resource(&monotonic_res);

	//std::cout << "vector:\n";
	////auto vec = std::pmr::vector<std::unique_ptr<bool, PmrResourceDeleter>>(&track_monoton);
	//auto track_monoton2 = makePrintingMemoryResource("vec", &monotonic_res);
	//auto vec = std::pmr::vector<bool*>(&track_monoton2);
	//auto _deleter = ContainerResourceDeleterGuard(vec);
	//auto builder = VectorBuilderWithResource(vec);
	////builder.addUnique<bool>(true);
	////builder.addUnique<bool>(true);
	////builder.addUnique<bool>(true);
	////builder.addUnique<bool>(true);
	//builder.add<bool>(true);
	//builder.add<bool>(true);
	//builder.add<bool>(true);
	//builder.add<bool>(true);
	//std::cout << "build\n";
	//builder.build();
	//std::cout << "map:\n";

	//auto map = std::pmr::map<std::pmr::string, int>(&track_monoton);
	//map.emplace("nush ceva string mai lung", 3);
	//map.emplace("nush ceva string mai lung1", 3);
	//map.emplace("nush ceva string mai lung2", 3);
	//map.emplace("nush ceva string mai lung3", 3);
	//map.emplace("nush ceva string mai lung4", 3);
	//map.emplace("nush ceva string mai lung5", 3);
	//map.emplace("nush ceva string mai lung6", 3);
	////map["nush ceva string mai lung"] = 3;

	{
		auto* type = ark::meta::getMetadata(typeid(ScriptingComponent));
		type->func(ark::SceneInspector::serviceName, renderScriptComponents);
		type->func(ark::serde::serviceSerializeName, serializeScriptComponents);
		type->func(ark::serde::serviceDeserializeName, deserializeScriptComponents);
	}


	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	Engine::create(Engine::resolutionFullHD, "Articifii!", sf::seconds(1 / 120.f), settings);
	Engine::backGroundColor = sf::Color(50, 50, 50);
	Engine::getWindow().setVerticalSyncEnabled(false);
	Engine::registerState<class TestingState>(States::TestingState);
	Engine::registerState<class ImGuiLayer>(States::ImGuiLayer);
	Engine::registerState<class ChessState>(States::ChessState);
	Engine::pushFirstState(States::TestingState);
	Engine::pushOverlay(States::ImGuiLayer);

	Engine::run();

	return 0;
}
