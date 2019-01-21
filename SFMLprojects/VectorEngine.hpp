#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <deque>
#include <any>
#include <iostream>
#include "Util.hpp"

//#define VENGINE_BUILD_DLL 0
//
//#if VENGINE_BUILD_DLL
//	#define VECTOR_ENGINE_API __declspec(dllexport)
//#else
//	#define VECTOR_ENGINE_API __declspec(dllimport)
//#endif

#define VECTOR_ENGINE_API

class Entity;
class System;

struct VECTOR_ENGINE_API Component {

public:
	virtual ~Component() = default;
	const Entity* entity() const { return entity_; }
	Entity* entity() { return entity_; }

protected:
	Entity* entity_;

private:
	virtual void Register() = 0;
	virtual void unRegister() = 0;

	friend class Entity;
};


template <typename T>
struct VECTOR_ENGINE_API Data : Component {

public:
	virtual ~Data() { unRegister();  }

protected:
	static inline System* system = nullptr;

private:
	void Register() override;
	void unRegister() override;

	bool registered = false;
	static inline std::vector<T*> components;
	friend class System;
};


struct VECTOR_ENGINE_API Transform : public Data<Transform>, sf::Transformable {
	using sf::Transformable::Transformable;
	operator const sf::Transform&() const { return this->getTransform();  }
	operator const sf::RenderStates&() const { return this->getTransform(); }
};

/* convinient alias, 
 * use only if entity has transformable component
 * use: getComponent<Transform>();
*/

// using Transform = sf::Transformable;

//// TODO: special rectangle shape class
//struct VECTOR_ENGINE_API RectangleShape : public Data<RectangleShape>, sf::RectangleShape { 
//	using sf::RectangleShape::RectangleShape;
//};


class VECTOR_ENGINE_API Script : public NonCopyable {

public:
	Script() = default;
	virtual ~Script();

protected:
	virtual void init() { }
	virtual void update() { }
	//virtual void fixedUpdate() { }
	virtual void handleInput(sf::Event) { }
	template <typename T> T* getComponent();
	template <typename T> T* getScript();
	Entity* entity() { return entity_; }
	const Entity* entity() const { return entity_; }

	// unRegister() si il scoate din Entitate
	// poate fi apelat in orice functie virtuala overriden
	void seppuku();

private:
	void Register();
	void unRegister();

	Entity* entity_ = nullptr;
	bool registered = false;

	static inline std::vector<Script*> scripts;
	friend class VectorEngine;
	friend class Entity;
};

class VECTOR_ENGINE_API Entity final : public NonCopyable {

public:
	explicit Entity(bool registered = false);
	Entity(Entity&& other) { *this = std::move(other); }
	Entity& operator=(Entity&& other);
	~Entity();

	void Register();

	std::any tag;

	int id() { return id_; }

	template <typename T> T* getComponent(); 
	template <typename T> T* getScript();

	Component* addComponent(std::unique_ptr<Component> c);
	Script* addScript(std::unique_ptr<Script> s);

	template <typename T, typename... Args>
	T* addComponent(Args&&... args) {
		return static_cast<T*>(addComponent(std::make_unique<T>(std::forward<Args>(args)...)));
	}

	template <typename T, typename... Args>
	Script* addScript(Args&&... args) {
		return addScript(std::make_unique<T>(std::forward<Args>(args)...));
	}

private:
	void unRegister();

	std::vector<std::unique_ptr<Component>> components;
	std::vector<std::unique_ptr<Script>> scripts;
	bool registered;
	int id_;

	static inline int tagCounter = 1;
	static inline std::vector<Entity*> entities;

	friend class VectorEngine;
	friend class Script;
};

// helpers

template <typename T>
inline void registerEntities(std::vector<T>& v)
{
	if constexpr(std::is_same_v<std::unique_ptr<Entity>, T> || 
	             std::is_same_v<std::shared_ptr<Entity>, T>)
		for (auto& e : v)
			e->Register();
	else
		for (auto& e : v)
			e.Register();
}

template <typename T, typename...Args>
void constructVector(std::vector<std::unique_ptr<T>>& range, int n, const Args&...args)
{
	range.reserve(n);
	for (int i = 0; i < n; i++)
		range.emplace_back(std::make_unique<T>(args...));
}

template <typename T>
std::vector<Entity> makeEntitiesFromComponents(std::vector<std::unique_ptr<T>> comps)
{
	std::vector<Entity> entities(comps.size());
	for (int i = 0; i < comps.size(); i++)
		entities[i].addComponent(std::move(comps[i]));
	return entities;
}

// fiecare sistem ar trebui sa aiba o singura instanta
class VECTOR_ENGINE_API System : public NonCopyable, public NonMovable {

public:
	System() = default;

protected:
	template <typename Comp>
	void initFrom() {
		Comp::system = this;
		for (Comp* c : Comp::components) {
			this->add(c);
		}
	}

	template <typename T>
	std::vector<T*>& getComponents() {
		return Data<T>::components;
	}

private:
	virtual void init() { }
	virtual void update() = 0;
	virtual void render(sf::RenderTarget& target) = 0;
	virtual void add(Component*) = 0;
	virtual void remove(Component*) = 0;

	friend class VectorEngine;
	template <typename T> friend struct Data;
};


class VECTOR_ENGINE_API VectorEngine final : public NonCopyable, public NonMovable {

public:
	static void create(sf::VideoMode vm, std::string name, sf::ContextSettings = sf::ContextSettings());

	static sf::Vector2u windowSize() { return { width, height }; }

	static void addSystem(System* s);

	static void run();

	static sf::Vector2f mousePositon() { return window.mapPixelToCoords(sf::Mouse::getPosition(window)); }

	static sf::Time deltaTime() { return delta_time + clock.getElapsedTime(); }

	static bool running() { return running_; }

	static sf::Vector2f center() { return static_cast<sf::Vector2f>(VectorEngine::windowSize()) / 2.f; }

	static inline sf::Color backGroundColor;

private:

	static inline sf::RenderWindow window;
	static inline sf::View view;
	static inline sf::Time delta_time;
	static inline sf::Clock clock;
	static inline uint32_t width, height;
	static inline bool running_ = false;
	static inline std::vector<System*> systems;

	template <class F, class...Args>
	static void forEachScript(F, Args&&...);
};


/*********************************/
/******** IMPLEMENTATION *********/
/*********************************/

template <typename T> T* Script::getComponent() { return entity_->getComponent<T>(); }
template <typename T> T* Script::getScript() { return entity_->getScript<T>(); }

template<typename T>
T* Entity::getComponent() 
{
	for (auto& c : components) {
		auto p = dynamic_cast<T*>(c.get());
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->id() << "): nu am gasit componenta :( \n";
	return nullptr;
}

template<typename T>
T* Entity::getScript()
{
	for (auto& s : scripts) {
		auto p = dynamic_cast<T*>(s.get());
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->id() << "): nu am gasit scriptul :( \n";
	return nullptr;
}

template<typename T>
inline void Data<T>::Register()
{
	if (!this->registered) {
		components.push_back(static_cast<T*>(this));
		this->registered = true;
		if (VectorEngine::running() && system)
			system->add(this);
	}
}

template<typename T>
inline void Data<T>::unRegister()
{
	if (this->registered) {
		erase(components, this);
		this->registered = false;
		if(system)
			system->remove(this);
	}
}
