#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include "Util.hpp"

class Entity;

struct Data {

	virtual ~Data() { }

protected:
	friend class VectorEngine;
	virtual void addThisToSystem() = 0;
};


class Script : public NonCopyable {
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

	virtual ~Script();

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


class Entity final : public NonCopyable {
	static inline size_t tagCounter = 1;
	static inline std::vector<Entity*> entities;
	friend class VectorEngine;
	friend class Script;
	// poate in viitor scot unique_ptr
	std::vector<std::unique_ptr<Data>> components;
	std::vector<std::unique_ptr<Script>> scripts;
	bool registered;
	int tag_;

public:

	int tag() { return tag_; }

	explicit Entity(bool registered = false);

	Entity(Entity&& other)
	{
		*this = std::move(other);
	}

	Entity& operator=(Entity&& other);

	~Entity();

	void Register();

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


// fiecare sistem ar trebui sa aiba o singura instanta
class System : public NonCopyable, public NonMovable {
private:
	friend class VectorEngine;
public:
	System() = default;
	virtual void init() { }
	virtual void update(sf::Time, sf::Vector2f) = 0;
	virtual void draw(sf::RenderWindow&) = 0;
	virtual void add(Data*) = 0;
};


class VectorEngine : public NonCopyable, public NonMovable {
public:
	VectorEngine(sf::VideoMode vm, std::string name);

	void run();

	void addSystem(System* s);

private:
	sf::RenderWindow window;
	sf::View view;
	uint32_t width, height;

	static inline std::vector<System*> systems;

	friend class Entity;
	static inline bool appStarted = false;
};


// implementare
template <typename T>
T* Script::getComponent()
{
	return entity->getComponent<T>();
}

template <typename T>
std::vector<T*> Script::getComponents()
{
	return entity->getComponents<T>();
}

template <typename T>
T* Script::getScript()
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
std::vector<T*> Entity::getComponents() // toate componentele gasite de tip T sunt returnate
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
	if constexpr (std::is_convertible_v<elem_type, std::unique_ptr<Data>>)
		for (auto& c : range)
			addComponent(std::move(c));
	else
		for (auto& c : range)
			addComponent(std::make_unique<elem_type>(c));
}