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

//class MovePlayer : public ark::ScriptT<MovePlayer> {
class MovePlayer : public ScriptT<MovePlayer, true> {
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

	MovePlayer() = default;
	MovePlayer(float speed, float rotationSpeed) : speed(speed), rotationSpeed(rotationSpeed) {}

	void setScale(sf::Vector2f scale)
	{
		transform->setScale(scale);
		resetOffset();
		particleEmitterOffsetLeft = particleEmitterOffsetLeft * transform->getScale();
		particleEmitterOffsetRight = particleEmitterOffsetRight * transform->getScale();
		runningParticles->size = { 60, 30 };
		runningParticles->size = runningParticles->size * transform->getScale();
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

ARK_REGISTER_TYPE(MovePlayer, "MovePlayerScript", ARK_DEFAULT_SERVICES)
{
	return ark::meta::members(
		member_property("speed", &MovePlayer::speed),
		member_property("scale", &MovePlayer::getScale, &MovePlayer::setScale)
	);
}

ARK_REGISTER_MEMBERS(ParticleScripts::RotateEmitter)
{
	using namespace ParticleScripts;
	return members(
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

class TestMessageSystem : public ark::System {
public:
	TestMessageSystem() : ark::System(typeid(TestMessageSystem)) {}

	void handleMessage(const ark::Message& message) override
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
	TestingState,
	ImGuiLayer
};

class BasicState : public ark::State {

protected:
	ark::Registry registry;
	sf::Sprite screen;
	sf::Texture screenImage;
	bool takenSS = false;
	bool pauseScene = false;

public:
	BasicState(ark::MessageBus& bus, std::pmr::memory_resource* res = std::pmr::new_delete_resource()) 
		: ark::State(bus), registry(bus, res) {}

	void handleEvent(const sf::Event& event) override
	{
		if (!pauseScene)
			registry.handleEvent(event);

		if (event.type == sf::Event::KeyPressed)
			if (event.key.code == sf::Keyboard::F1) {
				pauseScene = !pauseScene;
				takenSS = false;
			}
	}

	void handleMessage(const ark::Message& message) override
	{
		if (!pauseScene)
			registry.handleMessage(message);
	}

	void update() override
	{
		if (!pauseScene) {
			registry.update();
		}
		else
			registry.processPendingData();
	}

	void render(sf::RenderTarget& target) override
	{
		if (!pauseScene) {
			registry.render(target);
		}
		else if (!takenSS) {
			registry.render(target);
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
struct DelayedAction : ark::Component<DelayedAction> {

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
ARK_REGISTER_TYPE(DelayedAction, "DelayedAction", ARK_DEFAULT_SERVICES) { return members(); }

class DelayedActionSystem : public ark::SystemT<DelayedActionSystem> {
public:
	void init() override
	{
		requireComponent<DelayedAction>();
	}

	void update() override
	{
		for (auto entity : getEntities()) {
			auto& da = entity.getComponent<DelayedAction>();
			da.time -= Engine::deltaTime();
			if (da.time <= sf::Time::Zero) {
				if (da.action) {
					auto action = std::move(da.action);
					registry()->safeRemoveComponent(entity, typeid(DelayedAction));
					//entity.removeComponent<DelayedAction>();
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
	ark::SerdeJsonDirector* serde;
public:
	SaveEntityScript() = default;

	char key = 'k';

	void bind() noexcept override {
		serde = registry.getDirector<ark::SerdeJsonDirector>();
	}

	void handleEvent(const sf::Event& ev) noexcept override
	{
		if (ev.type == sf::Event::KeyPressed) {
			if (ev.key.code == std::tolower(key) - 'a'){
				auto e = entity();
				serde->serializeEntity(e);
				GameLog("just serialized entity %s", e.getComponent<TagComponent>().name);
			}
		}
	}
};
ARK_REGISTER_MEMBERS(SaveEntityScript) { return members(member_property("key", &SaveEntityScript::key)); }

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
		Entity e = registry.createEntity();
		e.addComponent<ark::TagComponent>(name);
		e.addComponent<ark::Transform>();
		return e;
	}

	void init() override
	{
		registry.addSystem<PointParticleSystem>();
		registry.addSystem<PixelParticleSystem>();
		registry.addSystem<ButtonSystem>();
		registry.addSystem<TextSystem>();
		registry.addSystem<AnimationSystem>();
		registry.addSystem<DelayedActionSystem>();

		// TODO
		//registry.addDefaultComponent<ark::TagComponent>();
		//registry.addDefaultComponent<ark::Transform>();
		//registry.addDefaultComponent(typeid(ark::Transform));
		//registry.addDefaultComponent(typeid(ark::TagComponent));

		// setup for tags
		registry.onConstruction<TagComponent>([&](ark::Entity e, TagComponent& tag) {
			tag._setEntity(e);
			if (tag.name.empty())
				tag.name = std::string("entity_") + std::to_string(e.getID());
		});

		// setup for scripts
		registry.addSystem<ScriptingSystem>();
		registry.onConstruction<ScriptingComponent>([registry = &this->registry](ark::Entity e, ScriptingComponent* comp) {
			comp->_setScene(registry);
			comp->_setEntity(e);
		});

		// setup for lua scripts
		auto pLuaScriptingSystem = registry.addSystem<LuaScriptingSystem>();
		registry.onConstruction<LuaScriptingComponent>([pLuaScriptingSystem](ark::Entity entity, LuaScriptingComponent& comp) {
			comp._setEntity(entity);
			comp._setSystem(pLuaScriptingSystem);
		});

		auto* inspector = registry.addDirector<SceneInspector>();
		auto* serde = registry.addDirector<SerdeJsonDirector>();
		registry.addDirector<FpsCounterDirector>();
		ImGuiLayer::addTab({ "registry inspector", [=]() { inspector->renderSystemInspector(); } });
		//registry.addSystem<TestMessageSystem>();

		button = makeEntity("button");
		mouseTrail = makeEntity("mouse_trail");
		rainbowPointParticles = makeEntity("rainbow_ps");
		greenPointParticles = makeEntity("green_ps");
		firePointParticles = makeEntity("fire_ps");
		rotatingParticles = makeEntity("rotating_ps");

		player = makeEntity("player");
		serde->deserializeEntity(player);
		
		//player.addComponent<Animation>("chestie.png", std::initializer_list<uint32_t>{2, 6}, sf::milliseconds(100), 1, false);
		/*player = registry.createEntity("player2");
		auto& transform = player.addComponent<Transform>();
		auto& animation = player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
		auto& pp = player.addComponent<PixelParticles>(100, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
		auto& playerScripting = player.addComponent<ScriptingComponent>(player);
		auto moveScript = playerScripting.addScript<MovePlayer>(400, 180);

		transform.move(Engine::center());
		transform.setOrigin(animation.frameSize() / 2.f);

		pp.particlesPerSecond = pp.getParticleNumber() / 2;
		pp.size = { 60, 30 };
		pp.speed = moveScript->speed;
		pp.emitter = transform.getPosition() + sf::Vector2f{ 50, 40 };
		pp.gravity = { 0, moveScript->speed };
		auto[w, h] = Engine::windowSize();
		pp.platform = { Engine::center() + sf::Vector2f{w / -2.f, 50}, {w * 1.f, 10} };

		std::vector<ComponentManager::ComponentMask> que
		moveScript->setScale({0.1, 0.1});

		player.addComponent<DelayedAction>(sf::seconds(4), [this, serde](Entity e) {
			GameLog("just serialized entity %s", e.getName());
			serde->serializeEntity(e);
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
		ark::Entity rainbowClone = registry.cloneEntity(rainbowPointParticles);
		//rainbowClone.getComponent<TagComponent>().name = "rainbow_clone";
		{
			auto& scripts = rainbowClone.addComponent<ScriptingComponent>();
			scripts.addScript<SpawnOnRightClick>();
			scripts.removeScript(typeid(SpawnOnRightClick));
			scripts.addScript<SpawnOnLeftClick>()->setActive(false);
			scripts.addScript<EmittFromMouse>();
		}
		rainbowClone.addComponent<DelayedAction>(sf::seconds(5), [](ark::Entity e) {
			e.getComponent<ScriptingComponent>().getScript<SpawnOnLeftClick>()->setActive(true);
		});
#endif

		firePointParticles.addComponent<PointParticles>(getFireParticles(1'000));
		{
			auto& scripts = firePointParticles.addComponent<ScriptingComponent>();
			scripts.addScript<SpawnOnLeftClick>();
			scripts.addScript<EmittFromMouse>();
		}
		firePointParticles.addComponent<DelayedAction>(sf::seconds(5), [this](ark::Entity e) {
			registry.destroyEntity(e);
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

// basically a ring-buffer, allocate TEMPORARY objects(no more than a few frames)
// when the it reaches the end of the buffer then loop from the beginning,
// if objects are still alive on loop they will get rewriten, allocate bigger buffer next time :P
class TemporaryResource : public std::pmr::memory_resource {
	std::unique_ptr<std::byte[]> memory;
	size_t size;
	void* ptr;
	size_t remainingSize;
	struct {
		void* ptr;
		size_t bytes;
	} lastDeallocation;
public:
	TemporaryResource(size_t size) 
		: memory(new std::byte[size]), ptr(memory.get())
		, size(size), remainingSize(size) { }

	void* do_allocate(const size_t bytes, const size_t align) override {
		if (std::align(align, bytes, ptr, remainingSize)) {
			this->remainingSize -= bytes;
			void* mem = ptr;
			ptr = (std::byte*)ptr + bytes;
			return mem;
		}
		else {
			if (remainingSize == size)
				return nullptr;
			ptr = memory.get();
			remainingSize = size;
			return do_allocate(bytes, align);
		}
	}

	// lol
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		lastDeallocation = { p, bytes };
	}

	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

class SegregatorResource : public std::pmr::memory_resource {
	std::pmr::memory_resource* small;
	std::pmr::memory_resource* large;
	int threshold;
public:
	SegregatorResource(int threshold, std::pmr::memory_resource* small, std::pmr::memory_resource* large)
		: small(small), large(large), threshold(threshold) { }

	void* do_allocate(const size_t bytes, const size_t align) override {
		if (bytes <= threshold)
			return small->allocate(bytes, align);
		else
			return large->allocate(bytes, align);
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		if (bytes <= threshold)
			small->deallocate(p, bytes, align);
		else
			large->deallocate(p, bytes, align);
	}
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

class AffixAllocator : public std::pmr::memory_resource {
	std::pmr::memory_resource* upstream;

public:
	struct Callback {
		// pointer, size, align allocate=true, dealloc=false
		std::function<void(size_t, size_t, bool)> prefix;
		std::function<void(void*, size_t, size_t, bool)> postfix;
	}cbs;

	AffixAllocator(std::pmr::memory_resource* up, Callback cb)
		: upstream(up), cbs(cb) {}

	void* do_allocate(const size_t bytes, const size_t align) override {
		if(cbs.prefix)
			cbs.prefix(bytes, align, true);
		void* p = upstream->allocate(bytes, align);
		if(cbs.postfix)
			cbs.postfix(p, bytes, align, true);
		return p;
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		if(cbs.prefix)
			cbs.prefix(bytes, align, false);
		upstream->deallocate(p, bytes, align);
		if(cbs.postfix)
			cbs.postfix(nullptr, bytes, align, false);
	}
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

class TrackingResource : public std::pmr::memory_resource {

};

class FallBackResource : public std::pmr::memory_resource {
	std::pmr::memory_resource* primary;
	std::pmr::memory_resource* secondary;
	std::deque<void*> secondaryPtrs;

public:
	FallBackResource(std::pmr::memory_resource* p, std::pmr::memory_resource* s)
		: primary(p), secondary(s) { }
	
	void* do_allocate(const size_t bytes, const size_t align) override {
		void* p = primary->allocate(bytes, align);
		if (!p) {
			p = secondary->allocate(bytes, align);
			secondaryPtrs.push_back(p);
		}
		return p;
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		if (secondaryPtrs.empty() || secondaryPtrs.end() == std::find(secondaryPtrs.begin(), secondaryPtrs.end(), p)) {
			primary->deallocate(p, bytes, align);
			Util::erase(secondaryPtrs, p);
		}
		else
			secondary->deallocate(p, bytes, align);
	}
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

auto makePrintingMemoryResource(std::string name, std::pmr::memory_resource* upstream) -> AffixAllocator 
{
	auto printing_prefix = [name](size_t bytes, size_t, bool alloc) {
		if (alloc)
			std::printf("alloc(%s) : %zu bytes\n", name.c_str(), bytes);
		else
			std::printf("dealloc(%s) : %zu bytes\n", name.c_str(), bytes);
	};
	return AffixAllocator(upstream, {printing_prefix});
}

template <typename T>
void destroyObjectFromResource(std::pmr::memory_resource* res, T* ptr)  {
	std::destroy_at(ptr);
	res->deallocate(ptr, sizeof(T), alignof(T));
}

template <typename T, typename...Args>
auto makeObjectFromResource(std::pmr::memory_resource* res, Args&&... args) -> T* {
	void* ptr = res->allocate(sizeof(T), alignof(T));
	return std::construct_at<T>(ptr, std::forward<Args>(args)...);
}


struct PmrResourceDeleter {
	std::pmr::memory_resource* res;

	template <typename T>
	void operator()(T* ptr) {
		destroyObjectFromResource(res, ptr);
	}
};

template <typename T, typename...Args>
auto makeUniqueFromResource(std::pmr::memory_resource* res, Args&&...args) -> std::unique_ptr<T, PmrResourceDeleter>
{
	void* vptr = res->allocate(sizeof(T), alignof(T));
	T* ptr = new (vptr) T(std::forward<Args>(args)...);
	return std::unique_ptr<T, PmrResourceDeleter>(ptr, { res });
}

// delay insertion so it can be done at once
// dar mai bine dau un 'reserve' SI GATA
template <typename V>
struct VectorBuilderWithResource {

	std::vector<std::function<void()>> builder;
	std::pmr::memory_resource* res;
	V& toBuild;

	VectorBuilderWithResource(V& toBuild, std::pmr::memory_resource* res) : toBuild(toBuild), res(res) {}
	VectorBuilderWithResource(V& toBuild) : toBuild(toBuild), res(toBuild.get_allocator().resource()) {}

	template <typename T, typename...Args>
	void add(Args&&... args) {
		builder.emplace_back([this, &args...]() {
			void* vptr = res->allocate(sizeof(T), alignof(T));
			//T* ptr = new (vptr) T(std::forward<Args>(args)...);
			T* ptr = new (vptr) T(std::move(args)...);
			toBuild.emplace_back(ptr);
		});
	}

	template <typename T, typename...Args>
	void addUnique(Args&&... args) {
		builder.emplace_back([this, &args...]() {
			toBuild.emplace_back(makeUniqueFromResource<T>(res, std::move(args)...));
			//toBuild.emplace_back(makeUniqueFromResource<T>(res, std::forward<Args>(args)...));
		});
	}

	void build() {
		toBuild.reserve(builder.size());
		for (auto& f : builder) {
			f();
		}
	}
};

// needs to be pmr
template <typename T>
struct ContainerResourceDeleterGuard {
	T& vec;
	ContainerResourceDeleterGuard(T& vec) : vec(vec) {}

	~ContainerResourceDeleterGuard() {
		for (auto* elem : vec)
			destroyObjectFromResource(vec.get_allocator().resource(), elem);
	}
};

constexpr std::size_t operator"" _KiB(unsigned long long value) noexcept
{
	return std::size_t(value * 1024);
}

constexpr std::size_t operator"" _KB(unsigned long long value) noexcept
{
	return std::size_t(value * 1000);
}

constexpr std::size_t operator"" _MiB(unsigned long long value) noexcept
{
	return std::size_t(value * 1024 * 1024);
}

constexpr std::size_t operator"" _MB(unsigned long long value) noexcept
{
	return std::size_t(value * 1000 * 1000);
}

// doesn't call destructor
template <typename T>
class WinkOut {
	std::aligned_storage_t<sizeof(T), alignof(T)> storage;
	T* _ptr() { return reinterpret_cast<T*>(&storage); }

public:
	template <typename... Args>
	WinkOut(Args&&... args) {
		new (&storage) T(std::forward<Args>(args)...);
	}

	T* data() { return _ptr(); }

	T& operator*() {
		return *_ptr();
	}

	T* operator->() {
		return _ptr();
	}

	void destruct() {
		_ptr()->~T();
	}
};

template <typename T>
concept CComponent = std::default_initializable<T> && std::copy_constructible<T> && std::move_constructible<T>;

int main() // are nevoie de c++17 si SFML 2.5.1
{
	auto default_memory_res = makePrintingMemoryResource("Rogue PMR Allocation!", std::pmr::null_memory_resource());
	std::pmr::set_default_resource(&default_memory_res);

	auto buff_size = 1000;
	auto buffer = std::make_unique<std::byte[]>(buff_size);
	auto track_new_del = makePrintingMemoryResource("new-del", std::pmr::new_delete_resource());
	auto monotonic_res = std::pmr::monotonic_buffer_resource(buffer.get(), buff_size, &track_new_del);
	auto track_monoton = makePrintingMemoryResource("monoton", &monotonic_res);
	//auto uptr = makeUniqueFromResource<int>(&monotonic_res);
	//auto pool = std::pmr::unsynchronized_pool_resource(&monotonic_res);

	std::cout << "vector:\n";
	//auto vec = std::pmr::vector<std::unique_ptr<bool, PmrResourceDeleter>>(&track_monoton);
	auto track_monoton2 = makePrintingMemoryResource("vec", &monotonic_res);
	auto vec = std::pmr::vector<bool*>(&track_monoton2);
	auto _deleter = ContainerResourceDeleterGuard(vec);
	auto builder = VectorBuilderWithResource(vec);
	//builder.addUnique<bool>(true);
	//builder.addUnique<bool>(true);
	//builder.addUnique<bool>(true);
	//builder.addUnique<bool>(true);
	builder.add<bool>(true);
	builder.add<bool>(true);
	builder.add<bool>(true);
	builder.add<bool>(true);
	std::cout << "build\n";
	builder.build();
	std::cout << "map:\n";

	auto map = std::pmr::map<std::pmr::string, int>(&track_monoton);
	map.emplace("nush ceva string mai lung", 3);
	map.emplace("nush ceva string mai lung1", 3);
	map.emplace("nush ceva string mai lung2", 3);
	map.emplace("nush ceva string mai lung3", 3);
	map.emplace("nush ceva string mai lung4", 3);
	map.emplace("nush ceva string mai lung5", 3);
	map.emplace("nush ceva string mai lung6", 3);
	//map["nush ceva string mai lung"] = 3;


	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	Engine::create(Engine::resolutionFullHD, "Articifii!", sf::seconds(1 / 120.f), settings);
	Engine::backGroundColor = sf::Color(50, 50, 50);
	Engine::getWindow().setVerticalSyncEnabled(false);
	Engine::registerState<class TestingState>(States::TestingState);
	Engine::registerState<class ImGuiLayer>(States::ImGuiLayer);
	Engine::pushFirstState(States::TestingState);
	Engine::pushOverlay(States::ImGuiLayer);

	Engine::run();

	return 0;
}
