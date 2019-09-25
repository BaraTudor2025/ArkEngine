#include "ArkEngine.hpp"
#include <iostream>
#include <vector>

#define VEngineDebug false

#if VEngineDebug
#define debug_log(fmt, ...) log(fmt, __VA_ARGS__)
#else
#define debug_log(fmt, ...)
#endif

System::~System() { }

void Entity::setActive(bool b)
{
	this->isActive = b;
	for (auto& ids : this->components) {
		this->scene->setComponentActive(ids.first, ids.second, b);
	}
}

void Entity::setAction(std::function<void(Entity&, std::any)> f, std::any args)
{
	this->action = f;
	if (ArkEngine::running())
		this->action(*this, std::move(args));
	else {
		if(args.has_value())
			// save argumets until ArkEngine::run() is called
			this->actionArgs = std::make_unique<std::any>(std::move(args));
	}
}

void Entity::addChild(Entity* child)
{
	if (child->parent && child->parent != this)
		child->removeFromParent();
	this->children.push_back(child);
	child->parent = this;
}

void Entity::removeChild(Entity* child)
{
	if (child->parent == this)
		Util::erase(this->children, child);
}

void Entity::removeFromParent()
{
	this->parent->removeChild(this);
}

std::deque<Entity>& System::getEntities()
{
	return ArkEngine::currentScene->entities;
}

Entity* Scene::createEntity(std::string name)
{
	this->entities.emplace_back(name);
	Entity* e = &this->entities.back();
	e->scene = this;
	return e;
}

void Scene::setComponentActive(std::type_index typeId, int index, bool b)
{
	auto& data = this->componentTable.at(typeId);
	data.active.at(index) = b;
}

void ArkEngine::create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime, sf::ContextSettings settings)
{
	frameTime = fixedUpdateTime;
	width = vm.width;
	height = vm.height;
	view.setSize(vm.width, vm.height);
	view.setCenter(0, 0);
	window.create(vm, name, sf::Style::Close | sf::Style::Resize, settings);
}

void ArkEngine::initScene()
{
	currentScene->init();

	for (auto& s : currentScene->systems)
		s->init();

	for (auto& s : currentScene->scripts)
		s->init();

	for (auto& e : currentScene->entities)
		if (e.action) {
			if (e.actionArgs && e.actionArgs->has_value()) {
				e.action(e, std::move(*e.actionArgs));
			} else {
				e.action(e, nullptr);
			}
		}
}

void ArkEngine::run()
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
			if(s->entity()->isActive) // TODO: vector<bool> activeScripts; or a 'bool active' member of 'class Script'
				s->update();

		scriptsLag += deltaTime();
		while (scriptsLag >= frameTime) {
			scriptsLag -= frameTime;
			for (auto& s : currentScene->scripts)
				if(s->entity()->isActive)
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
