#include "ark/core/Engine.hpp"
#include "ark/core/State.hpp"
#include "ark/core/MessageBus.hpp"
#include "ark/ecs/Scene.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/gui/Gui.hpp"
#include "ark/util/ResourceManager.hpp"

namespace ark {

	const ComponentManager::ComponentMask& Entity::getComponentMask() { return manager->getComponentMaskOfEntity(*this); }

	void State::requestStackPush(int stateId)
	{
		this->stateStack->pushState(stateId);
	}

	void State::requestStackPop()
	{
		this->stateStack->popState();
	}

	void State::requestStackClear()
	{
		this->stateStack->clearStack();
	}

	std::unordered_map<std::type_index, Resources::Handler> Resources::handlers;

	void Engine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
	{
		fixed_time = fixedUpdateTime;
		width = vm.width;
		height = vm.height;
		view.setSize(vm.width, vm.height);
		view.setCenter(0, 0);
		window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
		stateStack.mMessageBus = &messageBus;

		Resources::addHandler<sf::Texture>("textures", Resources::load_SFML_resource<sf::Texture>);
		Resources::addHandler<sf::Font>("fonts", Resources::load_SFML_resource<sf::Font>);
		Resources::addHandler<sf::Image>("imags", Resources::load_SFML_resource<sf::Image>);
	}

	MessageBus Engine::messageBus;
	StateStack Engine::stateStack;

	void Engine::updateEngine()
	{
		// handle events
		sf::Event event;
		while (window.pollEvent(event)) {

			switch (event.type) {

			case sf::Event::Closed:
				window.close();
				stateStack.handleEvent(event);
				break;

			case sf::Event::Resized:
			{
				auto [x, y] = window.getSize();
				auto aspectRatio = float(x) / float(y);
				view.setSize({width * aspectRatio, (float) height});
				stateStack.handleEvent(event);
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

	void Engine::run()
	{
		auto lag = sf::Time::Zero;

		clock.restart();

		while (window.isOpen()) {

			delta_time = clock.restart();

#if defined _DEBUG || defined USE_DELTA_TIME
			updateEngine();
			window.clear(backGroundColor);

			stateStack.preRender(window);
			stateStack.render(window);
			stateStack.postRender(window);
			window.display();
#else
			lag += delta_time;
			while (lag >= fixed_time) {
				lag -= fixed_time;
				updateEngine();

				window.clear(backGroundColor);

				stateStack.preRender(window);
				stateStack.render(window);
				stateStack.postRender(window);
				window.display();
			}
#endif

		}
		
	}
}
