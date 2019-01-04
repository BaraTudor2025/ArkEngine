#include "VectorEngine.hpp"
#include <iostream>
#include <vector>

#if 0
#define ecs_log(fmt, ...) log(fmt, __VA_ARGS__)
#else
#define ecs_log(fmt, ...)
#endif

static sf::Vector2f mouseCoords(sf::RenderWindow& window)
{
	sf::Vector2i mouse = sf::Mouse::getPosition(window);
	return window.mapPixelToCoords(mouse);
}

void Script::unRegister() {
	if (!entity)
		return;
	registered = false;
	ecs_log("tag: %d", entity->tag);
	erase(scripts, this);
	ecs_log("am sters din scripts\n");
	erase_if(entity->scripts, [&](auto& s) { return s.get() == this; });
	ecs_log("am sters din entitate\n");
}

Script::~Script()
{
	if (entity && registered)
		unRegister();
}

Entity::Entity(bool registered) : tag_(tagCounter++), registered(registered)
{
	if (registered)
		entities.push_back(this);
}

Entity::~Entity()
{
	ecs_log("sterg entitatea: %d", tag);
	for (auto& s : scripts)
		s->entity = nullptr;
	erase(entities, this);
	ecs_log("am sters entitatea");
}

Entity& Entity::operator=(Entity&& other)
{
	if (this != &other) {
		this->tag_ = other.tag_;
		other.tag_ = 0;
		this->components = std::move(other.components);
		this->scripts = std::move(other.scripts);
		for (auto& s : scripts) {
			s->entity = this;
		}
		if (other.registered) {
			erase(entities, &other);
			entities.push_back(this);
			this->registered = true;
		} else
			this->registered = false;
	}
	return *this;
}

void Entity::Register()
{
	if (!registered)
		entities.push_back(this);
	registered = true;
}

void Entity::addScript(std::unique_ptr<Script> s)
{
	s->entity = this;
	s->Register();
	if (VectorEngine::appStarted)
		s->init();
	scripts.push_back(std::move(s));
}

VectorEngine::VectorEngine(sf::VideoMode vm, std::string name)
	: width(vm.width), height(vm.height),
	view(sf::Vector2f(0, 0), sf::Vector2f(vm.width, vm.height))
{
	window.create(sf::VideoMode(800, 600), "Articifii!", sf::Style::Close | sf::Style::Resize);
}

// TODO: luat systemele ca parametrii
void VectorEngine::run()
{
	for (auto& s : this->systems)
		s->init();

	for (auto& e : Entity::entities)
		for (auto& c : e->components)
			c->addThisToSystem();

	for (auto& s : Script::scripts)
		s->init();

	appStarted = true;

	sf::Clock clock;
	while (window.isOpen()) {
		sf::Event ev;
		while (window.pollEvent(ev)) {
			sf::Vector2f mousePos = mouseCoords(window);
			switch (ev.type) {
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::Resized: {
				auto[x, y] = window.getSize();
				auto aspectRatio = float(x) / float(y);
				view.setSize({ width / aspectRatio, height / aspectRatio });
			}
				break;
			case sf::Event::MouseButtonPressed:
				if (ev.mouseButton.button == sf::Mouse::Left)
					for (auto& s : Script::scripts)
						s->onMouseLeftPress(mousePos);
				if (ev.mouseButton.button == sf::Mouse::Right)
					for (auto& s : Script::scripts)
						s->onMouseRightPress(mousePos);
				break;
			case sf::Event::MouseButtonReleased:
				if (ev.mouseButton.button == sf::Mouse::Left)
					for (auto& s : Script::scripts)
						s->onMouseLeftRelease();
				if (ev.mouseButton.button == sf::Mouse::Right)
					for (auto& s : Script::scripts)
						s->onMouseRightRelease();
				break;
			default:
				break;
			}
		}
		window.clear();
		sf::Vector2f mousePos = mouseCoords(window);

		sf::Time deltaTime = clock.restart();

		for (auto& script : Script::scripts)
			script->update(deltaTime, mousePos);

		for (auto system : this->systems)
			system->update(deltaTime, mousePos);

		for (auto system : this->systems)
			system->draw(window);

		window.display();
	}
}

void VectorEngine::addSystem(System * s)
{
	this->systems.push_back(s);
}
