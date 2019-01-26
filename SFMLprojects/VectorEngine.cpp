#include "VectorEngine.hpp"
#include <iostream>
#include <vector>

#define VEngineDebug false

#if VEngineDebug
#define debug_log(fmt, ...) log(fmt, __VA_ARGS__)
#else
#define debug_log(fmt, ...)
#endif

void Script::Register()
{ 
	if(!this->registered)
		scripts.push_back(this);
	registered = true; 
}

void Script::unRegister() 
{
	if (this->registered)
		erase(scripts, this);
	registered = false;
}

Entity::~Entity()
{
	for (auto& s : scripts)
		s->entity_ = nullptr;
	for (auto& c : components)
		c->entity_ = nullptr;
	this->unRegister();
}

Entity& Entity::operator=(Entity&& other)
{
	if (this != &other) {
		this->id_ = other.id_;
		this->tag = std::move(other.tag);
		this->components = std::move(other.components);
		this->scripts = std::move(other.scripts);
		this->action = std::move(other.action);
		this->actionArgs = std::move(other.actionArgs);
		if (other.registered) {
			erase(entities, &other);
			other.registered = false;
			this->Register();
		} else {
			this->registered = false;
		}
	}
	return *this;
}

void Entity::Register()
{
	if (!registered) {
		entities.push_back(this);
		for (auto& c : components) {
			c->entity_ = this;
			c->Register();
		}
		for (auto& s : scripts) {
			s->entity_ = this;
			s->Register();
		}
		if (VectorEngine::running())
			for (auto& s : scripts)
				s->init();
		registered = true;
	}
}

void Entity::unRegister()
{
	if (!registered)
		return;
	for (auto& s : scripts)
		s->unRegister();
	for (auto& c : components)
		c->unRegister();
	erase(entities, this);
	registered = false;
}

Component* Entity::addComponent(std::unique_ptr<Component> c)
{
	if (registered) {
		c->entity_ = this;
		c->Register();
	}
	components.push_back(std::move(c));
	return components.back().get();
}

Script* Entity::addScript(std::unique_ptr<Script> s)
{
	if (registered) {
		s->entity_ = this;
		s->Register();
	}
	if (VectorEngine::running())
		s->init();
	scripts.push_back(std::move(s));
	return scripts.back().get();
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

void VectorEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUT, sf::ContextSettings settings)
{
	frameTime = fixedUT;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
}

void VectorEngine::addSystem(System* s)
{
	systems.push_back(s);
}

void VectorEngine::run()
{
	debug_log("start init systems");
	for (auto& s : systems)
		s->init();
	
	debug_log("start init scripts");
	for (auto s : Script::scripts)
		s->init();

	for (auto& e : Entity::entities)
		if (e->action) {
			if (e->actionArgs && e->actionArgs->has_value()) {
				e->action(*e, std::move(*e->actionArgs));
			} else {
				e->action(*e, nullptr);
			}
		}

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
				view.setSize({ width / aspectRatio, height / aspectRatio });
				window.setView(view);
			}	break;
			default:
				for (auto s : Script::scripts)
					s->handleEvent(event);
				break;
			}
		}
		window.clear(backGroundColor);

		delta_time = clock.restart();

		for (auto s : Script::scripts)
			s->update();

		scriptsLag += deltaTime();
		while (scriptsLag >= frameTime) {
			scriptsLag -= frameTime;
			for (auto s : Script::scripts)
				s->fixedUpdate(frameTime);
		}

		for (auto system : systems)
			system->update();

		systemsLag += deltaTime();
		while (systemsLag >= frameTime) {
			systemsLag -= frameTime;
			for (auto system : systems)
				system->fixedUpdate(frameTime);
		}

		for (auto system : systems)
			system->render(window);

		window.display();
	}
}
