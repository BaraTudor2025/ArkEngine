#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <deque>
#include "Util.hpp"


struct Component {

public:
	virtual ~Component() = default;

private:
	virtual void Register() = 0;
	virtual void unRegister() = 0;
	friend class Entity;
};

class System;

// TODO: movable?
template <typename T>
struct Data : Component {

public:
	virtual ~Data() { unRegister();  }

protected:
	static inline System* system;
	static inline std::deque<T*> components;

private:
	void Register() override;

	void unRegister() override;

	bool registered = false;
};


class Entity;

class Script : public NonCopyable {

public:
	Script() = default;
	virtual ~Script();

protected:
	virtual void init() { }

	virtual void update() { }

	virtual void onMouseLeftPress() { }
	virtual void onMouseRightPress() { }
	virtual void onMouseLeftRelease() { }
	virtual void onMouseRightRelease() { }

	template <typename T> T* getComponent();

	template <typename T> std::vector<T*> getComponents();

	template <typename T> T* getScript();

	// unRegister() si il scoate din Entitate
	// poate fi apelat in orice functie virtuala overriden
	void seppuku();

private:
	void Register();
	void unRegister();
	Entity* entity = nullptr;
	bool registered = false;
	static inline std::vector<Script*> scripts;
	friend class VectorEngine;
	friend class Entity;
};


class Entity final : public NonCopyable {

public:
	explicit Entity(bool registered = false);

	Entity(Entity&& other)
	{
		*this = std::move(other);
	}

	Entity& operator=(Entity&& other);

	~Entity();

	// TODO: poate dau Register la Scripts doar in vectorii locali
	// si engine-ul depinde de entitati si nu std::vector<> Script::scripts
	// dar cum fac cu Data si Systems?
	void Register();

	int tag() { return tag_; }

	// prima componenta de tip T gasita este returnata
	template <typename T>
	T* getComponent(); 
 
	template <typename T>
	std::vector<T*> getComponents();

	template <typename T>
	T* getScript();

	template<typename Range>
	void addComponents(Range& range);

	template <typename T>
	void addComponentCopy(const T& c)
	{
		addComponent(std::make_unique<T>(c));
	}

	template <typename T, typename... Args>
	void addComponent(Args&&... args)
	{
		addComponent(std::make_unique<T>(std::forward<Args>(args)...));
	}

	void addComponent(std::unique_ptr<Component> c);

	template <typename T, typename... Args>
	void addScript(Args&&... args)
	{
		addScript(std::make_unique<T>(std::forward<Args>(args)...));
	}

	// ordinea in care sunt adaugate conteaza daca in init() apelezi getScript<WhateverScript>()
	// data WhateverScript::init() nu a fost apelat atunci varibilele sunt undefined 
	void addScript(std::unique_ptr<Script> s);

private:
	void unRegister();

	std::vector<std::unique_ptr<Component>> components;
	std::vector<std::unique_ptr<Script>> scripts;
	bool registered;
	int tag_;

	static inline int tagCounter = 1;
	static inline std::list<Entity*> entities;

	friend class VectorEngine;
	friend class Script;
};


// fiecare sistem ar trebui sa aiba o singura instanta
class System : public NonCopyable, public NonMovable {

public:
	System() = default;

protected:
	template <typename Comp>
	void initWith() {
		for (Comp* c : Comp::components) {
			this->add(c);
			c->system = this;
		}
	}

private:
	virtual void init() { }

	virtual void update() = 0;

	virtual void render(sf::RenderWindow&) = 0;

	virtual void add(Component*) = 0;

	virtual void remove(Component*) = 0;

	friend class VectorEngine;

	template <typename T>
	friend struct Data;
};


class VectorEngine final : public NonCopyable, public NonMovable {

public:
	static void create(sf::VideoMode vm, std::string name);

	static void addSystem(System* s);

	static void run();

	static sf::Vector2f mousePositon() { return window.mapPixelToCoords(sf::Mouse::getPosition(window)); }

	static sf::Time deltaTime() { return delta_time + clock.getElapsedTime(); }

	static bool running() { return running_; }
	
private:

	template <class F, class...Args>
	static void forEachScript(F, Args&&...);

	static inline sf::RenderWindow window;
	static inline sf::View view;
	static inline uint32_t width, height;
	static inline bool running_ = false;
	static inline sf::Time delta_time;
	static inline sf::Clock clock;

	static inline std::vector<System*> systems;
};


/*********************************/
/******** IMPLEMENTATION *********/
/*********************************/
template <typename T>
inline T* Script::getComponent()
{
	return entity->getComponent<T>();
}

template <typename T>
inline std::vector<T*> Script::getComponents()
{
	return entity->getComponents<T>();
}

template <typename T>
inline T* Script::getScript()
{
	return entity->getScript<T>();
}

template<typename T>
T* Entity::getComponent() 
{
	for (auto& c : components) {
		auto p = dynamic_cast<T*>(c.get());
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->tag() << "): nu am gasit componenta :( \n";
	return nullptr;
}

template<typename T>
std::vector<T*> Entity::getComponents()
{
	std::vector<T*> comps;
	for (auto& c : components) {
		auto p = dynamic_cast<T*>(c.get());
		if (p)
			comps.push_back(p);
	}
	if (comps.empty())
		std::cerr << "entity id(" << this->tag() << "): nu am gasit componentele :( \n";
	return comps;
}

template<typename T>
T* Entity::getScript()
{
	for (auto& s : scripts) {
		auto p = dynamic_cast<T*>(s.get());
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->tag() << "): nu am gasit scriptul :( \n";
	return nullptr;
}

template<typename Range>
void Entity::addComponents(Range& range)
{
	using elem_type = std::decay_t<decltype(*std::begin(range))>;
	if constexpr (std::is_convertible_v<elem_type, std::unique_ptr<Component>>)
		for (auto& c : range)
			addComponent(std::move(c));
	else
		for (auto& c : range)
			addComponent(std::make_unique<elem_type>(c));
}

template<typename T>
inline void Data<T>::Register()
{
	if (!this->registered)
		components.push_back(static_cast<T*>(this));
	if (VectorEngine::running())
		system->add(this);
	this->registered = true;
}

template<typename T>
inline void Data<T>::unRegister()
{
	if (this->registered) {
		system->remove(this);
		erase(components, this);
		this->registered = false;
	}
}
