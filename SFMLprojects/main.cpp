#include <SFML/Graphics.hpp>
#include <SFML/System/String.hpp>
#include <functional>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
#include <thread>
#include "Particles.hpp"
#include "Util.hpp"
#include "VectorEngine.hpp"
#include "Ecs.hpp"
#include "ParticleScripts.hpp"
#include "RandomNumbers.hpp"
using namespace std::literals;

/*
void dontShowConsoleOnRelease()
{
	HWND hWnd = GetConsoleWindow();
#ifdef NDEBUG
	ShowWindow(hWnd, SW_HIDE);
#else
	ShowWindow(hWnd, SW_SHOW);
#endif
}
*/

#if 0

enum class Direction {
	Up, Down, Left, Right, Nop
};

struct VectorApp {

	VectorApp(sf::VideoMode vm, std::string title, uint32_t antialiasingLevel = 16)
	{
		sf::ContextSettings settings;
		settings.antialiasingLevel = antialiasingLevel;
		window.create(vm, title, sf::Style::Default, settings);
		window.setVerticalSyncEnabled(true);
	}

	void gameLoop(std::function<void(sf::RenderWindow&, Direction)> draw)
	{	
		while (window.isOpen())
		{
			sf::Event event;
			while (window.pollEvent(event))
			{
				if (event.type == sf::Event::Closed) {
					window.close();
					std::exit(EXIT_SUCCESS);
				}
				if (event.type == sf::Event::KeyPressed)
					if (event.key.code == sf::Keyboard::Escape)
						std::cout<<"ai apasat esc\n";
			}
			window.clear(sf::Color::Black);		

			for (const auto& cs : this->controlScheme)
				if (sf::Keyboard::isKeyPressed(cs.first))
					draw(window, cs.second);

			draw(window, Direction::Nop);
			window.display();
			/*
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::W))
				draw(window, Direction::Up);
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::S))
				draw(window, Direction::Down);
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::A))
				draw(window, Direction::Left);
			if(sf::Keyboard::isKeyPressed(sf::Keyboard::D))
				draw(window, Direction::Right);
			*/

		}
	}
	sf::RenderWindow window;
	std::vector<std::pair<sf::Keyboard::Key, Direction>> controlScheme;
};

sf::VertexArray getQuad()
{
	sf::VertexArray quad(sf::Quads, 4);

	// define it as a rectangle, located at (10, 10) and with size 100x100
	quad[0].position = sf::Vector2f(10.f, 10.f);
	quad[1].position = sf::Vector2f(110.f, 10.f);
	quad[2].position = sf::Vector2f(110.f, 110.f);
	quad[3].position = sf::Vector2f(10.f, 110.f);

	// define its texture area to be a 25x50 rectangle starting at (0, 0)
	quad[0].texCoords = sf::Vector2f(0.f, 0.f);
	quad[1].texCoords = sf::Vector2f(25.f, 0.f);
	quad[2].texCoords = sf::Vector2f(25.f, 50.f);
	quad[3].texCoords = sf::Vector2f(0.f, 50.f);

	return quad;
}

sf::VertexArray getTriangle()
{
	sf::VertexArray triangle(sf::Lines, 3);

	// define the position of the triangle's points
	triangle[0].position = sf::Vector2f(10.f, 10.f);
	triangle[1].position = sf::Vector2f(200.f, 10.f);
	triangle[2].position = sf::Vector2f(200.f, 400.f);
	
	// define the color of the triangle's points
	triangle[0].color = sf::Color::Red;
	triangle[1].color = sf::Color::Blue;
	triangle[2].color = sf::Color::Green;

	return triangle;
}

// ECS: Entities - Components - Systems
int main2()
{		
	VectorApp app(sf::VideoMode(800, 600), "SFML works!");
	app.controlScheme = { 
		{sf::Keyboard::W, Direction::Up },
		{sf::Keyboard::A, Direction::Left },
		{sf::Keyboard::S, Direction::Down },
		{sf::Keyboard::D, Direction::Right }
	};

	auto texture = load<sf::Texture>("imags/toaleta.jpg");
	auto quad = getQuad();
	auto triangle = getTriangle();

	sf::RectangleShape player(sf::Vector2f(100, 100));
	player.setFillColor(sf::Color::Blue);
	sf::RenderTexture tr;

	float speed = 2;
	std::thread readThread([&]() {
		while (true) {
			printf("\ninsert speed\n>>");
			float newSpeed;
			std::cin >> newSpeed;
			speed = newSpeed;
		}
	});

	app.gameLoop([&](sf::RenderWindow& window, Direction dir) {
		switch (dir)
		{
		case Direction::Nop:
			window.draw(player);
			break;
		case Direction::Up:
			player.move(0, -speed);
			break;
		case Direction::Down:
			player.move(0, speed);
			break;
		case Direction::Left:
			player.move(-speed, 0);
			break;
		case Direction::Right:
			player.move(speed, 0);
			break;
		default:
			break;
		}
		//window.draw(triangle);
		//window.draw(quad, &texture);
	});

	return 0;
}
#endif

std::unique_ptr<Entity> makeFireWorksEntity(int count, const Particles& particlesTemplate)
{
	std::vector<Particles> fireWorks(count, particlesTemplate);
	
	for (auto& fw : fireWorks) {
		auto x = RandomNumber<float>(50, 750);
		auto y = RandomNumber<float>(50, 550);
		fw.count = 1000;
		fw.emitter = { x,y };
		fw.fireworks = true;
		fw.lifeTime = sf::seconds(RandomNumber<float>(2, 8));
		fw.speedDistribution.values = { 0, RandomNumber<int>(40, 70) };
		fw.speedDistribution.type = DistributionType::uniform;
	}

	auto fireWorksEntity = std::make_unique<Entity>();
	fireWorksEntity->addComponents(fireWorks);
	return fireWorksEntity;
}

int main() // are nevoie de c++17 si SFML 2.5.1
{
	VectorEngine game(sf::VideoMode(800, 600), "Articifii!");

	Particles fireParticles(1'000, sf::seconds(2), { 2, 100 }, { 0, 2 * PI }, DistributionType::uniform);
	fireParticles.speedDistribution.type = DistributionType::uniform;
	fireParticles.getColor = []() {
		auto green = RandomNumber<uint32_t>(0, 150);
		return sf::Color(255, green, 0);
	};

	Particles whiteParticles(1000, sf::seconds(6), { 0, 5 });
	whiteParticles.speedDistribution.type = DistributionType::uniform;
	whiteParticles.angleDistribution.type = DistributionType::uniform;

	Particles rainbowParticles(2000, sf::seconds(3), { 0, 100 }, {0,2*PI}, DistributionType::uniform);
	rainbowParticles.getColor = []() {
		return sf::Color(RandomNumber<uint32_t>(0x000000ff, 0xffffffff));
	};

	Entity letterM{ true }, letterU{ true }, letterI{ true }, letterE{ true };

	letterM.addComponent<Particles>(whiteParticles);
	letterU.addComponent<Particles>(whiteParticles);
	letterI.addComponent<Particles>(whiteParticles);
	letterE.addComponent<Particles>(whiteParticles);

	using namespace ParticlesScripts;
	letterM.addScript<PlayModel>("./res/litere/letterM.txt");
	letterU.addScript<PlayModel>("./res/litere/letterU.txt");
	letterI.addScript<PlayModel>("./res/litere/letterI.txt");
	letterE.addScript<PlayModel>("./res/litere/letterE.txt");

	Entity rainbow{true};
	rainbow.addComponent<Particles>(rainbowParticles);
	rainbow.addScript<SpawnOnRightClick>();
	rainbow.addScript<EmittFromMouse>();

	Entity fire{ true };
	fire.addComponent<Particles>(fireParticles);
	fire.addScript<SpawnOnLeftClick>();
	fire.addScript<EmittFromMouse>();
	
	// TODO: de parametrizat sistemele

	Entity trail{true};
	trail.addComponent<Particles>(whiteParticles);
	trail.addScript<DeSpawnOnMouseClick<TraillingEffect>>();
	trail.addScript<TraillingEffect>();
	trail.addScript<EmittFromMouse>();

	auto fireWorksEntity = makeFireWorksEntity(30, rainbowParticles);
	fireWorksEntity->addScript<SpawnLater>(5);
	fireWorksEntity->Register();

	ParticleSystem::gravityVector = { 0, 0 };

	//std::thread modifyVarsThread([&]() {
	//	auto&[x, y] = ParticleSystem::gravityVector;
	//	while (true) {
	//		std::cout << "enter gravity x and y: ";
	//		std::cin >> x >> y;
	//		std::cout << std::endl;
	//		//fireWorksEntity->addScript<SpawnParticlesLater>(5);
	//	}
	//});
	//modifyVarsThread.detach();

	game.run();
	
	return 0;
}

int mainRandom()
{
	std::map<int, int> nums;
	auto args = std::pair(0.f, 2*PI);
	int n = 100'000;
	for (int i = 0; i < n; i++) {
		auto num = RandomNumber(args, DistributionType::normal);
		nums[std::round(num)]++;
	}
	for (auto [num, count] : nums) {
		std::cout << num << ':' << count << '\n';
	}
	return 0;
}