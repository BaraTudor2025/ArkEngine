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
#include "Particles.hpp"
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
#include "VectorEngine.hpp"
#include "Util.hpp"
#include "Entities.hpp"
#include "Scripts.hpp"
#include "ResourceManager.hpp"

using namespace std::literals;

#if 1

// Animation applies textures on a shape component
// Entity needs shape component
struct Animation : public Data<Animation> {

public:
	Animation(std::string fileName, sf::Vector2u frameCount, sf::Time frameTime, int state)
	: fileName(fileName), frameCount(frameCount), frameTime(frameTime), row(state)
	{ }

	int row;
	sf::Time frameTime;
	bool flipped = false;

private:
	std::string fileName;
	sf::Vector2u currentFrame;
	const sf::Vector2u frameCount;
	sf::Time passedTime;
	sf::IntRect uvRect;
	sf::Texture* pTexture;
	//sf::VertexArray vertices;

	template<typename>
	friend class AnimationSystem;

public:
	template<std::size_t N> decltype(auto) get() {
		if constexpr (N == 0) return this->row;
		else if constexpr (N == 1) return this->frameCount;
		else if constexpr (N == 2) return this->frameTime;
		else if constexpr (N == 3) return (this->currentFrame);
		else if constexpr (N == 4) return (this->passedTime);
		else if constexpr (N == 5) return (this->uvRect);
	}
};

namespace std{

	template<> struct tuple_size<Animation> : std::integral_constant<std::size_t, 6> {};

	template<std::size_t N> 
	struct tuple_element<N, Animation> {
		using type = decltype(std::declval<Animation>().get<N>());
	};
}

template <typename Animated>
class AnimationSystem : public System {

public:
	void init() override {
		initFrom<Animation>();
	}

	void update() override
	{
		for (auto animation : this->getComponents<Animation>()) {
			if (!animation->entity()->getComponent<Animated>())
				continue;

			auto&[row, frameCount, frameTime, currentFrame, passedTime, uvRect] = *animation;

			currentFrame.y = row;
			passedTime += VectorEngine::deltaTime();
			if (passedTime >= frameTime) {
				passedTime -= frameTime;
				currentFrame.x += 1;
				if (currentFrame.x >= frameCount.x)
					currentFrame.x = 0;
			}
			uvRect.top = currentFrame.y * uvRect.height;
			uvRect.left = currentFrame.x * uvRect.width;
		}
	}

	void add(Component* c) override 
	{
		if (auto a = static_cast<Animation*>(c); a) {
			if (!a->entity()->getComponent<Animated>())
				return;
			a->pTexture = load<sf::Texture>(a->fileName);
			a->pTexture->setSmooth(true);
			a->uvRect.width = a->pTexture->getSize().x / (float)a->frameCount.x;
			a->uvRect.height = a->pTexture->getSize().y / (float)a->frameCount.y;
			a->passedTime = sf::seconds(0);
			a->currentFrame.x = 0;
			a->entity()->getComponent<Animated>()->setTexture(a->pTexture);
		}
	}

	virtual void render(sf::RenderTarget& target) override
	{
		for (auto animation : this->getComponents<Animation>()) {
			auto animated = animation->entity()->getComponent<Animated>();
			if (!animated)
				continue;
			animated->setTextureRect(animation->uvRect);
			//sf::Shape p;
			//sf::RenderStates rs;
			//rs.texture = animation->pTexture;
			target.draw(*animated);
		}
	}

	virtual void remove(Component*) override { }

};

#endif

#if 0
struct RigidBody : public Data<RigidBody> {

};

class ColisionSystem : public System {

};

struct Mesh : public Data<Mesh> {
	std::string fileName;
};

class MeshSystem : public System {

public:
	virtual void update() override
	{
	}
	virtual void render(sf::RenderTarget& target) override
	{
	}
	virtual void add(Component *) override
	{
	}
	virtual void remove(Component *) override
	{
	}

private:
	std::vector<sf::Texture*> textures;
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
	RectangleShape* body;
	float speed;
public:

	MovePlayer(float speed) : speed(speed) { }
	void init()
	{
		animation = getComponent<Animation>();
		body = getComponent<RectangleShape>();
	}
	void update() {
		bool moved = false;
		auto dt = VectorEngine::deltaTime().asSeconds();
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			body->move(speed * dt, 0);
			moved = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			body->move(-speed * dt, 0);
			moved = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
			body->move(0, -speed * dt);
			moved = true;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
			body->move(0, speed * dt);
			moved = true;
		}
		if (moved)
			animation->row = 1;
		else
			animation->row = 0;
	}

};

int main() // are nevoie de c++17 si SFML 2.5.1
{
	auto fullHD = sf::VideoMode(1920, 1080);
	auto normalHD = sf::VideoMode(1280, 720);
	auto fourByThree = sf::VideoMode(1024, 768);

	sf::ContextSettings settings;
	settings.antialiasingLevel = 10;
	VectorEngine::create(fourByThree, "Articifii!", settings);
	VectorEngine::backGroundColor = sf::Color(150, 150, 150);
	VectorEngine::addSystem(new AnimationSystem<RectangleShape>());
	//VectorEngine::addSystem(&ParticleSystem::instance);
	
	Entity player;
	auto shape = player.addComponent<RectangleShape>();
	shape->setSize(sf::Vector2f{ 100, 150 });
	shape->setPosition(200, 200);
	player.addComponent<Animation>("tux_from_linux.png", sf::Vector2u{3, 9}, sf::milliseconds(200), 0);
	player.addScript<MovePlayer>(100);
	player.Register();

	using namespace ParticleScripts;

	std::string prop("");
	//std::getline(std::cin, prop);
	std::vector<Entity> letters = makeLetterEntities(prop);
	registerEntities(letters);

	auto rainbowParticles = getRainbowParticles();
	auto fireParticles = getFireParticles();
	auto greenParticles = getGreenParticles();

	Entity rainbow{ false };
	rainbow.addComponent<Particles>(rainbowParticles);
	rainbow.addScript<SpawnOnRightClick>();
	rainbow.addScript<EmittFromMouse>();

	Entity fire{ false };
	fire.addComponent<Particles>(fireParticles);
	fire.addScript<SpawnOnLeftClick>();
	fire.addScript<EmittFromMouse>();

	Entity grass{ false };
	greenParticles.spawn = true;
	grass.addComponent<Particles>(greenParticles);
	grass.addScript<DeSpawnOnMouseClick<>>();
	//grass.addScript<ReadColorFromConsole>();
	grass.addScript<EmittFromMouse>();

	Entity trail{ false };
	trail.addComponent<Particles>(1000, sf::seconds(5), Distribution{ 0.f, 2.f }, Distribution{ 0.f,0.f }, DistributionType::normal );
	trail.addScript<EmittFromMouse>();
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();

	Entity plimbarica;
	//plimbarica.addComponent<Transform>();
	plimbarica.addComponent<Particles>(rainbowParticles);
	//plimbarica.addScript<RotateParticles>(4 * 360);
	//plimbarica.addScript<TraillingEffect>();
	//plimbarica.addScript<Rotate>(360/2, VectorEngine::center());
	plimbarica.getComponent<Particles>()->spawn = true;
	//pp->spawn = true;
	//pp->emitter = VectorEngine::center();

	//plimbarica.Register();
	//grass.Register();
	//fire.Register();
	//trail.Register();
	//rainbow.Register();

	auto fwEntities = makeFireWorksEntities(10, rainbowParticles);
	for (auto& e : fwEntities) {
		e.addScript<SpawnLater>(5);
		//e.Register();
	}

	auto randomParticles = makeRandomParticlesFountains(50, 1.f, rainbowParticles);
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

	//readGMag.Register();
	//updateGP.Register();

	ParticleSystem::hasUniversalGravity = true;
	ParticleSystem::gravityVector = { 0, 0 };
	ParticleSystem::gravityMagnitude = 100;
	ParticleSystem::gravityPoint = VectorEngine::center();

	bool reg = true;

	VectorEngine::run();
	
	return 0;
}
