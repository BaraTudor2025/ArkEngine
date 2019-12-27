#include "ark/core/Engine.hpp"
#include "ark/core/State.hpp"
#include "ark/core/MessageBus.hpp"
#include "ark/ecs/Scene.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/gui/Gui.hpp"
#include "ark/util/ResourceManager.hpp"

namespace ark {

	const ComponentManager::ComponentMask& Entity::getComponentMask() { return manager->getComponentMaskOfEntity(*this); }

	const std::string& Entity::getName() { return manager->getNameOfEntity(*this); }

	void Entity::setName(std::string name) { return manager->setNameOfEntity(*this, name); }

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

	void State::addTab(std::string name, std::function<void()> render)
	{
		InternalGui::addTab({name, render});
	}

	void State::removeTab(std::string name)
	{
		InternalGui::removeTab(name);
	}

	State::~State()
	{}

	std::unordered_map<std::type_index, Resources::Handler> Resources::handlers;

	void Engine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
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

	MessageBus Engine::messageBus;
	StateStack Engine::stateStack;

	void Engine::updateEngine()
	{
		// handle events
		sf::Event event;
		while (window.pollEvent(event)) {

			ImGui::SFML::ProcessEvent(event);

			switch (event.type) {

			case sf::Event::Closed:
				window.close();
				break;

			case sf::Event::Resized:
			{
				auto [x, y] = window.getSize();
				auto aspectRatio = float(x) / float(y);
				view.setSize({width * aspectRatio, (float) height});
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
		auto imguiLag = sf::Time::Zero;
		auto imgui_fixed_time = sf::seconds(1 / 60.f);

		ImGui::SFML::Init(window);
		InternalGui::init();

#ifdef NDEBUG
		// call update so that program doesn't crash
		ImGui::SFML::Update(window, imgui_fixed_time);
#endif

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

#if defined _DEBUG || defined USE_DELTA_TIME
			ImGui::SFML::Update(window, delta_time);
			InternalGui::render();
#else
			// updating imgui at a consistent rate
			imguiLag += delta_time;
			while (imguiLag >= imgui_fixed_time) {
				imguiLag -= imgui_fixed_time;
				ImGui::SFML::Update(window, imgui_fixed_time);
				InternalGui::render();
			}
#endif

			window.clear(backGroundColor);

			stateStack.render(window);
			ImGui::SFML::Render(window);
			window.display();
		}
		ImGui::SFML::Shutdown();
	}
}
