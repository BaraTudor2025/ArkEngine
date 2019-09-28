#pragma once

#include "Core.hpp"
#include "Scene.hpp"

#include <SFML/Graphics.hpp>

class ARK_ENGINE_API ArkEngine final : public NonCopyable, public NonMovable{
public:

	static inline const sf::VideoMode resolutionFullHD{1920, 1080};
	static inline const sf::VideoMode resolutionNormalHD{1280, 720};
	static inline const sf::VideoMode resolutionFourByThree{1024, 768};

	static void create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime ,sf::ContextSettings = sf::ContextSettings());

	static sf::Vector2u windowSize() { return { width, height }; }

	static void run();

	template <typename T>
	static void setScene() { currentScene = std::make_unique<T>(); currentScene->init(); }

	static sf::Vector2f mousePositon() { return window.mapPixelToCoords(sf::Mouse::getPosition(window)); }

	static sf::Time deltaTime() { return delta_time + clock.getElapsedTime(); }
	static sf::Time fixedTime() { return fixed_time; }

	static sf::Vector2f center() { return static_cast<sf::Vector2f>(ArkEngine::windowSize()) / 2.f; }

	static inline sf::Color backGroundColor;

	static sf::RenderWindow& getWindow() { return window; }

private:
	static void initScene();

	static inline sf::RenderWindow window;
	static inline sf::View view;
	static inline sf::Time delta_time;
	static inline sf::Time fixed_time;
	static inline sf::Clock clock;
	static inline uint32_t width, height;
	// TODO Engine: add StateStack
	static inline std::unique_ptr<Scene> currentScene;

};

void ArkEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
{
	fixed_time = fixedUpdateTime;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
}

void ArkEngine::run()
{
	//auto scriptsLag = sf::Time::Zero;
	//auto systemsLag = sf::Time::Zero;
	auto lag = sf::Time::Zero;

	while (window.isOpen()) {

		sf::Event event;
		while (window.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
				window.close();
			case sf::Event::Resized: {
				auto[x, y] = window.getSize();
				auto aspectRatio = float(x) / float(y);
				view.setSize({ width * aspectRatio, (float)height });
				//window.setView(view);
			}
			default:
				currentScene->forwardEvent(event);
				break;
			}
		}
		window.clear(backGroundColor);

		delta_time = clock.restart();

		currentScene->processPendingData();
		
		currentScene->update();

		lag += deltaTime();
		while (lag >= fixed_time) {
			lag -= fixed_time;
			currentScene->fixedUpdate();
		}

		currentScene->render(window);

		window.display();
	}
}
