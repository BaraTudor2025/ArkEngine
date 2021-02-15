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

#include <ark/core/Signal.hpp>
#include <ark/core/Engine.hpp>
#include <ark/core/State.hpp>
#include <ark/ecs/EntityManager.hpp>
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
			{.property_name = "scale", .drag_speed = 0.001f}
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

class BasicState : public ark::State {

protected:
	ark::EntityManager manager;
	ark::SystemManager systems;
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
	ark::View<DelayedAction> view;
public:
	void init() override
	{
		view = entityManager.view<DelayedAction>();
	}

	void update() override
	{
		for (auto [entity, da] : view.each()) {
			da.time -= Engine::deltaTime();
			if (da.time <= sf::Time::Zero) {
				if (da.action) {
					auto action = std::move(da.action);
					entity.remove<DelayedAction>();
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
				GameLog("just serialized entity %s", e.get<TagComponent>().name);
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

	ark::Entity makeEntity(std::string name)
	{
		Entity e = manager.createEntity();
		e.get<TagComponent>().name = name;
		return e;
	}

	//Registry cloneRegistry() {
	//	auto newManager = Registry();
	//	for (auto type : this->manager.getComponentTypes())
	//		newManager.addComponentType(type);
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
	//	return std::move(newManager);
	//}

	void init() override
	{
		manager.onCreate().connect<&EntityManager::add<Transform>>();

		manager.onCreate().connect<&EntityManager::add<TagComponent>>();
		manager.onAdd<TagComponent>().connect(TagComponent::onAdd);

		manager.onAdd<ScriptingComponent>().connect(ScriptingComponent::onAdd);
		manager.onCopy<ScriptingComponent>().connect(ScriptingComponent::onClone);

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
		auto* imguiState = getState<ImGuiLayer>();
		imguiState->addTab({ "registry inspector", [=]() { inspector->renderSystemInspector(); } });

		manager.onAdd<LuaScriptingComponent>().connect(LuaScriptingComponent::onAdd, systems.addSystem<LuaScriptingSystem>());

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

		button.add<Button>(sf::FloatRect{ 100, 100, 200, 100 });
		auto& b = button.get<Button>();
		b.setFillColor(sf::Color(240, 240, 240));
		b.setOutlineColor(sf::Color::Black);
		b.setOutlineThickness(3);
		b.moveWithMouse = true;
		b.loadPosition("mama");
		b.setOrigin(b.getSize() / 2.f);
		b.onClick = []() { std::cout << "click "; };
		auto& t = button.add<Text>();
		t.setString("buton");
		t.setOrigin(b.getOrigin());
		t.setPosition(b.getPosition() + b.getSize() / 3.5f);
		t.setFillColor(sf::Color::Black);

		using namespace ParticleScripts;

		rainbowPointParticles.add<PointParticles>(getRainbowParticles());
		{
			auto& scripts = rainbowPointParticles.add<ScriptingComponent>();
			scripts.addScript<SpawnOnRightClick>();
			scripts.addScript(typeid(EmittFromMouse));
		}

#if 1
		ark::Entity rainbowClone = manager.cloneEntity(rainbowPointParticles);
		rainbowClone.get<TagComponent>().name = "rainbow_clone";
		{
			auto& scripts = rainbowClone.get<ScriptingComponent>();
			scripts.getScript<SpawnOnRightClick>()->setActive(false);;
			//auto& scripts = rainbowClone.addComponent<ScriptingComponent>();
			//scripts.addScript<SpawnOnRightClick>();
			//scripts.removeScript(typeid(SpawnOnRightClick));
			//scripts.addScript<SpawnOnLeftClick>()->setActive(false);
			//scripts.addScript<EmittFromMouse>();
		}
		rainbowClone.add<DelayedAction>(sf::seconds(5), [](ark::Entity entity) {
			entity.get<ScriptingComponent>().getScript<SpawnOnRightClick>()->setActive(true);
		});
#endif

		firePointParticles.add<PointParticles>(getFireParticles(1'000));
		{
			auto& scripts = firePointParticles.add<ScriptingComponent>();
			scripts.addScript<SpawnOnLeftClick>();
			scripts.addScript<EmittFromMouse>();
		}
		firePointParticles.add<DelayedAction>(sf::seconds(5), [this](ark::Entity e) {
			manager.destroyEntity(e);
		});


		auto& grassP = greenPointParticles.add<PointParticles>(getGreenParticles());
		grassP.spawn = true;
		{
			auto& scripts = greenPointParticles.add<ScriptingComponent>();
			scripts.addScript<DeSpawnOnMouseClick<>>();
			scripts.addScript<EmittFromMouse>();
			auto& luaScripts = greenPointParticles.add<LuaScriptingComponent>();
			luaScripts.addScript("test.lua");
		}

		rotatingParticles.add<Transform>();
		auto& plimb = rotatingParticles.add<PointParticles>(getRainbowParticles());
		plimb.spawn = true;
		//plimb->emitter = Engine::center();
		//plimb->emitter.x += 100;
		//plimbarica.addScript<Rotate>(360/2, Engine::center());
		{
			auto& scripts = rotatingParticles.add<ScriptingComponent>();
			scripts.addScript<RotateEmitter>(180, Engine::center(), 100);
			scripts.addScript<TraillingEffect>();
		}

		//mouseTrail.addComponent<PointParticles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
		mouseTrail.add(typeid(PointParticles));
		auto& mouseParticles = mouseTrail.get<PointParticles>();
		mouseParticles.setParticleNumber(1000);
		mouseParticles.setLifeTime(sf::seconds(5), DistributionType::normal);
		mouseParticles.speedDistribution = { 0.f, 2.f };
		mouseParticles.angleDistribution = { 0.f, 0.f };
		{
			auto& scripts = mouseTrail.add<ScriptingComponent>();
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
		std::cout << pps.m_size() << std::endl; // 3
		auto cs = parent->getChildrenComponents<Transform>();
		std::cout << cs.m_size() << std::endl; // 1
		auto none = parent->getChildrenComponents<MeshComponent>();
		std::cout << none.m_size() << std::endl; // 0
		auto none2 = child2->getChildrenComponents<PointParticles>();
		std::cout << none2.m_size() << std::endl; // 0
	}

	Entity* parent;
	Entity* child1;
	Entity* child2;
	Entity* grandson;
};
#endif

class LoggerEntityManager {
	std::vector<ScopedConnection> m_conns;
public:
	LoggerEntityManager() = default;
	LoggerEntityManager(ark::EntityManager& man) {
		this->connect(man);
	}
	void connect(ark::EntityManager& man) {
		m_conns.push_back(man.onCreate().connect([](ark::EntityManager&, ark::EntityId entity) {
			EngineLog(LogSource::EntityM, LogLevel::Info, "created entity with id(%d)", entity);
		}));
		m_conns.push_back(man.onDestroy().connect([](ark::EntityManager&, ark::EntityId entity) {
			EngineLog(LogSource::EntityM, LogLevel::Info, "destroyed entity with id(%d)", entity);
		}));
	}
	void disconnect() {
		for (auto& con : m_conns)
			con.release();
	}
};

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

struct ChessPlayerComponent {
	int id;
};

//auto _chess_player_reg_ = [] {
//	ark::meta::registerMetadata<ChessPlayerComponent>();
//	ark::meta::addTypeToGroup(ARK_META_COMPONENT_GROUP, typeid(ChessPlayerComponent));
//	registerServiceDefault<ChessPlayerComponent>();
//	return 0;
//}();
//ARK_MEMBERS(ChessPlayerComponent, 
//	ark::meta::member_property("id", &ChessPlayerComponent::id)
//);

ARK_REGISTER_COMPONENT(ChessPlayerComponent, registerServiceDefault<ChessPlayerComponent>()) { 
	return members<ChessPlayerComponent>(ark::meta::member_property("id", &ChessPlayerComponent::id)); 
}

//template <>
//const auto ark::meta::detail::sMembers<ChessPlayerComponent>
//	= ark::meta::members<ChessPlayerComponent>(ark::meta::member_property("id", &ChessPlayerComponent::id));

//ARK_STATIC_REFLECTION(ChessPlayerComponent) []{
//	ark::meta::registerMetadata<ChessPlayerComponent>();
//	registerServiceDefault<ChessPlayerComponent>();
//	return ark::meta::members<ChessPlayerComponent>();
//}();

struct ChessPieceComponent {
	Entity player;
	sf::Vector2i coord; // position on board
	int type;
	std::function<bool(int, int)> canMoveTo;
};

ARK_REGISTER_COMPONENT(ChessPieceComponent, registerServiceDefault<ChessPieceComponent>()) { 
	return members<ChessPieceComponent>(
	); 
}

void createChessPiece(ark::Registry& manager, sf::Vector2i coord, class ChessSystem* sys, int meshX, bool playerAlb, ark::Entity alb, ark::Entity negru);

struct MousePickUpComponent {
	sf::FloatRect selectArea;
	int filter;
	int dx, dy;
};

ARK_REGISTER_COMPONENT(MousePickUpComponent, registerServiceDefault<MousePickUpComponent>()) { 
	return members<MousePickUpComponent>(
		member_property("selectArea", &MousePickUpComponent::selectArea)
	); 
}

struct MessagePickUp {
	ark::Entity entity;
	bool isPicked;
	bool isReleased;
};

class MousePickUpSystem : public ark::SystemT<MousePickUpSystem> {
	int filters = 0;
	int m_genFlags = 1;
	ark::Entity selectedEntity;
	ark::View<const ark::Transform, MousePickUpComponent> view;

public:
	void init() override {
		view = entityManager.view<const ark::Transform, MousePickUpComponent>();
	}

	void setFilter(int filt = 0) { filters = filt; }

	int generateBitFlag() {
		auto c = m_genFlags;
		m_genFlags <<= 1;
		return c;
	}

	void handleEvent(sf::Event ev) override {
		if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Button::Left) {
			for (auto [entity, pick] : view.each<ark::Entity, MousePickUpComponent>()) {
				if ((pick.filter & filters) == filters && pick.selectArea.contains(ev.mouseButton.x, ev.mouseButton.y)) {
					selectedEntity = entity;
					const auto [x, y] = view.get<const Transform>(entity).getPosition();
					pick.dx = ev.mouseButton.x - x;
					pick.dy = ev.mouseButton.y - y;
				}
			}
		}
		else if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Button::Left) {
			if (selectedEntity) {
				postMessage<MessagePickUp>({
					.entity = selectedEntity,
					.isPicked = false,
					.isReleased = true
				});
				selectedEntity = {};
			}
		}
	}

	void update() override {
		if (selectedEntity) {
			auto [trans, pick] = selectedEntity.get<Transform, const MousePickUpComponent>();
			auto [x, y] = ark::Engine::mousePositon();
			trans.setPosition(x - pick.dx, y - pick.dy);
		}
		// update-ul are sens doar pentru cele cu Transform-ul modificat
		// TODO (ecs) poate adaug un flag m_dirty pentru componente cand le acceses prin ref, fara flag cand sunt 'const'
		for (auto [trans, pick] : view) {
			pick.selectArea.left = trans.getPosition().x;
			pick.selectArea.top = trans.getPosition().y;
		}
	}
};

class ChessSystem : public ark::SystemT<ChessSystem> {
	std::vector<std::vector<ark::Entity>> board;
	ark::Entity playerInTurn;
	//std::vector<ark::Entity> playersQuery;
	EntityQuery<ChessPlayerComponent> playersQuery;

public:

	struct EnforceRules {
		bool notPlayerTurn = true;
	} rules;

	float kPieceMeshSize = 75; // pixeli
	float kBoardOffset = kPieceMeshSize * 2;
	int kBoardLength = 8;

	sf::Vector2i toCoord(sf::Vector2f pos) {
		pos -= sf::Vector2f(kBoardOffset, kBoardOffset);
		pos = pos + sf::Vector2f(kPieceMeshSize / 2, kPieceMeshSize / 2);
		pos /= kPieceMeshSize;
		return sf::Vector2i(pos.x, pos.y);
	}

	sf::Vector2f toPos(sf::Vector2i coord) {
		return sf::Vector2f{ coord.x * kPieceMeshSize, coord.y * kPieceMeshSize } + sf::Vector2f(kBoardOffset, kBoardOffset);
	}

	sf::Vector2f roundPos(sf::Vector2f vec) {
		return toPos(toCoord(vec));
	}

	bool isCoordInBounds(sf::Vector2i vec) {
		return vec.x >= 0 && vec.x < kBoardLength && vec.y >= 0 && vec.y < kBoardLength;
	}

	void nextPlayerTurn() {
		auto it = std::find(playersQuery.begin(), playersQuery.end(), playerInTurn);
		if (it == playersQuery.end() - 1)
			playerInTurn = *playersQuery.begin();
		else
			playerInTurn = *std::next(it);
		auto id = playerInTurn.get<ChessPlayerComponent>().id;
		systemManager.getSystem<MousePickUpSystem>()->setFilter(id);
	}

	void init() override {
		board.resize(kBoardLength);
		for (auto& v : board)
			v.resize(kBoardLength);

		playersQuery.connect(entityManager);

		entityManager.onAdd<ChessPieceComponent>().connect([this](ark::EntityManager& man, ark::EntityId entity) {
			auto& piece = man.get<ChessPieceComponent>(entity);
			this->board[piece.coord.x][piece.coord.y] = Entity{ entity, man };
		});

		auto* pickSystem = systemManager.getSystem<MousePickUpSystem>();
		entityManager.onAdd<ChessPlayerComponent>().connect([this, pickSystem](ark::EntityManager& man, ark::EntityId entity) {
			auto& player = man.get<ChessPlayerComponent>(entity);
			player.id = pickSystem->generateBitFlag();
			if (!this->playerInTurn) {
				this->playerInTurn = this->playersQuery.entities().back();
				pickSystem->setFilter(player.id);
			}
		});
	}

	void handleMessage(const ark::Message& msg) override {
		if (auto* data = msg.tryData<MessagePickUp>(); data && data->isReleased) {
			ark::Entity selectedPiece = data->entity;
			auto [trans, piece] = selectedPiece.get<Transform, ChessPieceComponent>();
			sf::Vector2i newCoord = toCoord(trans.getPosition());
			// is on table
			if (!isCoordInBounds(newCoord)) {
				trans.setPosition(toPos(piece.coord));
				return;
			}
			// is move legal
			if (auto hereEnt = board[newCoord.x][newCoord.y]; hereEnt) {
				if (hereEnt.get<ChessPieceComponent>().player == piece.player) {
					// reset, can't move on top of your piece
					trans.setPosition(toPos(piece.coord));
					return;
				}
				else {
					// take piece
					entityManager.destroyEntity(hereEnt);
				}
			}
			// move
			trans.setPosition(toPos(newCoord));
			board[piece.coord.x][piece.coord.y] = {};
			board[newCoord.x][newCoord.y] = selectedPiece;
			piece.coord = newCoord;
			nextPlayerTurn();
		}
	}

	void update() override { }
};

void createChessPiece(ark::EntityManager& manager, sf::Vector2i coord, ChessSystem* sys, int meshX, bool playerAlb, ark::Entity alb, ark::Entity negru)
{
	Entity entity = manager.createEntity();

	auto& trans = entity.get<Transform>();
	trans.setPosition(coord.x * sys->kPieceMeshSize, coord.y * sys->kPieceMeshSize);
	trans.move(sys->kBoardOffset, sys->kBoardOffset);
	//trans.setOrigin(0, 0);

	int size = sys->kPieceMeshSize;

	auto& mesh = manager.add<MeshComponent>(entity, "chess_pieces.png");
	mesh.setTextureRect(sf::IntRect{ size * meshX, size * playerAlb, size, size });

	ark::Entity player = playerAlb ? alb : negru;

	entity.add<MousePickUpComponent>({
		.selectArea = sf::FloatRect(trans.getPosition().x, trans.getPosition().y, size, size),
		.filter = player.get<ChessPlayerComponent>().id
	});

	entity.add<ChessPieceComponent>(player, coord);
}

/* de implementat moduri */
class ChessState : public BasicState {
	LoggerEntityManager managerLogger;
public:
	ChessState(ark::MessageBus& mb) : BasicState(mb) {}

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

	Entity cloneEnt(ark::Entity entity) {
		Entity clone = manager.createEntity();
		for (auto comp : entity.eachComponent()) {
			clone.add(comp.type, entity);
		}
		return clone;
	}

	void init() override {
		managerLogger.connect(manager);
		manager.onCreate().connect<&EntityManager::add<Transform>>();
		manager.onCreate().connect<&EntityManager::add<TagComponent>>();
		manager.onAdd<TagComponent>().connect(TagComponent::onAdd);

		systems.addSystem<MeshSystem>();
		systems.addSystem<SceneInspector>();
		systems.addSystem<FpsCounterDirector>();
		systems.addSystem<ScriptingSystem>();
		systems.addSystem<RenderSystem>();
		systems.addSystem<MousePickUpSystem>();
		ChessSystem* chessSys = systems.addSystem<ChessSystem>();

		manager.onAdd<ScriptingComponent>().connect(ScriptingComponent::onAdd);
		manager.onCopy<ScriptingComponent>().connect(ScriptingComponent::onClone);
		manager.onAdd<LuaScriptingComponent>().connect(LuaScriptingComponent::onAdd, systems.addSystem<LuaScriptingSystem>());

		auto* imgui = getState<ImGuiLayer>();
		imgui->addTab({ "chess-vars", [chess = chessSys]() {
			// TODO: sa modific si entitatile o data cu ele
			ImGui::DragFloat("board-offset", &chess->kBoardOffset, 0.01f, 0, 0, "%.1f");
			ImGui::InputInt("board-length", &chess->kBoardLength);
			ImGui::DragFloat("piece-mesh-size in pixels", &chess->kPieceMeshSize, 0.01f, 0, 0, "%.1f");
		}});

		const int size = chessSys->kPieceMeshSize;

		Entity playerAlb = manager.createEntity();
		playerAlb.add<ChessPlayerComponent>();

		Entity playerNegru = manager.createEntity();
		playerNegru.add<ChessPlayerComponent>();

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				if (board[i][j] == 0)
					continue;
				int x = std::abs(board[i][j]) - 1;
				int y = board[i][j] > 0 ? 1 : 0;
				createChessPiece(manager, sf::Vector2i{ j, i }, chessSys, x, /*alb*/y, playerAlb, playerNegru);
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

// inspirat de entt::meta si entt::delegate
class any_function {

	std::function<std::any(std::any*)> m_fun;

	template <typename... Args, typename F, std::size_t... Indexes>
	static std::any arkImplInvoke(F fun, std::any* args, std::index_sequence<Indexes...>) {
		return std::invoke(fun, std::any_cast<Args>(args[Indexes])...);
	}

public:

	template <typename... Args>
	std::any operator()(Args&&... args)
	{
		auto anyArgs = std::array<std::any, sizeof...(Args)>{std::forward<Args>(args)...};
		return m_fun(anyArgs.data());
	}

	template <typename... Args, typename F>
	static auto make(F fun) -> any_function
	{
		auto func = any_function();
		func.m_fun = [fun](std::any* args) -> std::any {
			return any_function::arkImplInvoke<Args...>(fun, args, std::index_sequence_for<Args...>{});
		};
		return func;
	}
};

int test(int i) { return i + i; }

		//auto std_each() {
		//	return m_manager->entities 
		//		| std::views::filter([this](const auto& entity) { return (entity.mask & this->m_mask) == this->m_mask; })
		//		| std::views::transform([this](auto& entity) {
		//			return std::tuple<ark::Entity, Cs&...>(ark::Entity(entity.id, this->m_manager), *m_ids.get<Cs>()...);
		//		});
		//}

//template <typename F, typename...Args>
//auto bind_args(F&& fun, Args&&... capt) {
//	return[fun = std::forward<F>(fun), ...capt = std::forward<Args>(capt)](auto&&... args) -> decltype(auto) { 
//		return fun(std::forward<Args>(capt)..., std::forward<decltype(args)>(args)...);
//	};
//}

//void nush() {
//	std::tuple<int, float> tup;
//	std::visit(template<size_t... Index>[](std::index_sequence<Index...>{}, auto&&... vals) {
//
//	}, std::make_index_sequence<std::tuple_size_v<decltype(tup)>>{}, tup);
//}

int main() // are nevoie de c++17 si SFML 2.5.1
{
	auto default_memory_res = makePrintingMemoryResource("Rogue PMR Allocation!", std::pmr::null_memory_resource());
	std::pmr::set_default_resource(&default_memory_res);
	any_function fun = any_function::make<int>(test);
	auto res = fun(48);
	std::cout << std::any_cast<int>(res) << '\n';

	{
		auto signal = Signal<bool(Entity*)>();
		auto sink = Sink{ signal };
		sink.connect<&Entity::isValid>();
		std::cout << signal.size();
		sink.disconnect<&Entity::isValid>();
		std::cout << signal.size();
	}

	{
		auto signal = Signal<bool()>();
		auto sink = Sink{ signal };
		Entity e;
		{
			ScopedConnection conn = sink.connect(&Entity::isValid, &e);
			std::cout << signal.size();
		}
		//conn.release();
		std::cout << signal.size();
	}

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

	Engine::registerState<TestingState>();
	Engine::registerState<ImGuiLayer>();
	Engine::registerState<ChessState>();

	Engine::pushOverlay<ImGuiLayer>();
	Engine::pushFirstState<ChessState>();

	Engine::run();

	return 0;
}
