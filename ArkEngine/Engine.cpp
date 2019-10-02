
#include "Engine.hpp"
#include "Scene.hpp"
#include "Entity.hpp"

const ComponentManager::ComponentMask& Entity::getComponentMask() { return manager->getComponentMaskOfEntity(*this); }

const std::string& Entity::name() { return manager->getNameOfEntity(*this); }

void Entity::setName(std::string name) { return manager->setNameOfEntity(*this, name); }

System::~System() {}

void ArkEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
{
	fixed_time = fixedUpdateTime;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
	//USE_FIXED_TIME = false;
}

void ArkEngine::handleEvents()
{
	sf::Event event;
	while (window.pollEvent(event)) {
		switch (event.type) {
		case sf::Event::Closed:
			window.close();
			break;
		case sf::Event::Resized: {
			auto[x, y] = window.getSize();
			auto aspectRatio = float(x) / float(y);
			view.setSize({ width * aspectRatio, (float)height });
			//window.setView(view);
			break;
		}
		default:
			currentScene->forwardEvent(event);
			break;
		}
	}
}

void ArkEngine::updateEngine()
{
	handleEvents();
	currentScene->processPendingData();
	currentScene->updateSystems();
	currentScene->updateScripts();
}

void ArkEngine::run()
{
	auto lag = sf::Time::Zero;

	clock.restart();

	while (window.isOpen()) {

		window.clear(backGroundColor);

		delta_time = clock.restart();

#if defined _DEBUG || defined USE_DELTA_TIME
		updateEngine();
#else
		lag += delta_time;
		while (lag >= fixed_time) {
			lag -= fixed_time;
			updateEngine();
		}
#endif
		currentScene->render(window);

		window.display();
	}
}

