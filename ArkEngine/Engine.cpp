#include "Engine.hpp"
#include "Scene.hpp"
#include "Entity.hpp"
#include "MessageBus.hpp"
#include "State.hpp"
#include "ResourceManager.hpp"
#include "Gui.hpp"

#include <imgui.h>
#include <imgui-SFML.h>

const ComponentManager::ComponentMask& Entity::getComponentMask() { return manager->getComponentMaskOfEntity(*this); }

const std::string& Entity::name() { return manager->getNameOfEntity(*this); }

void Entity::setName(std::string name) { return manager->setNameOfEntity(*this, name); }

void State::requestStackPush(int stateId) {
	this->stateStack.pushState(stateId);
}

void State::requestStackPop() {
	this->stateStack.popState();
}

void State::requestStackClear() {
	this->stateStack.clearStack();
}

std::unordered_map<std::type_index, Resources::Handler> Resources::handlers;

void ArkEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
{
	fixed_time = fixedUpdateTime;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
	stateStack.messageBus = &messageBus;

	Resources::addHandler<sf::Texture>("textures", Resources::load_SFML_resource<sf::Texture>);
	Resources::addHandler<sf::Font>("fonts", Resources::load_SFML_resource<sf::Font>);
	Resources::addHandler<sf::Image>("imags", Resources::load_SFML_resource<sf::Image>);
}

MessageBus ArkEngine::messageBus;
StateStack ArkEngine::stateStack;

void ArkEngine::updateEngine()
{
	// handle events
	sf::Event event;
	while (window.pollEvent(event)) {

		ImGui::SFML::ProcessEvent(event);

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
	auto ImGuiDeltaTime = sf::seconds(1 / 30.f);

	ImGui::SFML::Init(window);
	InternalGui::init();

	clock.restart();

	while (window.isOpen()) {

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

		ImGui::SFML::Update(window, ImGuiDeltaTime);
		InternalGui::render();

		window.clear(backGroundColor);

		stateStack.render(window);
		ImGui::SFML::Render(window);
		window.display();
	}
	ImGui::SFML::Shutdown();
}

