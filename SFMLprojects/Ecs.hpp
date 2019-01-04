#pragma once
#include <SFML/System/Time.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include "VectorEngine.hpp"
#include "Util.hpp"


#if 0
#define ecs_log(fmt, ...) log(fmt, __VA_ARGS__)
#else
#define ecs_log(fmt, ...)
#endif

class Entity;

// Component sau Data, numele sunt interschimbabile
struct Data {
	virtual ~Data() { ecs_log("sunt distrus"); }

protected:
	friend class VectorEngine;
	virtual void addThisToSystem() = 0;
};

class Script {
	static inline std::vector<Script*> scripts;
	friend class VectorEngine;
	friend class Entity;
	void Register() { registered = true; scripts.push_back(this); }
	Entity* entity = nullptr;
	bool registered = false;

protected:
	void unRegister();

public:

	Script() { }
	virtual ~Script() { 
		ecs_log("sunt distrus"); 
		if (entity && registered) 
			unRegister(); 
	}

	virtual void init() { }

	virtual void update(sf::Time deltaTime, sf::Vector2f mousePos) { }

	virtual void onMouseLeftPress(sf::Vector2f) { }
	virtual void onMouseRightPress(sf::Vector2f) { }
	virtual void onMouseLeftRelease() { }
	virtual void onMouseRightRelease() { }

	template <typename T> T* getComponent();

	template <typename T> std::vector<T*> getComponents();

	template <typename T> T* getScript();
};

#define DECLARE_ENTITY_ON_HEAP(name) auto name = std::make_unique<Entity>()

class Entity final : public NonCopyable {
	static inline size_t tagCounter = 1;
	static inline std::vector<Entity*> entities;
	friend class VectorEngine;
	friend class Script;
	// poate in viitor scot unique_ptr
	std::vector<std::unique_ptr<Data>> components;
	std::vector<std::unique_ptr<Script>> scripts;
	bool registered;

public:
	const int tag;

	Entity(bool registered = false) : tag(tagCounter++), registered(registered){
		if(registered)
			entities.push_back(this);
	}

	void Register() {
		if (!registered)
			entities.push_back(this);
		registered = true;
	}

	// TODO: de scris
	Entity(Entity&&) : tag(tagCounter++)
	{

	}

	Entity& operator=(Entity&&)
	{
		
	}

	~Entity() {
		ecs_log("sterg entitatea: %d", tag);
		for (auto& s : scripts)
			s->entity = nullptr;
		erase(entities, this);
		ecs_log("am sters entitatea");
	}

	// prima componenta de tip T gasita este returnata
	template <typename T>
	T* getComponent(); 

	template <typename T>
	std::vector<T*> getComponents();

	template <typename T>
	T* getScript();

	template<typename Range>
	void addComponents(Range& range);

	template <typename T, typename... Args>
	void addComponent(Args&&... args)
	{
		addComponent(std::make_unique<T>(std::forward<Args>(args)...));
	}

	void addComponent(std::unique_ptr<Data> c)
	{
		components.push_back(std::move(c));
	}

	template <typename T, typename... Args>
	void addScript(Args&&... args)
	{
		addScript(std::make_unique<T>(std::forward<Args>(args)...));
	}

	void addScript(std::unique_ptr<Script> s);
};

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

inline void Script::unRegister() {
	if (!entity)
		return;
	registered = false;
	ecs_log("tag: %d", entity->tag);
	erase(scripts, this);
	ecs_log("am sters din scripts\n");
	erase_if(entity->scripts, [&](auto& s) { return s.get() == this; });
	ecs_log("am sters din entitate\n");
}

// sistemul ar trebui sa aiba o singura instanta
class System {
private:
	static inline std::vector<System*> systems;
	friend class VectorEngine;
public:
	System() {
		systems.push_back(this);
	}
	virtual void update(sf::Time, sf::Vector2f) = 0;
	virtual void draw(sf::RenderWindow&) = 0;
	virtual void add(Data*) = 0;
};

template<typename T>
inline T * Entity::getComponent() 
{
	for (auto& c : components) {
		auto p = dynamic_cast<T*>(c.get());
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->tag << "): nu am gasit componenta :( \n";
	return nullptr;
}

template<typename T>
inline std::vector<T*> Entity::getComponents() // toate componentele gasite de tip T sunt returnate
{
	std::vector<T*> comps;
	for (auto& c : components) {
		auto p = dynamic_cast<T*>(c.get());
		if (p)
			comps.push_back(p);
	}
	if (comps.empty())
		std::cerr << "entity id(" << this->tag << "): nu am gasit componentele :( \n";
	return comps;
}

template<typename T>
inline T * Entity::getScript()
{
	for (auto& s : scripts) {
		auto p = dynamic_cast<T*>(s.get());
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->tag << "): nu am gasit scriptul :( \n";
	return nullptr;
}

template<typename Range>
inline void Entity::addComponents(Range& range)
{
	using elem_type = std::decay_t<decltype(*std::begin(range))>;
	if constexpr (std::is_convertible_v<elem_type, std::unique_ptr<Data>>)
		for (auto& c : range)
			addComponent(std::move(c));
	else
		for (auto& c : range)
			addComponent(std::make_unique<elem_type>(c));
}

inline void Entity::addScript(std::unique_ptr<Script> s)
{
	s->entity = this;
	s->Register();
	if (VectorEngine::appStarted)
		s->init();
	scripts.push_back(std::move(s));
}

#undef ecs_log