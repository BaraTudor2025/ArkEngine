
#include "Engine.hpp"
#include "Scene.hpp"
#include "Entity.hpp"
#include "MessageBus.hpp"
#include "State.hpp"

const ComponentManager::ComponentMask& Entity::getComponentMask() { return manager->getComponentMaskOfEntity(*this); }

const std::string& Entity::name() { return manager->getNameOfEntity(*this); }

void Entity::setName(std::string name) { return manager->setNameOfEntity(*this, name); }

System::~System() {}

void State::requestStackPush(int stateId) {
	this->stateStack.pushState(stateId);
}

void State::requestStackPop() {
	this->stateStack.popState();
}

void State::requestStackClear() {
	this->stateStack.clearStack();
}

std::unordered_map<std::type_index, std::function<bool(void*, std::string)>> Resources::loaders;

void ArkEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
{
	fixed_time = fixedUpdateTime;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
	stateStack.messageBus = &messageBus;

	Resources::addLoader<sf::Texture>(Resources::sfmlResourceLoader<sf::Texture>);
	Resources::addLoader<sf::Font>(Resources::sfmlResourceLoader<sf::Font>);
	Resources::addLoader<sf::Image>(Resources::sfmlResourceLoader<sf::Image>);
}

MessageBus ArkEngine::messageBus;
StateStack ArkEngine::stateStack;

void ArkEngine::updateEngine()
{
	// handle events
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
			stateStack.handleEvent(event);
			break;
		}
	}

	// handle messages
	Message* p;
	while (messageBus.pool(p))
		stateStack.handleMessage(*p);
	stateStack.processPendingChanges();
	stateStack.update();
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
		stateStack.render(window);

		window.display();
	}
}

