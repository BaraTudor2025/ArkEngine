
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
}

void ArkEngine::run()
{
	//auto scriptsLag = sf::Time::Zero;
	//auto systemsLag = sf::Time::Zero;
	auto lag = sf::Time::Zero;
	auto scriptsLag = sf::Time::Zero;

	clock.restart();

	while (window.isOpen()) {

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
		window.clear(backGroundColor);

		delta_time = clock.restart();

		currentScene->processPendingData();
		
		// update scripts with the data of last frame 
		currentScene->updateScripts();
		currentScene->updateSystems();

		scriptsLag += deltaTime();
		while (scriptsLag >= fixed_time) {
			scriptsLag -= fixed_time;
			currentScene->fixedUpdateScripts();
			//scriptsLag += delta_time;
			currentScene->fixedUpdateSystems();
		}

		//currentScene->updateSystems();
		//currentScene->updateScripts();

		//lag += deltaTime();
		//while (lag >= fixed_time) {
		//	lag -= fixed_time;
		//	//currentScene->fixedUpdateSystems();
		//	currentScene->fixedUpdateScripts();
		//}

		currentScene->render(window);

		window.display();
	}
}
