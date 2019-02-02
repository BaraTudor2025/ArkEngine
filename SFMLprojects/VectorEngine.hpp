#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <deque>
#include <any>
#include <iostream>
#include "Util.hpp"
#include "static_any.hpp"

//#define VENGINE_BUILD_DLL 0
//
//#if VENGINE_BUILD_DLL
//	#define VECTOR_ENGINE_API __declspec(dllexport)
//#else
//	#define VECTOR_ENGINE_API __declspec(dllimport)
//#endif

#define VECTOR_ENGINE_API

class Entity;
class Scene;

extern int getUniqueComponentID();

template <typename T>
struct VECTOR_ENGINE_API Component {

public:
	Entity* entity() { return entity_; }

	void setActive(bool b);

	static inline const int id = getUniqueComponentID();

protected:
	Entity* entity_;
	Scene* scene_;

	struct Vector { 
		std::vector<T> data; 
		std::vector<bool> active; 
	};

private:
	friend class Scene;
	friend class Entity;
};

template <typename T> using is_component = std::is_base_of<Component<T>, T>;
template <typename T> constexpr bool is_component_v = is_component<T>::value;

struct VECTOR_ENGINE_API Transform : public Component<Transform>, sf::Transformable {
	using sf::Transformable::Transformable;
	operator const sf::Transform&() const { return this->getTransform();  }
	operator const sf::RenderStates&() const { return this->getTransform(); }
};

class VECTOR_ENGINE_API Script : public NonCopyable {

public:
	Script() = default;
	virtual ~Script() = default;

protected:
	virtual void init() { }
	virtual void update() { }
	virtual void fixedUpdate(sf::Time) { } // maybe use fixedTime
	virtual void handleEvent(sf::Event) { }
	template <typename T> T* getComponent();
	template <typename T> T* getScript();
	Entity* entity() { return entity_; }
	const Entity* entity() const { return entity_; }

private:
	Entity* entity_ = nullptr;

	friend class VectorEngine;
	friend class Entity;
};

template <typename T> using is_script = std::is_base_of<Script, T>;
template <typename T> constexpr bool is_script_v = is_script<T>::value;

class Scene;

class VECTOR_ENGINE_API Entity final : public NonCopyable {

public:
	explicit Entity(std::any tag = std::any()) : tag(tag), id_(idCounter++) { };

	// move ctor is broken, don't use
	Entity(Entity&& other) { *this = std::move(other); }
	Entity& operator=(Entity&& other);
	~Entity() = default;

	std::any tag;

	int id() { return id_; }

	// TODO: ?
	void setActive(bool b){ }

	void setAction(std::function<void(Entity&, std::any)> f, std::any args = std::any());

	template <typename T> T* getScript();

	template <typename T>
	T* getComponent();

	template <typename T, typename...Args> 
	T* addComponent(Args&&...args);

	template <typename T, typename... Args>
	Script* addScript(Args&&... args);

private:
	using ComponentTypeID = int;
	using ComponentIndex = int;
	std::unordered_map<ComponentTypeID, ComponentIndex> components;
	std::vector<Script*> scripts;
	std::function<void(Entity&, std::any)> action = nullptr;
	std::unique_ptr<std::any> actionArgs;
	int id_;
	Scene* scene;

	static inline int idCounter = 1;

	template <class> friend struct Component;
	friend class VectorEngine;
	friend class Scene;
	friend class Script;
	friend class System;
};

// fiecare sistem ar trebui sa aiba o singura instanta
class VECTOR_ENGINE_API System : public NonCopyable{

public:
	System() = default;
	virtual ~System() = 0;

protected:

	template <typename T, typename F>
	void forEach(F f);

	std::vector<Entity*>& getEntities();

private:
	virtual void init() { }
	virtual void handleEvent(sf::Event) { }
	virtual void update() { }
	virtual void fixedUpdate(sf::Time) { }
	virtual void render(sf::RenderTarget& target) { }

	Scene* scene;
	friend class VectorEngine;
	friend class Scene;
	template <typename T> friend struct Component;
};

class VECTOR_ENGINE_API Scene {

public:
	Scene() = default;
	virtual void init() = 0;
	virtual ~Scene() = default;

protected:

	template <typename T, typename...Args>
	void addSystem(Args&&...args) {
		this->systems.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		this->systems.back()->scene = this;
	}

	template <typename T>
	void removeSystem() {
		for (auto& system : systems)
			if (auto s = dynamic_cast<T*>(system.get()); s)
				erase(systems, [&](auto& sys) { return sys.get() == s; });
	}

	void registerEntity(Entity& e) { 
		e.scene = this;
		entities.push_back(&e);
	}

	void registerEntity(Entity* e) { 
		registerEntity(*e);
	}

	template <typename... Ts>
	void addComponentType()
	{
		(std::cout << ... << Ts::id);
		std::cout << '\n';
		static_assert((... && is_component_v<Ts>));
		((this->componentTable[Component<Ts>::id] = Component<Ts>::Vector()),...);
	}

private:
	template <typename T>
	auto& getComponents()
	{
		using vector_t = typename Component<T>::Vector;
		using ref_vector_t = vector_t&;
		auto& comps = this->componentTable.at(Component<T>::id);
		return any_cast<vector_t>(comps);
		//return std::any_cast<ref_vector_t>(comps);
	}

	template <typename T, typename...Args>
	int pushComponent(Args&&...args)
	{
		auto& comps = this->getComponents<T>();
		comps.data.emplace_back(std::forward<Args>(args)...);
		comps.active.push_back(true);
		return comps.data.size() - 1;
	}

private:
	std::vector<Entity*> entities;
	std::vector<std::unique_ptr<Script>> scripts;
	std::vector<std::unique_ptr<System>> systems;
	//std::unordered_map<int, std::any> componentTable;
	std::unordered_map<int, static_any<sizeof(Component<Transform>::Vector)>> componentTable;

	friend class VectorEngine;
	friend class Script;
	friend class Entity;
	friend class System;
	template <class> friend struct Component;
};

class VECTOR_ENGINE_API VectorEngine final : public NonCopyable {

public:
	static inline const sf::VideoMode resolutionFullHD{1920, 1080};
	static inline const sf::VideoMode resolutionNormalHD{1280, 720};
	static inline const sf::VideoMode resolutionFourByThree{1024, 768};

	/* fixedUpdateTime is the time of each fixed frame */
	static void create(sf::VideoMode vm, std::string name, sf::Time fixedUpdateTime ,sf::ContextSettings = sf::ContextSettings());

	static sf::Vector2u windowSize() { return { width, height }; }

	static void run();

	static void setVSync(bool enabled) { window.setVerticalSyncEnabled(enabled); }

	static void setFPSlimit(int fps) { window.setFramerateLimit(fps); }

	template <typename T>
	static void setScene() { currentScene = std::make_unique<T>(); initScene(); }

	static sf::Vector2f mousePositon() { return window.mapPixelToCoords(sf::Mouse::getPosition(window)); }

	static sf::Time deltaTime() { return delta_time + clock.getElapsedTime(); }
	static sf::Time fixedTime() { return frameTime; }

	static bool running() { return running_; }

	static sf::Vector2f center() { return static_cast<sf::Vector2f>(VectorEngine::windowSize()) / 2.f; }

	static inline sf::Color backGroundColor;
	
	// should be used only for debuging purpose
	static sf::RenderWindow& debugGetWindow() { return window; }

private:

	static void initScene();

	static inline sf::RenderWindow window;
	static inline sf::View view;
	static inline sf::Time delta_time;
	static inline sf::Time frameTime;
	static inline sf::Clock clock;
	static inline uint32_t width, height;
	static inline bool running_ = false;
	static inline std::unique_ptr<Scene> currentScene;
	friend class System;
};


/*********************************/
/******** IMPLEMENTATION *********/
/*********************************/

template <typename T> T* Script::getComponent() { return entity_->getComponent<T>(); }
template <typename T> T* Script::getScript() { return entity_->getScript<T>(); }

template<typename T>
T* Entity::getScript()
{
	static_assert(is_script_v<T>);
	for (auto& s : scripts) {
		auto p = dynamic_cast<T*>(s);
		if (p)
			return p;
	}
	std::cerr << "entity id(" << this->id() << "): nu am gasit scriptul :( \n";
	return nullptr;
}

template<typename T>
inline T* Entity::getComponent()
{
	static_assert(is_component_v<T>);
	try {
		auto index = this->components.at(Component<T>::id);
		auto& cs = scene->getComponents<T>();
		return &cs.data[index];
	}
	catch (const std::out_of_range& e) {
		std::cerr << "didn't find component\n";
		std::cerr << e.what() << '\n';
		return nullptr;
	}
}

template<typename T, typename ...Args>
inline T* Entity::addComponent(Args&& ...args)
{
	static_assert(is_component_v<T>);
	auto index = this->scene->pushComponent<T>(std::forward<Args>(args)...);
	this->components[T::id] = index;
	auto c = this->getComponent<T>();
	c->entity_ = this;
	c->scene_ = this->scene;
	return c;
}

template<typename T, typename ...Args>
inline Script* Entity::addScript(Args && ...args)
{
	static_assert(is_script_v<T>);
	auto script = std::make_unique<T>(std::forward<Args>(args)...);
	auto ptr = script.get();
	ptr->entity_ = this;
	this->scripts.push_back(ptr);
	this->scene->scripts.push_back(std::move(script));
	return ptr;
}

template<typename T>
inline void Component<T>::setActive(bool b)
{
	auto index = entity()->components[id];
	auto& cs = scene_->getComponents<T>();
	cs.active[index] = b;
}

template<typename T, typename F>
inline void System::forEach(F f)
{
	static_assert(is_component_v<T>);
	auto& cs = scene->getComponents<T>();
	for (int i = 0; i < cs.data.size(); i++)
		if (cs.active[i])
			std::invoke(f, cs.data[i]);
}
