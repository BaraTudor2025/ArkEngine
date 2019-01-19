#include "VectorEngine.hpp"
#include <iostream>
#include <vector>

#if 0
#define ecs_log(fmt, ...) log(fmt, __VA_ARGS__)
#else
#define ecs_log(fmt, ...)
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

struct SeppukuException { Script* script; };

void Script::seppuku()
{
	throw SeppukuException{this};
}

Script::~Script()
{
	unRegister();
}

Entity::Entity(bool registered) : tag_(tagCounter++), registered(registered)
{
	if (registered) {
		entities.push_back(this);
	}
}

Entity::~Entity()
{
	for (auto& s : scripts)
		s->entity_ = nullptr;
	this->unRegister();
}

Entity& Entity::operator=(Entity&& other)
{
	if (this != &other) {
		this->tag_ = other.tag_;
		this->components = std::move(other.components);
		this->scripts = std::move(other.scripts);
		for (auto& s : scripts)
			s->entity_ = this;
		for (auto& c : components)
			c->entity_ = this;
		if (other.registered) {
			erase(entities, &other);
			other.registered = false;
			entities.push_back(this);
			this->registered = true;
		} else
			this->registered = false;
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
	}
	registered = true;
}

void Entity::unRegister()
{
	this->registered = false;
	if (!this->registered)
		return;
	for (auto& s : scripts)
		s->unRegister();
	for (auto& c : components)
		c->unRegister();
	erase(entities, this);
}

void Entity::addComponent(std::unique_ptr<Component> c)
{
	if (this->registered) {
		c->Register();
		c->entity_ = this;
	}
	components.push_back(std::move(c));
}

void Entity::addScript(std::unique_ptr<Script> s)
{
	if (this->registered) {
		s->entity_ = this;
		s->Register();
	}
	if (VectorEngine::running())
		s->init();
	scripts.push_back(std::move(s));
}

void VectorEngine::create(sf::VideoMode vm, std::string name)
{
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize);
}

void VectorEngine::addSystem(System* s)
{
	systems.push_back(s);
}

template <typename F, typename...Args>
void VectorEngine::forEachScript(F f, Args&&...args)
{
	for (auto it = Script::scripts.begin(); it != Script::scripts.end();) {
		try {
			std::invoke(f, *it, std::forward<Args>(args)...);
			//(*it)->init();
			it++;
		} catch (const SeppukuException& exp) {
			exp.script->unRegister();
			erase_if(exp.script->entity_->scripts, [&](auto& s) { return s.get() == exp.script; });
			//std::cout << "SEPPUKU\n";
		}
	}
}

void VectorEngine::run()
{
	for (auto& s : systems)
		s->init();
	
	forEachScript(&Script::init);

	running_ = true;

	while (window.isOpen()) {

		sf::Event ev;
		while (window.pollEvent(ev)) {
			switch (ev.type) {
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::Resized: {
				auto[x, y] = window.getSize();
				auto aspectRatio = float(x) / float(y);
				view.setSize({ width / aspectRatio, height / aspectRatio });
			}	break;
			default:
				forEachScript(&Script::handleInput, ev);
				break;
			}
		}
		window.clear();

		delta_time = clock.restart();

		forEachScript(&Script::update);

		for (auto system : systems)
			system->update();

		for (auto system : systems)
			system->render(window);

		window.display();
	}
}
