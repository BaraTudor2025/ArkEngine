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
#include <SFML/Network.hpp>
#include <SFML/Network/TcpSocket.hpp>

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

//import std.core;

using namespace std::literals;
using namespace ark;

enum YellowPlayerAnimations {
	Run = 0,
	Stand
};

class MoveAnimatedPlayer : public ScriptT<MoveAnimatedPlayer, true> {
	AnimationController* animation;
	Transform* transform;
	PixelParticles* runningParticles;
	MeshComponent* mesh;
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
		animation = getComponent<AnimationController>();
		runningParticles = getComponent<PixelParticles>();
		mesh = getComponent<MeshComponent>();
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
			mesh->flipX = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			transform->move(-dx * std::cos(angle), -dx * std::sin(angle));
			runningParticles->angleDistribution = { -PI / 4, PI / 10, DistributionType::normal };
			runningParticles->emitter = transform->getPosition() + particleEmitterOffsetRight;
			moved = true;
			mesh->flipX = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
			transform->move(dx * std::sin(angle), -dx * std::cos(angle));
			moved = true;
			mesh->flipY = false;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
			transform->move(-dx * std::sin(angle), dx * std::cos(angle));
			moved = true;
			mesh->flipY = true;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q))
			transform->rotate(rotationSpeed * dt);
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::E))
			transform->rotate(-rotationSpeed * dt);

		if (moved) {
			animation->play(YellowPlayerAnimations::Run);
			runningParticles->spawn = true;
		}
		else {
			animation->play(YellowPlayerAnimations::Stand);
			runningParticles->spawn = false;
		}
	}
};

ARK_REGISTER_TYPE(gScriptGroupName, MoveAnimatedPlayer, registerServiceDefault<MoveAnimatedPlayer>())
{
	auto* type = ark::meta::type<MoveAnimatedPlayer>();
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

/* TODO (general): TexturedParticles */
/* TODO (general): ColisionSystem */
/* TODO (general): TileSystem */
/* TODO (general): Text(with fade), TextBoxe */
/* TODO (general): MusicSystem/SoundSystem */
/* TODO (general): Shaders */
/* TODO (general): ParticleEmmiter with settings*/
/* TODO (general): add a archetype component manager*/


/* TODO:
	* small vector
	* - optional ref
	* - refactor Util::find
	* - optimize rng
	* - add 'ark' namespace
	* - add serialization(nlohmann json)
	* - add more options to gui editor
	* (part) add scripting with lua(sol3) and/or chaiscript
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

	auto makeEntity(std::string name = "") -> ark::Entity
	{
		Entity entity = manager.createEntity();
		if(!name.empty())
			entity.get<TagComponent>().name = name;
		return entity;
	}

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

struct CameraComponent {
	sf::View view;
};

ARK_REGISTER_COMPONENT_WITH_TAG(sf::View, sfView, registerServiceDefault<sf::View>()) {
	auto type = ark::meta::type<sf::View>();
	type->data(ark::SceneInspector::serviceOptions, std::vector<ark::EditorOptions>{
		{ .property_name = "center", .drag_speed = 0.4f },
		{ .property_name = "rotation", .drag_speed = 0.05f },
		{ .property_name = "size", .drag_speed = 0.4f },
		{ .property_name = "viewport", .options = {
			{.property_name = "top", . drag_speed = 0.00075f, .format = "%.4f" },
			{.property_name = "left", . drag_speed = 0.00075f, .format = "%.4f"},
			{.property_name = "height", . drag_speed = 0.001f, .drag_min = 0.01f, .drag_max = 1, .format = "%.3f"},
			{.property_name = "width", . drag_speed = 0.001f, .drag_min = 0.01f, .drag_max = 1, .format = "%.3f"}
		} }
	});

	return ark::meta::members<sf::View>(
		ark::meta::member_property("center", &sf::View::getCenter, &sf::View::setCenter),
		ark::meta::member_property("rotation", &sf::View::getRotation, &sf::View::setRotation),
		ark::meta::member_property("size", &sf::View::getSize, &sf::View::setSize),
		ark::meta::member_property("viewport", &sf::View::getViewport, &sf::View::setViewport)
	);
}

ARK_REGISTER_COMPONENT(CameraComponent, registerServiceDefault<CameraComponent>()) {
	return ark::meta::members<CameraComponent>(
		ark::meta::member_property("view", &CameraComponent::view)
	);
}

class CameraSystem : public ark::SystemT<CameraSystem>, public ark::Renderer {
public:
	void init() override {
		entityManager.onAdd<CameraComponent>().connect([](ark::EntityManager& manager, ark::EntityId entity) {
			// init with current view
			manager.get<CameraComponent>(entity).view = ark::Engine::getWindow().getView();
		});
	}

	void update() override {}

	void render(sf::RenderTarget& win) override { 
		for (auto& comp : entityManager.view<CameraComponent>()) {
			win.setView(comp.view);
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

public:
	TestingState(ark::MessageBus& mb) : BasicState(mb) {}

private:

	void init() override
	{
		manager.onCreate().connect<&EntityManager::add<Transform>>();

		manager.onCreate().connect<&EntityManager::add<TagComponent>>();
		manager.onAdd<TagComponent>().connect(TagComponent::onAdd);

		manager.onAdd<ScriptingComponent>().connect(ScriptingComponent::onAdd);
		manager.onClone<ScriptingComponent>().connect(ScriptingComponent::onClone);

		manager.onAdd<AnimationController>().connect(AnimationController::onAdd);

		systems.addSystem<PointParticleSystem>();
		systems.addSystem<PixelParticleSystem>();
		systems.addSystem<ButtonSystem>();
		systems.addSystem<TextSystem>();
		systems.addSystem<MeshSystem>();
		systems.addSystem<AnimationSystem>();
		systems.addSystem<DelayedActionSystem>();
		systems.addSystem<ScriptingSystem>();
		systems.addSystem<CameraSystem>();

		systems.addSystem<FpsCounterDirector>();
		auto* inspector = systems.addSystem<SceneInspector>();
		auto* imguiState = getState<ImGuiLayer>();
		imguiState->addTab({ "registry inspector", [=]() { inspector->renderSystemInspector(); } });

		auto luaSys = systems.addSystem<LuaScriptingSystem>();
		manager.onAdd<LuaScriptingComponent>().connect(LuaScriptingComponent::onAdd, luaSys);

		button = makeEntity("button");
		mouseTrail = makeEntity("mouse_trail");
		rainbowPointParticles = makeEntity("rainbow_ps");
		greenPointParticles = makeEntity("green_ps");
		firePointParticles = makeEntity("fire_ps");
		rotatingParticles = makeEntity("rotating_ps");

		player = makeEntity("player");
		//ark::serde::deserializeEntity(player);
		{
			player.add<MeshComponent>("chestie.png");
			player.add<AnimationController>(2, 6);
			makeAnimation(player, YellowPlayerAnimations::Stand, 6).framerate = sf::milliseconds(125);
			makeAnimation(player, YellowPlayerAnimations::Run, 6).framerate = sf::milliseconds(75);

			player.get<AnimationController>().play(1);

			/**/
			auto& pp = player.add<PixelParticles>(100, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
			auto& playerScripting = player.add<ScriptingComponent>();
			auto moveScript = playerScripting.addScript<MoveAnimatedPlayer>(400, 180);

			auto& trans = player.get<ark::Transform>();
			trans.move(Engine::center());
			trans.setOrigin(player.get<MeshComponent>().getMeshSize() / 2.f);

			pp.particlesPerSecond = pp.getParticleNumber() / 2;
			pp.size = { 60, 30 };
			pp.speed = moveScript->speed;
			pp.emitter = trans.getPosition() + sf::Vector2f{ 50, 40 };
			pp.gravity = { 0, moveScript->speed };
			auto [w, h] = Engine::windowSize();
			pp.platform = { Engine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };

			moveScript->setScale({ 0.2f, 0.2f });

			//player.add<DelayedAction>(sf::seconds(4), [](Entity e) {
			//	GameLog("just serialized entity %s", e.getComponent<TagComponent>().name);
			//	ark::serde::serializeEntity(e);
			//});
			/**/
		}

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
			/// <summary>
			/// AIDI
			/// </summary>
			scripts.addScript(typeid(EmittFromMouse));
		}

#if 1
		ark::Entity rainbowClone = manager.clone(rainbowPointParticles);
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
		mouseParticles.colorLowerBound = mouseParticles.colorUpperBound = sf::Color::White;
		mouseParticles.setParticleNumber(1000);
		mouseParticles.setLifeTime(sf::seconds(5), DistributionType::normal);
		mouseParticles.speedDistribution = { 0.f, 2.f };
		mouseParticles.angleDistribution = { 0.f, 0.f };
		{
			auto& scripts = mouseTrail.add<ScriptingComponent>();
			scripts.addScript<EmittFromMouse>();
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

class LoggerEntityManager {
	std::vector<ScopedConnection> m_conns;
public:
	LoggerEntityManager() = default;
	LoggerEntityManager(ark::EntityManager& man) {
		this->connect(man);
	}
	void connect(ark::EntityManager& man) {
		m_conns.push_back(man.onCreate().connect([](ark::EntityManager&, ark::EntityId entity) {
			EngineLog(LogSource::EntityM, LogLevel::Info, "create entity-id(%d)", entity);
		}));
		m_conns.push_back(man.onDestroy().connect([](ark::EntityManager&, ark::EntityId entity) {
			EngineLog(LogSource::EntityM, LogLevel::Info, "destroy entity-id(%d)", entity);
		}));
		//m_conns.push_back(man.onAdd().connect([](ark::EntityManager&, ark::EntityId entity, std::type_index type) {
		//	EngineLog(LogSource::EntityM, LogLevel::Info, "add component (%s) on entity-id(%d)", type.name(), entity);
		//}));
		//m_conns.push_back(man.onRemove().connect([](ark::EntityManager&, ark::EntityId entity, std::type_index type) {
		//	EngineLog(LogSource::EntityM, LogLevel::Info, "remove component (%s) on entity-id(%d)", type.name(), entity);
		//}));
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
	std::function<std::vector<sf::Vector2i>(sf::Vector2i)> generateMoves;
};

struct ChessEnPassantTag { 
	sf::Vector2i behindCoord;
};

ARK_REGISTER_COMPONENT(ChessPieceComponent, registerServiceDefault<ChessPieceComponent>()) { 
	return members<ChessPieceComponent>(
	); 
}
ARK_REGISTER_COMPONENT(ChessEnPassantTag, registerServiceDefault<ChessEnPassantTag>()) { 
	return members<ChessEnPassantTag>(
	); 
}

void createChessPiece(ark::Registry& manager, sf::Vector2i coord, class ChessSystem* sys, int meshX, bool playerAlb, ark::Entity alb, ark::Entity negru);

struct MousePickUpComponent {
	sf::FloatRect selectArea;
	int filter;
	int dx, dy;
	bool setTransform = true;
	// pentru drag: tine apasat si dai drumul
	// pentru pickup(drag=false): click sa selectezi, click sa-i dai drumul
	bool drag = true;
};

ARK_REGISTER_COMPONENT(MousePickUpComponent, registerServiceDefault<MousePickUpComponent>()) { 
	return members<MousePickUpComponent>(
		member_property("selectArea", &MousePickUpComponent::selectArea)
	); 
}

struct MessagePickUp {
	ark::Entity entity{};
	sf::Vector2f mousePosition{}; // setat doar daca este released
	bool isReleased{}; // or picked up
	bool isSameSpot = false; // is put on same spot
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

	void setFilter(int bitFlags = 0) { filters = bitFlags; }
	void setFilter(ark::Entity entity) { filters = entity.get<MousePickUpComponent>().filter; }
	void reset() { selectedEntity.reset(); }

	int generateBitFlag() {
		auto c = m_genFlags;
		m_genFlags <<= 1;
		return c;
	}

	void handleEvent(sf::Event ev) override {
		if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Button::Left) {
			if (selectedEntity) {
				const auto& pick = selectedEntity.get<const MousePickUpComponent>();
				if (!pick.drag) { // pickup-drop
					auto [x, y] = ark::Engine::mousePositon();
					if (pick.setTransform) {
						auto& trans = selectedEntity.get<ark::Transform>();
						trans.setPosition(x - pick.dx, y - pick.dy);
					}
					bool isSameSpot = pick.selectArea.contains(x, y);
					postMessage<MessagePickUp>({
						.entity = selectedEntity,
						.mousePosition = {x, y},
						.isReleased = true,
						.isSameSpot = isSameSpot
					});
					selectedEntity = {};
					// ca sa nu intre in for-each-view
					if (isSameSpot)
						return;
				}
			}
			for (auto [entity, pick] : view.each<ark::Entity, MousePickUpComponent>()) {
				if ((pick.filter & filters) == filters && pick.selectArea.contains(ev.mouseButton.x, ev.mouseButton.y)) {
					selectedEntity = entity;
					const auto [x, y] = view.get<const Transform>(entity).getPosition();
					pick.dx = ev.mouseButton.x - x;
					pick.dy = ev.mouseButton.y - y;
					postMessage<MessagePickUp>({
						.entity = selectedEntity,
						.mousePosition = ark::Engine::mousePositon(),
						.isReleased = false
					});
				}
			}
		}
		else if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Button::Left) {
			if (selectedEntity) {
				const auto& pick = selectedEntity.get<const MousePickUpComponent>(); 
				if (pick.drag) {
					auto [x, y] = selectedEntity.get<const ark::Transform>().getPosition();
					postMessage<MessagePickUp>({
						.entity = selectedEntity,
						.mousePosition = ark::Engine::mousePositon(),
						.isReleased = true
					});
					selectedEntity = {};
				}
			}
		}
	}

	void update() override {
		if (selectedEntity) {
			const auto& pick = selectedEntity.get<const MousePickUpComponent>();
			if (pick.drag && pick.setTransform) {
				auto& trans = selectedEntity.get<ark::Transform>();
				auto [x, y] = ark::Engine::mousePositon();
				trans.setPosition(x - pick.dx, y - pick.dy);
			}
		}
		// update-ul are sens doar pentru cele cu Transform-ul modificat
		// TODO (ecs) poate adaug un flag m_dirty pentru componente cand le acceses prin ref, fara flag cand sunt 'const'
		for (auto [trans, pick] : view) {
			pick.selectArea.left = trans.getPosition().x;
			pick.selectArea.top = trans.getPosition().y;
		}
	}
};

// id unic intr-un joc multiplayer, adica id-ul este consistent in retea 
// si poate fi folosit pentru ne referi la o entitate cand trimitem informatii destre entitati
struct NetworkIdComponent {
	int id;
};

std::string_view socketStatusStr(sf::Socket::Status status) {
	switch (status) {
	case sf::Socket::Done:
		return "Done";
	case sf::Socket::NotReady:
		return "NotReady";
	case sf::Socket::Partial:
		return "Partial";
	case sf::Socket::Disconnected:
		return "Disconnected";
	case sf::Socket::Error:
		return "Error";
	default:
		return "Invalid Status Code";
	}
}

enum class ServerOpType : std::int8_t {
	AcceptConnection,
	RejectConnection,
	CreateEntity,
	DestroyEntity,
	AddComponent,
	UpdateComponent,
	RemoveComponent,
};

enum class ClientOpType : std::int8_t {
	Connect,
	Disconnect,
	CreateEntity,
	DestroyEntity,
	SendData
};

template <typename T> 
requires std::is_enum_v<T>
sf::Packet& operator<<(sf::Packet& pack, T enumVal) {
	pack << (static_cast<std::underlying_type_t<T>>(enumVal));
	return pack;
}

template <typename T>
requires std::is_enum_v<T>
sf::Packet& operator>>(sf::Packet& pack, T& enumVal) {
	auto val = static_cast<std::underlying_type_t<T>>(enumVal);
	pack >> val;
	enumVal = static_cast<T>(val);
	return pack;
}

class NetworkSystem : public ark::SystemT<NetworkSystem> {
	std::unordered_map<int, std::function<void(sf::Packet&)>> m_handlers;
	bool m_isServer = false;
	int m_idCounter = 0;
	//std::vector<ark::EntityId> m_entities; // index is the 'id' from NetworkIdComponent

	void send(sf::Packet& packet) {
		while (sock.send(packet) == sf::Socket::Status::Partial) {
			sock.send(packet);
		}
	}
	//std::vector<sf::TcpSocket> m_clients;
	sf::TcpSocket m_client;
	sf::TcpSocket m_server;

public:

	sf::TcpSocket sock;
	sf::TcpListener listener;
	char buffIpAddr[20];
	const std::uint16_t port = 12025;

	/* only the server creates entities?
	 * system to broadcast/send components: transform/rigidBody?
	 * generic derived system type that de/serrializes and sends components
	 * 
	*/
	// send requestToServer to create entity with components
	// server also allocs a NetworkIdComponent
	// void requestEntity<Comps...>();

	// all clients get the requested entity by a client
	void requestEntity() {
		if (!m_isServer) {
			sf::Packet packet;
			packet << ClientOpType::CreateEntity;
		}
	}

	void init() override { 
		if (m_isServer) {
			entityManager.onCreate().connect([this](ark::EntityManager& man, ark::EntityId entity) {
				man.add<NetworkIdComponent>(entity).id = m_idCounter++;
				sf::Packet pack;
				pack << ServerOpType::CreateEntity << m_idCounter - 1;
				this->send(pack);
				//assign id and send to clients
				// request create entity catre server, serverul da net-id-ul pe care-l trimite la clienti
			});
			entityManager.onDestroy().connect([this](ark::EntityManager& man, ark::EntityId entity) {
				int netId = man.get<NetworkIdComponent>(entity).id;
				sf::Packet pack;
				pack << ServerOpType::DestroyEntity << netId;
				this->send(pack);
			});
		}
		//entityManager.onAdd().connect([](ark::EntityManager& man, ark::EntityId entity, std::type_index type){ 
		//	// send to clients the added component
		//})
		std::memset(buffIpAddr, 0, sizeof(buffIpAddr));
		sock.setBlocking(false);
		listener.setBlocking(false);
		listener.listen(this->port);
	}

	template <typename T>
	requires requires(T x) { static_cast<int>(x); }
	void addHandle(T id, std::function<void(sf::Packet&)> func) {
		m_handlers[static_cast<int>(id)] = func;
	}

	template <typename... Ts>
	void sendComponent(ark::Entity entity) {
		assert(entityManager.has<Ts>() && ...);
		// serialize and send packet
	}

	template <typename F>
	void send(F&& fun) {
		sf::Packet packet;
		packet << ClientOpType::SendData;
		fun(packet);
		this->send(packet);
	}

	void update() override {
		if (sock.getRemoteAddress() == sf::IpAddress::None) {
			m_isServer = false;
			if (listener.accept(sock) == sf::Socket::Done) {
				std::string str = sock.getRemoteAddress().toString();
				m_isServer = true;
				listener.close();
			}
		}
		sf::Packet packet;
		sf::SocketSelector selector;
		switch (auto status = sock.receive(packet)) {
		case sf::Socket::Status::Done:
			ClientOpType opType;
			packet >> opType;
			if (opType == ClientOpType::SendData) {
				int id;
				packet >> id;
				m_handlers.at(id)(packet);
			}
			else {

			}
			break;
		default:
			//GameLog("[socket-recv]: %s", socketStatusStr(status));
			break;
		//case sf::Socket::Status::Disconnected:
		//	GameLog("[socket-recv]: disconected");
		//	break;
		//case sf::Socket::Status::Error:
		//	GameLog("[socket-recv]: error");
		//	break;
		//case sf::Socket::Status::NotReady:
		//	GameLog("[socket-recv]: not ready");
		//	break;
		//case sf::Socket::Status::Partial:
		//	GameLog("[socket-recv]: partial");
		//	break;
		}

		//int recivedId = 0;
		//nlohmann::json in;

		/* il gasim prin indexare */
		//ark::EntityId entity = m_entities[recivedId];

		/* sau cu view */
		//entityManager.view<NetworkIdComponent>().each([&, this](ark::Entity entity, NetworkIdComponent netComp) {
		//	if (netComp.id == recivedId) {
		//		// or deserializeComponent
		//		//ark::serde::deserializeEntity(in, entity);
		//	}
		//});
	}
};

class ChessSystem : public ark::SystemT<ChessSystem>, public ark::Renderer {
	std::vector<std::vector<ark::Entity>> board;
	EntityQuery<ChessPlayerComponent> playersQuery;
	std::vector<sf::Vector2i> movesToDraw;
	sf::Vector2i origianlPosToDraw;
	ark::Entity selectedPiece;
	NetworkSystem* netSystem;

	enum class Operation {
		Move, Select, UnSelect
	};

public:
	ark::Entity playerInTurn;

	struct EnforceRules {
		bool notPlayerTurn = true;
	} rules;

	float kPieceSize = 75; // pixeli
	sf::Vector2f kBoardOffset = { 150, 150 };
	int kBoardLength = 8;

	bool rulePlayerTurn = true;
	bool ruleLegalMoves = true;
	bool ruleShowMoves = true;
	bool ruleWarnKingOnAttack = true;
	bool ruleMoveKingOnAttack = true;

	sf::Vector2i toCoord(sf::Vector2f pos) {
		pos = (pos - kBoardOffset /*+ (kPieceSize/2) pentru 'withOrigin/centru'*/) / kPieceSize;
		return sf::Vector2i(pos.x, pos.y);
	}

	sf::Vector2f toPos(sf::Vector2i coord) {
		return sf::Vector2f{ coord.x * kPieceSize, coord.y * kPieceSize } + kBoardOffset;
	}

	bool isOutOfBounds(sf::Vector2i coord) {
		return coord.x < 0 || coord.y < 0 || coord.x >= kBoardLength || coord.y >= kBoardLength;
	}

	void nextPlayerTurn() {
		if (!rulePlayerTurn) {
			systemManager.getSystem<MousePickUpSystem>()->setFilter(0);
			return;
		}
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

		netSystem = systemManager.getSystem<NetworkSystem>();
		netSystem->addHandle(Operation::Move, [this](sf::Packet& packet) {
			sf::Vector2i newCoord;
			packet >> newCoord.x >> newCoord.y;
			GameLog("[Move]: %d, %d", newCoord.x, newCoord.y);
			this->handlePieceMove(newCoord);
		});
		netSystem->addHandle(Operation::Select, [this](sf::Packet& packet) {
			sf::Vector2i coord;
			packet >> coord.x >> coord.y;
			GameLog("[Select]: %d, %d", coord.x, coord.y);
			this->handlePieceSelect(board[coord.x][coord.y]);
		});
		netSystem->addHandle(Operation::UnSelect, [this](sf::Packet& packet) {
			selectedPiece.reset();
			GameLog("[de-select]");
		});

		entityManager.onAdd<ChessPieceComponent>().connect([this](ark::EntityManager& man, ark::EntityId entity) {
			auto& piece = man.get<ChessPieceComponent>(entity);
			board[piece.coord.x][piece.coord.y] = Entity{ entity, man };
		});

		auto* pickSystem = systemManager.getSystem<MousePickUpSystem>();
		entityManager.onAdd<ChessPlayerComponent>().connect([this, pickSystem](ark::EntityManager& man, ark::EntityId entity) {
			auto& player = man.get<ChessPlayerComponent>(entity);
			player.id = pickSystem->generateBitFlag();
			if (!playerInTurn) {
				playerInTurn = playersQuery.entities().back();
				pickSystem->setFilter(player.id);
			}
		});
	}

	enum class BoxType {
		Empty,
		Ally,
		Enemy,
		OutOfBounds
	};

	//
	// note: returneaza ally si pentru propria piesa
	BoxType typeOfBox(sf::Vector2i coord) {
		if (isOutOfBounds(coord))
			return BoxType::OutOfBounds;
		else if (auto entity = board[coord.x][coord.y]; !entity)
			return BoxType::Empty;
		else if (auto& piece = entity.get<ChessPieceComponent>(); piece.player != playerInTurn)
			return BoxType::Enemy;
		else
			return BoxType::Ally;
	}

	// legal move for empty or enemy
	// break on non empty
	template <class R, class V, class F>
	void forDirection(R& moves, V coord, F dir) {
		dir(coord);
		for (int i = 0; i < board.size(); i++, dir(coord)) {
			auto type = typeOfBox(coord);
			if (type == BoxType::Empty || type == BoxType::Enemy)
				moves.push_back(coord);
			if (type != BoxType::Empty)
				break;
		}
	}

	void handlePieceMove(sf::Vector2i newCoord) {
		auto [trans, piece] = selectedPiece.get<Transform, ChessPieceComponent>();
		// reset if move is ilegal
		if (ruleLegalMoves && movesToDraw.end() == std::find(movesToDraw.begin(), movesToDraw.end(), newCoord)) {
			selectedPiece = {};
			trans.setPosition(toPos(piece.coord));
			return;
		}
		//using enum BoxType;
		switch (auto type = typeOfBox(newCoord); type) {
		// reset
		case BoxType::OutOfBounds:
		case BoxType::Ally:
			trans.setPosition(toPos(piece.coord));
			break;

		case BoxType::Enemy:
			entityManager.destroyEntity(board[newCoord.x][newCoord.y]);
		[[fallthrough]];
		// move
		case BoxType::Empty:
			entityManager.view<ChessEnPassantTag>().each([&](ark::Entity markedEntity, ChessEnPassantTag& enpass) {
				if (selectedPiece.get<ChessPieceComponent>().player != markedEntity.get<ChessPieceComponent>().player) {
					if (newCoord == enpass.behindCoord) // daca am pus pionul in spate
						entityManager.destroyEntity(markedEntity);
					else
						markedEntity.remove<ChessEnPassantTag>();
				}
			});
			trans.setPosition(toPos(newCoord));
			board[piece.coord.x][piece.coord.y] = {};
			board[newCoord.x][newCoord.y] = selectedPiece;
			piece.coord = newCoord;
			nextPlayerTurn();
			break;
		}
		selectedPiece = {};
	}

	void handlePieceSelect(ark::Entity entity) {
		selectedPiece = entity;
		auto& piece = selectedPiece.get<const ChessPieceComponent>();
		origianlPosToDraw = piece.coord;
		// ca typeOfBox sa returneze corect Enemy sau Ally
		if (!rulePlayerTurn) {
			playerInTurn = piece.player;
		}
		if(ruleLegalMoves || ruleShowMoves)
			movesToDraw = piece.generateMoves(piece.coord);
	}

	void handleMessage(const ark::Message& msg) override {
		if (auto* data = msg.tryData<MessagePickUp>(); !data) {
			return;
		}
		// un-select
		else if (data->isSameSpot) {
			selectedPiece = {};
			netSystem->send([&](sf::Packet& packet) {
				packet << (int)Operation::UnSelect;
			});
		}
		// selected
		else if (!data->isReleased) { 
			handlePieceSelect(data->entity);
			auto [x, y] = data->entity.get<ChessPieceComponent>().coord;
			netSystem->send([&](sf::Packet& packet) {
				packet << (int)Operation::Select << x << y;
			});
		}
		// try to moves
		else if (data->isReleased) { 
			sf::Vector2i newCoord = toCoord(data->mousePosition);
			handlePieceMove(newCoord);
			netSystem->send([&](sf::Packet& packet) {
				packet << (int)Operation::Move << newCoord.x << newCoord.y;
			});
		} 
	}

	void update() override {
	}

	void render(sf::RenderTarget& win) override {
		if (selectedPiece) {
			sf::CircleShape circle;
			const auto ksize = kPieceSize / 8;
			const auto kcenter = kPieceSize / 2;
			circle.setOrigin(ksize, ksize);
			circle.setPosition(toPos(origianlPosToDraw));
			circle.move(kcenter, kcenter);
			circle.setRadius(ksize);
			auto red = sf::Color::Red;
			red.a = 200;
			circle.setFillColor(red);
			win.draw(circle);

			if (ruleShowMoves) {
				auto yellow = sf::Color::Yellow;
				yellow.a = 200;
				circle.setFillColor(yellow);
				for (auto move : this->movesToDraw) {
					circle.setPosition(toPos(move));
					circle.move(kcenter, kcenter);
					win.draw(circle);
				}
			}
		}
	}
};

auto generateMovesDiagonal(ChessSystem* sys, sf::Vector2i coord) {
	auto moves = std::vector<sf::Vector2i>();
	sys->forDirection(moves, coord, [](auto& coord) { coord.x--; coord.y--; });
	sys->forDirection(moves, coord, [](auto& coord) { coord.x++; coord.y++; });
	sys->forDirection(moves, coord, [](auto& coord) { coord.x--; coord.y++; });
	sys->forDirection(moves, coord, [](auto& coord) { coord.x++; coord.y--; });
	return moves;
}

auto generateMovesLine(ChessSystem* sys, sf::Vector2i coord) {
	auto moves = std::vector<sf::Vector2i>();
	sys->forDirection(moves, coord, [](auto& coord) { coord.x++; });
	sys->forDirection(moves, coord, [](auto& coord) { coord.x--; });
	sys->forDirection(moves, coord, [](auto& coord) { coord.y++; });
	sys->forDirection(moves, coord, [](auto& coord) { coord.y--; });
	return moves;
}

auto generateMovesQueen(ChessSystem* sys, sf::Vector2i coord) {
	auto vec = generateMovesDiagonal(sys, coord);
	auto vec2 = generateMovesLine(sys, coord);
	vec.insert(vec.end(), vec2.begin(), vec2.end());
	return vec;
}

template <typename R>
auto generateMovesWithPredef(ChessSystem* sys, const R& predef, sf::Vector2i coord) {
	auto moves = std::vector<sf::Vector2i>();
	for (sf::Vector2i move : predef) {
		if (auto type = sys->typeOfBox(move); type == ChessSystem::BoxType::Empty || type == ChessSystem::BoxType::Enemy)
			moves.push_back(move);
	}
	return moves;
}

auto generateMovesHorse(ChessSystem* sys, sf::Vector2i coord) {
	const auto predef = std::array{
		coord + sf::Vector2i{2, 1},
		coord + sf::Vector2i{2, -1},
		coord + sf::Vector2i{-2, 1},
		coord + sf::Vector2i{-2, -1},
		coord + sf::Vector2i{1, 2}, 
		coord + sf::Vector2i{1, -2}, 
		coord + sf::Vector2i{-1, 2}, 
		coord + sf::Vector2i{-1, -2}, 
	};
	return generateMovesWithPredef(sys, predef, coord);
}

auto generateMovesKing(ChessSystem* sys, sf::Vector2i coord) {
	const auto predef = std::array{
		coord + sf::Vector2i{0, 1},
		coord + sf::Vector2i{0, -1},
		coord + sf::Vector2i{1, 0},
		coord + sf::Vector2i{-1, 0},
		coord + sf::Vector2i{1, 1},
		coord + sf::Vector2i{1, -1},
		coord + sf::Vector2i{-1, 1},
		coord + sf::Vector2i{-1, -1},
	};
	return generateMovesWithPredef(sys, predef, coord);
}

auto generateMovesPawn(ChessSystem* sys, ark::EntityManager& manager, ark::Entity entity, sf::Vector2i firstCoord, auto dir, const auto origCoord)
{
	auto moves = std::vector<sf::Vector2i>();
	auto coord = origCoord;
	auto pushIf = [&](auto boxType) {
		if (auto type = sys->typeOfBox(coord); type == boxType)
			moves.push_back(coord);
	};
	// prima casuta/fata
	dir(coord);
	pushIf(ChessSystem::BoxType::Empty);
	// note: pentru sah in 4 tre sa fac si {coord.y -=1; coord.y += 2}
	// stanga
	coord.x -= 1;
	pushIf(ChessSystem::BoxType::Enemy);
	// dreapta
	coord.x += 2;
	pushIf(ChessSystem::BoxType::Enemy);

	// vezi daca poate face en-passant la o piesa marcata
	for (auto [markedEntity, enpass, markedPiece] : manager.view<ChessEnPassantTag, ChessPieceComponent>().each()) {
		auto coord = origCoord;
		// daca n-a miscat doua patratele
		if (enpass.behindCoord == markedPiece.coord) {
			markedEntity.remove<ChessEnPassantTag>();
			continue;
		}
		// stanga
		coord.x -= 1;
		if (markedPiece.coord == coord && sys->typeOfBox(coord) == ChessSystem::BoxType::Enemy) {
			dir(coord);
			moves.push_back(coord);
		}
		// dreapta
		coord.x += 2;
		if (markedPiece.coord == coord && sys->typeOfBox(coord) == ChessSystem::BoxType::Enemy) {
			dir(coord);
			moves.push_back(coord);
		}
	}
	// la prima mutare pot muta doua
	if (firstCoord == entity.get<ChessPieceComponent>().coord) {
		auto coord = origCoord;
		dir(coord);
		if (sys->typeOfBox(coord) == ChessSystem::BoxType::Empty) {
			// este tag-uit chiar daca nu a facut miscarea de doua patratele,
			// si-i ok, merge si asa.
			// nu-i ok, am un bug.
			// care era bug-ul? cand incepe negru?
			auto prevCoord = coord;
			dir(coord);
			if (sys->typeOfBox(coord) == ChessSystem::BoxType::Empty) {
				entity.add<ChessEnPassantTag>().behindCoord = prevCoord;
				moves.push_back(coord);
			}
		}
	}
	return moves;
};

void createChessPiece(ark::EntityManager& manager, sf::Vector2i coord, ChessSystem* sys, int meshX, bool playerAlb, ark::Entity alb, ark::Entity negru)
{
	Entity entity = manager.createEntity();

	auto& trans = entity.get<Transform>();
	trans.setPosition(coord.x * sys->kPieceSize, coord.y * sys->kPieceSize);
	trans.move(sys->kBoardOffset);

	auto& mesh = manager.add<MeshComponent>(entity, "chess_pieces.png");
	int meshSize = mesh.getMeshSize().x / 6; /* 6 = numarul de piese in mesh */
	mesh.setTextureRect(sf::IntRect{ meshSize * meshX, meshSize * (!playerAlb), meshSize, meshSize });
	trans.setScale(sys->kPieceSize / meshSize, sys->kPieceSize / meshSize);

	ark::Entity player = playerAlb ? alb : negru;

	entity.add<MousePickUpComponent>({
		.selectArea = sf::FloatRect(trans.getPosition().x, trans.getPosition().y, sys->kPieceSize, sys->kPieceSize),
		.filter = player.get<ChessPlayerComponent>().id,
		.setTransform = false,
		.drag = false
	});

	auto generateMovesPawnLocal = [sys, &manager, entity, firstCoord = coord](auto dir, const auto origCoord) mutable {
		return generateMovesPawn(sys, manager, entity, firstCoord, dir, origCoord);
	};

	auto pieceType = meshX + 1;

	entity.add<ChessPieceComponent>({
		.player = player,
		.coord = coord,
		.type = pieceType,
		.generateMoves = [&] () mutable -> std::function<std::vector<sf::Vector2i>(sf::Vector2i coord)> { 
			if (pieceType == 1)
				return ark::bind_front(generateMovesKing, sys); // TODO: add rocada
			else if (pieceType == 2)
				return ark::bind_front(generateMovesQueen, sys);
			else if (pieceType == 3)
				return ark::bind_front(generateMovesDiagonal, sys);
			else if (pieceType == 4)
				return ark::bind_front(generateMovesHorse, sys);
			else if (pieceType == 5)
				return ark::bind_front(generateMovesLine, sys);
			else if (playerAlb)
				return ark::bind_front(generateMovesPawnLocal, [](auto& coord) {coord.y--; });
			else
				return ark::bind_front(generateMovesPawnLocal, [](auto& coord) {coord.y++; });
		}()
	});
}

struct FilterComponent {
	int flagsAll;
	int flagsNone;
};

template <typename F>
void forFilter(ark::EntityManager& man, FilterComponent filter, F&& fun) {
	man.view<FilterComponent>().each([&](ark::Entity entity, FilterComponent& comp) {
		const bool all = (comp.flagsAll & filter.flagsAll) == filter.flagsAll;
		const bool exc = !(comp.flagsNone & filter.flagsNone);
		if (all & exc) {
			fun(entity);
		}
	});
}

auto& getTrackRes() {
	static MallocResource mallocRes;
	static TrackingResource trackRes(&mallocRes);
	return trackRes;
}

/* TODO(chess): de implementat moduri */
class ChessState : public BasicState {
	LoggerEntityManager managerLogger;
public:
	ChessState(ark::MessageBus& mb) : BasicState(mb) {}

	std::vector<std::vector<int>> board = 
	{	{-5, -4, -3, -2, -1, -3, -4, -5},
		{-6, -6, -6, -6, -6, -6, -6, -6},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{6, 6, 6, 6, 6, 6, 6, 6},
		{5, 4, 3, 2, 1, 3, 4, 5},
	};


	void init() override {
		managerLogger.connect(manager);
		//manager.addType<ark::TagComponent>();
		//manager.addType<ark::Transform>();
		manager.onCreate().connect<&EntityManager::add<TagComponent>>();
		manager.onCreate().connect<&EntityManager::add<Transform>>();
		manager.onAdd<TagComponent>().connect(TagComponent::onAdd);

		systems.addSystem<NetworkSystem>();
		systems.addSystem<MousePickUpSystem>();
		systems.addSystem<MeshSystem>();
		systems.addSystem<SceneInspector>();
		systems.addSystem<FpsCounterDirector>();
		systems.addSystem<ScriptingSystem>();
		systems.addSystem<RenderSystem>();
		//systems.addSystem<LuaScriptingSystem>();
		ChessSystem* chessSys = systems.addSystem<ChessSystem>();

		manager.onAdd<ScriptingComponent>().connect(ScriptingComponent::onAdd);
		manager.onClone<ScriptingComponent>().connect(ScriptingComponent::onClone);
		manager.onAdd<LuaScriptingComponent>().connect(LuaScriptingComponent::onAdd, systems.getSystem<LuaScriptingSystem>());

		Entity boardEntity = makeEntity("chess-board");
		boardEntity.add<MeshComponent>("chess_board.png");
		auto setBoardSize = [=, this](float pieceSize) mutable {
			const auto& mesh = boardEntity.get<const MeshComponent>();
			auto& trans = boardEntity.get<ark::Transform>();
			auto scale = (pieceSize * chessSys->kBoardLength) / mesh.getMeshSize().x;
			trans.setPosition(chessSys->kBoardOffset);
			trans.setScale(scale, scale);
		};
		setBoardSize(chessSys->kPieceSize);

		getState<ImGuiLayer>()->addTab("chess settings", [=, this]() mutable {
			{
				//auto bytes = getTrackRes().getBytes();
				//auto str = getTrackRes().formatSummary();
				//auto str = getTrackRes().formatSummary();
				//std::cout << str;
				//getTrackRes().clearLogs();
				//ImGui::Text("Game Allocations %s", str.c_str());
				//auto nowBytes = getTrackRes().getBytes();
				//ImGui::Text("Allocs per frame %s", str.c_str());
			}

			auto setPiecesPosition = [&] {
				for (auto [trans, mesh, piece, pickup] : manager.view<ark::Transform, MeshComponent, ChessPieceComponent, MousePickUpComponent>()) {
					pickup.selectArea.height = chessSys->kPieceSize;
					pickup.selectArea.width = chessSys->kPieceSize;
					auto scale = chessSys->kPieceSize / mesh.getMeshSize().x;
					trans.setPosition(piece.coord.x * chessSys->kPieceSize, piece.coord.y * chessSys->kPieceSize);
					trans.move(chessSys->kBoardOffset);
					trans.setScale(scale, scale);
				}
			};

			// pieces and board size
			if (ImGui::DragFloat("pieces size in pixels", &chessSys->kPieceSize, 0.1f, 0, 0, "%.1f")) {
				setBoardSize(chessSys->kPieceSize);
				setPiecesPosition();
			}

			// board position
			float vec[2] = { chessSys->kBoardOffset.x, chessSys->kBoardOffset.y };
			if (ImGui::DragFloat2("board-position", vec, 0.2f, 0, 0, "%.1f")) {
				chessSys->kBoardOffset = { vec[0], vec[1] };
				setPiecesPosition();
				auto& trans = boardEntity.get<ark::Transform>();
				trans.setPosition(chessSys->kBoardOffset);
			}

			// drag-and-drop or select-release pieces
			bool drag = false;
			manager.view<MousePickUpComponent>().each([&](auto& pickup) {
				drag = pickup.drag;
				return false;
			});
			if (ImGui::Checkbox("drag-and-drop/select-release", &drag)) {
				for (auto& pickup : manager.view<MousePickUpComponent>()) {
					systems.getSystem<MousePickUpSystem>()->reset();
					pickup.drag = drag;
					pickup.setTransform = drag;
				}
			}

			// rules
			ImGui::Text("Rules to enforce");
			if (ImGui::Checkbox("player turns", &chessSys->rulePlayerTurn)) {
				if (!chessSys->rulePlayerTurn)
					systems.getSystem<MousePickUpSystem>()->setFilter(0);
				else
					systems.getSystem<MousePickUpSystem>()->setFilter(chessSys->playerInTurn.get<ChessPlayerComponent>().id);
			}
			ImGui::Checkbox("enforce legal moves", &chessSys->ruleLegalMoves);
			ImGui::Checkbox("show legal moves", &chessSys->ruleShowMoves);
			ImGui::Checkbox("warn king if attacked", &chessSys->ruleWarnKingOnAttack);
			ImGui::Checkbox("move king if attacked", &chessSys->ruleMoveKingOnAttack);

			// multiplayer stuff
			auto netSystem = systems.getSystem<NetworkSystem>();
			ImGui::InputText("IPv4", netSystem->buffIpAddr, sizeof(netSystem->buffIpAddr));
			if (ImGui::Button("connect", ImVec2(100, 50))) {
				netSystem->sock.connect(netSystem->buffIpAddr, netSystem->port, sf::seconds(1.5));
			}
			sf::IpAddress addr = netSystem->sock.getRemoteAddress();
			auto s = addr.toString();
			if (addr != addr.None)
				ImGui::Text("connected to: %s", s);
			else
				ImGui::Text("not connected");
		});

		Entity playerAlb = manager.createEntity();
		playerAlb.add<ChessPlayerComponent>();

		Entity playerNegru = manager.createEntity();
		playerNegru.add<ChessPlayerComponent>();

		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				if (board[i][j] == 0)
					continue;
				int x = std::abs(board[i][j]) - 1;
				int alb = board[i][j] > 0 ? 1 : 0; // y
				createChessPiece(manager, sf::Vector2i{ j, i }, chessSys, x, alb, playerAlb, playerNegru);
			}
		}
	}
};
/**/

class NetTestState : public BasicState {

public:
	NetTestState(ark::MessageBus& mb) : BasicState(mb) {}

	void init() override {
		systems.addSystem<NetworkSystem>();
		systems.addSystem<FpsCounterDirector>();
		getState<ImGuiLayer>()->addTab("net-conn", [=, this]() mutable {
			auto netSystem = systems.getSystem<NetworkSystem>();
			ImGui::InputText("IPv4", netSystem->buffIpAddr, sizeof(netSystem->buffIpAddr));
			if (ImGui::Button("connect", ImVec2(100, 50))) {
				netSystem->sock.connect(netSystem->buffIpAddr, netSystem->port);
			}
			sf::IpAddress addr = netSystem->sock.getRemoteAddress();
			auto s = addr.toString();
			if (addr != addr.None)
				ImGui::Text("connected to: %s", s);
			else
				ImGui::Text("not connected");
		});
	}
};

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

//void* operator new(std::size_t size) {
//	return getTrackRes().allocate(size);
//}
//
//void operator delete(void* p){
//	// n-avem nevoie de size si align ca avem malloc
//	getTrackRes().deallocate(p, 8);
//}

int main() // are nevoie de c++17 si SFML 2.5.1
{
	GameLog("Static Allocations");
	getTrackRes().printSummary();
	getTrackRes().clearLogs();

	//LVector<VectorLayout::SOA, int, float> vec;
	//vec.push_back(10, 5.5);
	//vec.push_back(11, 9.5);
	//vec.push_back(19, 34.5);
	//for (auto [pp, i] : vec.each()) {
	//	std::cout << pp << ' ' << i << ' ';
	//}

	auto default_memory_res = makePrintingMemoryResource("Rogue PMR Allocation!", std::pmr::null_memory_resource());
	std::pmr::set_default_resource(&default_memory_res);
	any_function fun = any_function::make<int>(test);
	auto res = fun(48);
	std::cout << std::any_cast<int>(res) << '\n';
	{
		auto* type = ark::meta::type<ScriptingComponent>();
		type->func(ark::SceneInspector::serviceName, renderScriptComponents);
		type->func(ark::serde::serviceSerializeName, serializeScriptComponents);
		type->func(ark::serde::serviceDeserializeName, deserializeScriptComponents);
		//type->func("onClone", ScriptingComponent::onClone);
	}

	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	Engine::create(Engine::resolutionFullHD, "Articifii!", sf::seconds(1 / 120.f), settings);
	Engine::backGroundColor = sf::Color(50, 50, 50);
	Engine::getWindow().setVerticalSyncEnabled(false);

	Engine::registerState<TestingState>();
	Engine::registerState<ImGuiLayer>();
	Engine::registerState<ChessState>();
	Engine::registerState<NetTestState>();

	Engine::pushOverlay<ImGuiLayer>();
	//Engine::pushFirstState<ChessState>();
	Engine::pushFirstState<TestingState>();

	GameLog("Main allocs");
	getTrackRes().printSummary();
	getTrackRes().clearLogs();
	Engine::run();

	return 0;
}
