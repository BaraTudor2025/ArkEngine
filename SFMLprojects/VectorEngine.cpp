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

struct SeppukuException { Script* script; };

void Script::seppuku()
{
	throw SeppukuException{this};
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

template <typename F, typename...Args>
void VectorEngine::forEachScript(F f, Args&&...args)
{
	for (auto it = Script::scripts.begin(); it != Script::scripts.end();) {
		try {
			std::invoke(f, *it, std::forward<Args>(args)...);
			it++;
		} catch (const SeppukuException& exp) {
#if VEngineDebug
			static int seppukuCounter = 0;
			seppukuCounter++;
			debug_log("seppuku nr %d commited", seppukuCounter);
#endif
			exp.script->unRegister();
			erase_if(exp.script->entity_->scripts, [&](auto& s) { return s.get() == exp.script; });
			debug_log("removed script");
		}
	}
}

void VectorEngine::run()
{
	debug_log("start init systems");
	for (auto& s : systems)
		s->init();
	
	debug_log("start init scripts");
	forEachScript(&Script::init);

	running_ = true;
	auto scriptsLag = sf::Time::Zero;
	auto systemsLag = sf::Time::Zero;

	debug_log("start game loop scripts");
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
				forEachScript(&Script::handleEvent, ev);
				break;
			}
		}
		window.clear(backGroundColor);

		delta_time = clock.restart();

		forEachScript(&Script::update);

		scriptsLag += deltaTime();
		while (scriptsLag >= frameTime) {
			scriptsLag -= frameTime;
			forEachScript(&Script::fixedUpdate, frameTime);
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
