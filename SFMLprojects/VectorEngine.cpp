#include "VectorEngine.hpp"
#include <iostream>
#include <vector>

#define VEngineDebug false

#if VEngineDebug
#define debug_log(fmt, ...) log(fmt, __VA_ARGS__)
#else
#define debug_log(fmt, ...)
#endif

int getUniqueComponentID()
{
	static int id = 0;
	id += 1;
	return id;
}

System::~System() { }

Entity& Entity::operator=(Entity&& other)
{
	std::cout << "move\n";
	if (this != &other) {
		this->id_ = other.id_;
		this->tag = std::move(other.tag);
		this->components = std::move(other.components);
		this->scripts = std::move(other.scripts);
		for (auto s : this->scripts)
			s->entity_ = this;
		this->action = std::move(other.action);
		this->actionArgs = std::move(other.actionArgs);
		//TODO: forEach component.entity = this
		//if (other.registered && this->scene) {
		//	erase(scene->entities, &other);
		//	other.registered = false;
		//	this->Register();
		//} else {
		//	this->registered = false;
		//}
	}
	return *this;
}

void Entity::setAction(std::function<void(Entity&, std::any)> f, std::any args)
{
	this->action = f;
	if (VectorEngine::running())
		this->action(*this, std::move(args));
	else {
		if(args.has_value())
			// if not running save arguments unitl VectorEngine::run is called
			this->actionArgs = std::make_unique<std::any>(std::move(args));
	}
}

std::vector<Entity*>& System::getEntities()
{
	return VectorEngine::currentScene->entities;
}

void VectorEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUT, sf::ContextSettings settings)
{
	frameTime = fixedUT;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
}

void VectorEngine::initScene()
{
	currentScene->init();

	for (auto& s : currentScene->systems)
		s->init();

	for (auto& s : currentScene->scripts)
		s->init();

	for (auto& e : currentScene->entities)
		if (e->action) {
			if (e->actionArgs && e->actionArgs->has_value()) {
				e->action(*e, std::move(*e->actionArgs));
			} else {
				e->action(*e, nullptr);
			}
		}
}

void VectorEngine::run()
{
	auto scriptsLag = sf::Time::Zero;
	auto systemsLag = sf::Time::Zero;

	debug_log("start game loop");
	running_ = true;
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
			}	break;
			default:
				for (auto& s : currentScene->systems)
					s->handleEvent(event);
				for (auto& s : currentScene->scripts)
					s->handleEvent(event);
				break;
			}
		}
		window.clear(backGroundColor);

		delta_time = clock.restart();

		for (auto& s : currentScene->scripts)
			s->update();

		scriptsLag += deltaTime();
		while (scriptsLag >= frameTime) {
			scriptsLag -= frameTime;
			for (auto& s : currentScene->scripts)
				s->fixedUpdate();
		}

		for (auto& system : currentScene->systems)
			system->update();

		systemsLag += deltaTime();
		while (systemsLag >= frameTime) {
			systemsLag -= frameTime;
			for (auto& system : currentScene->systems)
				system->fixedUpdate();
		}

		for (auto& system : currentScene->systems)
			system->render(window);

		window.display();
	}
}
