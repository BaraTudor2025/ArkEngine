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

	void init() {
		animation = getComponent<Animation>();

		transform = getComponent<Transform>();
		transform->setOrigin(animation->frameSize() / 2.f);
		transform->move(VectorEngine::center());
		transform->scale(0.15, 0.15);

		runningParticles = getComponent<PixelParticles>();
		auto pp = runningParticles;
		pp->particlesPerSecond = pp->count;
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
		} else {
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

// ? 
struct TextBox : Data<TextBox> {

};

struct Text : Data<Text>, sf::Text { 

	Text(std::string fontName = "KeepCalm-Medium.ttf") : fontName(fontName) { }

	bool moveWithMouse = false;

private:
	std::string fontName;
	friend class GuiSystem;
};

struct Button : Data<Button>, sf::RectangleShape {

	Button(sf::FloatRect rect, std::string texture = "")
		: textureName(texture), rect(rect)
	{
		this->setSize({ this->rect.width, this->rect.height });
		this->move(this->rect.left, this->rect.top);
	}

	std::function<void()> onClick;
	void savePosition(std::string file) { 
		std::ofstream fout("./res/gui_data/" + file);
		std::cout << "mama\n";
		auto[x, y] = this->getPosition();
		fout << x << ' ' << y;
	}
	void loadPosition(std::string file) {
		std::ifstream fin("./res/gui_data/" + file);
		float x, y;
		fin >> x >> y;
		this->setPosition(x, y);
	}
	bool moveWithMouse = false;

private:
	sf::FloatRect rect;
	std::string textureName;
	friend class GuiSystem;
};

class GuiSystem : public System {

	void init() {
		initFrom<Button>();
		initFrom<Text>();
	}

	void add(Component* c) {
		if (auto b = dynamic_cast<Button*>(c); b) {
			if (!b->textureName.empty()) {
				b->setTexture(load<sf::Texture>(b->textureName));
			}
		}
		if (auto t = dynamic_cast<Text*>(c); t) {
			t->setFont(*load<sf::Font>(t->fontName));
		}
	}

	void update() {
		auto process = [&](auto& components) {
			for (auto& c : components) {
				if (c->moveWithMouse && isLeftMouseButtonPressed) {
					auto mouse = VectorEngine::mousePositon();
					if (c->getGlobalBounds().contains(mouse)) {
						c->setPosition(mouse);
					}
				}
			}
		};
		process(this->getComponents<Button>());
		process(this->getComponents<Text>());
	}

	void handleEvent(sf::Event event) {
		switch (event.type)
		{
		case sf::Event::MouseButtonPressed: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = true;
				for (auto& b : this->getComponents<Button>())
					if(b->getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y))
						b->onClick(); 
			}
			if (event.mouseButton.button == sf::Mouse::Right) {
				isRightMouseButtonPressed = true;
			}
		} break;
		case sf::Event::MouseButtonReleased: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = false;
			}
			if (event.mouseButton.button == sf::Mouse::Right) {
				isRightMouseButtonPressed = false;
			}

		} break;
		default:
			break;
		}
	}

	void render(sf::RenderTarget& target) {
		for (auto& b : this->getComponents<Button>())
			target.draw(*b);
		for (auto& t : this->getComponents<Text>()) {
			target.draw(*t);
		}
	}

private:
	bool isLeftMouseButtonPressed = false;
	bool isRightMouseButtonPressed = false;
	sf::Vector2f prevMousePos{ 0,0 };
};

template<typename T>
class SaveGuiElementPosition : public Script {

public:
	SaveGuiElementPosition(std::string file, sf::Keyboard::Key key): file(file), key(key) { }

private:
	void init() {
		component = getComponent<T>();
	}
	void handleEvent(sf::Event event)
	{
		switch (event.type) {
		case sf::Event::KeyPressed:
			if (event.key.code == key)
				component->savePosition(file);
		}
	}

private:
	std::string file;
	sf::Keyboard::Key key;
	T* component;
};

int main() // are nevoie de c++17 si SFML 2.5.1
{
	sf::ContextSettings settings = sf::ContextSettings();
	settings.antialiasingLevel = 16;

	VectorEngine::create(VectorEngine::resolutionFourByThree, "Articifii!", sf::seconds(1/60.f), settings);
	VectorEngine::setVSync(false);
	VectorEngine::backGroundColor = sf::Color(50, 50, 50);

	VectorEngine::addSystem(new FpsCounterSystem(sf::Color::White));
	VectorEngine::addSystem(new ParticleSystem());
	VectorEngine::addSystem(new GuiSystem());
	VectorEngine::addSystem(new AnimationSystem());
	//VectorEngine::addSystem(new DebugEntitySystem());
	//VectorEngine::addSystem(new DebugParticleSystem());

	Entity player;
	player.addComponent<Transform>();
	player.addComponent<Animation>("chestie.png", sf::Vector2u{6, 2}, sf::milliseconds(100), 1, false);
	player.addComponent<PixelParticles>(2'000, sf::seconds(7), sf::Vector2f{ 5, 5 }, std::pair{ sf::Color::Yellow, sf::Color::Red });
	player.addScript<MovePlayer>(400, 180);
	player.Register();

	Entity image;
	image.addComponent<Transform>()->setPosition(VectorEngine::center());
	image.addComponent<Mesh>("toaleta.jpg", false);
	//image.Register();

	Entity button;
	//button.addScript<SaveGuiElementPosition<Button>>("mama"s, sf::Keyboard::S);
	auto b = button.addComponent<Button>(sf::FloatRect{100, 100, 200, 100});
	b->setFillColor(sf::Color(240, 240, 240));
	b->setOutlineColor(sf::Color::Black);
	b->setOutlineThickness(3);
	b->moveWithMouse = true;
	b->loadPosition("mama");
	b->setOrigin(b->getSize() / 2.f);
	int i = 1;
	b->onClick = [&]() { std::cout << "\nclick " << i++; };
	auto t = button.addComponent<Text>();
	t->setOrigin(b->getOrigin());
	t->setPosition(b->getPosition() + b->getSize() / 3.5f);
	t->setString("ma-ta");
	t->setFillColor(sf::Color::Black);
	button.Register();

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
	trail.addComponent<PointParticles>(2000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal);
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

	auto fwEntities = makeFireWorksEntities(2000, fireParticles, false);
	for (auto& e : fwEntities) {
		e.setAction(Action::SpawnLater, 10);
		e.Register();
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
