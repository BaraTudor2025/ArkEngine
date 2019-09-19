#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <deque>
#include <any>
#include <optional>
#include <iostream>
#include "Util.hpp"
#include "static_any.hpp"

//#define VENGINE_BUILD_DLL 0
//
//#if VENGINE_BUILD_DLL
//	#define ARK_ENGINE_API __declspec(dllexport)
//#else
//	#define ARK_ENGINE_API __declspec(dllimport)
//#endif

#define ARK_ENGINE_API

class Entity;
class Scene;

extern int getUniqueComponentID();

template <typename T>
struct ARK_ENGINE_API Component {

public:
	Entity* entity() { return entity_; }

	void setActive(bool b);

	static inline const int id = getUniqueComponentID();

private:
	Entity* entity_;
	Scene* scene_;

	friend class Scene;
	friend class Entity;
};

template <typename T> using is_component = std::is_base_of<Component<T>, T>;
template <typename T> constexpr bool is_component_v = is_component<T>::value;

struct ARK_ENGINE_API Transform : public Component<Transform>, sf::Transformable {
	using sf::Transformable::Transformable;
	operator const sf::Transform&() const { return this->getTransform();  }
	operator const sf::RenderStates&() const { return this->getTransform(); }
};




class ARK_ENGINE_API Script : public NonCopyable {

public:
	Script() = default;
	virtual ~Script() = default;

protected:
	virtual void init() { }
	virtual void update() { }
	virtual void fixedUpdate() { }
	virtual void handleEvent(sf::Event) { }
	template <typename T> T* getComponent();
	template <typename T> T* getScript();
	Entity* entity() { return entity_; }
	const Entity* entity() const { return entity_; }

private:
	Entity* entity_ = nullptr;

	friend class ArkEngine;
	friend class Entity;
};

template <typename T> using is_script = std::is_base_of<Script, T>;
template <typename T> constexpr bool is_script_v = is_script<T>::value;




class ARK_ENGINE_API Entity final : public NonCopyable, public NonMovable {

	Entity() : tag(std::any()), name(""),  id_(idCounter++) { };
	Entity(std::string name) : tag(std::any()), name(name), id_(idCounter++) { };

public:
	~Entity() = default;

	std::any tag;

	std::string name;

	int id() { return id_; }

	void setActive(bool b);

	void setAction(std::function<void(Entity&, std::any)> f, std::any args = std::any());

	template <typename T> 
	T* getScript();

	template <typename T>
	T* getComponent();

	template <typename T, typename...Args> 
	T* addComponent(Args&&...args);

	template <typename T, typename... Args>
	Script* addScript(Args&&... args);

	bool hasParent() { return this->parent != nullptr; }

	bool hasChildren() { return this->children.size() != 0; }

	Entity* getParent() { return this->parent; }

	void addChild(Entity* child);

	void removeChild(Entity* child);

	void removeFromParent();

	// recursive: retrieves the components of the childrens' children and etc...
	template <typename T>
	std::vector<T*> getChildrenComponents();

private:
	using ComponentTypeID = int;
	using ComponentIndex = int;
	std::unordered_map<ComponentTypeID, ComponentIndex> components;
	std::vector<Script*> scripts;
	std::vector<Entity*> children;
	Entity* parent = nullptr;
	std::function<void(Entity&, std::any)> action = nullptr;
	std::unique_ptr<std::any> actionArgs;
	int id_;
	bool isActive = true;
	Scene* scene = nullptr;

	static inline int idCounter = 1;

	template <class> friend struct Component;
	friend class ArkEngine;
	friend class Scene;
	friend class Script;
	friend class System;
	friend struct std::_Default_allocator_traits<std::allocator<Entity>>; // for visual studio 2019
};




// fiecare sistem ar trebui sa aiba o singura instanta
class ARK_ENGINE_API System : public NonCopyable{

public:
	System() = default;
	virtual ~System() = 0;

protected:

	template <typename T, typename F>
	void forEach(F f);

	std::deque<Entity>& getEntities();

private:
	virtual void init() { }
	virtual void update() { }
	virtual void fixedUpdate() { }
	virtual void handleEvent(sf::Event) { }
	virtual void render(sf::RenderTarget& target) { }

	Scene* scene = nullptr;
	friend class ArkEngine;
	friend class Scene;
	template <typename T> friend struct Component;
};




class ARK_ENGINE_API Scene {

public:
	Scene() = default;
	virtual ~Scene() = default;

protected:

	virtual void init() = 0;

	template <typename T, typename...Args>
	void addSystem(Args&& ...args);

	template <typename T>
	void addComponentType();

	template <typename T>
	void removeSystem();

	Entity* createEntity(std::string name = "");
	void createEntity(Entity*& entity, std::string name = "") { entity = createEntity(name); }

	template <typename...Ts>
	void createEntities(Ts& ... args)
	{
		((args = createEntity()),...);
	}

	template <typename Range>
	void createEntity(Range& range) {
		for (auto& e : range)
			e = createEntity();
	}

private:

	template <typename T> using ComponentContainer = std::deque<T>;

	// structure used by readers
	template <typename T>
	struct ComponentsData {
		ComponentContainer<T>& components;
		std::vector<bool>& active;
		const int sizeOfComponent;
	};

	template <typename T>
	ComponentsData<T> getComponentsData();

	void setComponentActive(int typeId, int index, bool b);

	int getComponentSize(int id);

	// returns std::pair{pointer to component, index of component}
	template <typename T, typename...Args>
	auto createComponent(bool isActive, Args&& ...args);

private:
	std::deque<Entity> entities;
	std::vector<std::unique_ptr<Script>> scripts;
	std::vector<std::unique_ptr<System>> systems;

	// size of component container
	static inline constexpr std::size_t sizeOfCC = sizeof(ComponentContainer<Component<Transform>>);

	// structure that is stored inside table
	// we use static_any to erase the type of the container, the type being ComponentContainer<T>
	struct InternalComponentsData {
		static_any<sizeOfCC> components;
		std::vector<bool> active;
		int sizeOfComponent;
	};

	// the key is the id of the component type
	std::unordered_map<int, InternalComponentsData> componentTable;

	friend class ArkEngine;
	friend class Script;
	friend class Entity;
	friend class System;
	template <class> friend struct Component;
};




class ARK_ENGINE_API ArkEngine final : public NonCopyable {

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

	static sf::Vector2f center() { return static_cast<sf::Vector2f>(ArkEngine::windowSize()) / 2.f; }

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
inline T* Entity::getScript()
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
		auto index = this->components.at(T::id);
		auto data = scene->getComponentsData<T>();
		return &data.components[index];
	}
	catch (const std::out_of_range& e) {
		std::cerr << "didn't find component, error msg: " << e.what() << '\n' ;
		return nullptr;
	}
}

template<typename T, typename ...Args>
inline T* Entity::addComponent(Args&& ...args)
{
	static_assert(is_component_v<T>);
	if (scene) {
		auto [comp, index] = this->scene->createComponent<T>(this->isActive, std::forward<Args>(args)...);
		this->components[T::id] = index;
		comp->entity_ = this;
		comp->scene_ = this->scene;
		return comp;
	} else {
		std::cerr << "entity isn't attached to scene\n";
		return nullptr;
	}
}

template<typename T, typename ...Args>
inline Script* Entity::addScript(Args && ...args)
{
	static_assert(is_script_v<T>);
	if (scene) {
		auto script = std::make_unique<T>(std::forward<Args>(args)...);
		auto ptr = script.get();
		ptr->entity_ = this;
		this->scripts.push_back(ptr);
		this->scene->scripts.push_back(std::move(script));
		return ptr;
	} else {
		std::cerr << "entity isn't attached to scene\n";
		return nullptr;
	}
}

template<typename T>
std::vector<T*> Entity::getChildrenComponents()
{
	if (!this->hasChildren())
		return {};
	std::vector<T*> comps;
	for (auto e : this->children) {
		if (auto c = e->getComponent<T>(); c)
			comps.push_back(c);
		if (e->hasChildren()) {
			std::vector<T*> childComps = e->getChildrenComponents<T>();
			comps.insert(comps.end(), childComps.begin(), childComps.end());
		}
	}
	return comps;
}

template<typename T>
inline void Component<T>::setActive(bool b)
{
	auto index = entity()->components[id];
	entity()->scene->setComponentActive(id, index, b);
}

template<typename T, typename F>
inline void System::forEach(F f)
{
	static_assert(is_component_v<T>);
	auto data = scene->getComponentsData<T>();
	for (int i = 0; i < data.components.size(); i++)
		if (data.active[i]) {
			std::invoke(f, data.components[i]);
		}
}

template<typename T, typename ...Args>
inline void Scene::addSystem(Args&& ...args)
{
	this->systems.push_back(std::make_unique<T>(std::forward<Args>(args)...));
	this->systems.back()->scene = this;
}

template<typename T>
inline void Scene::removeSystem()
{
	for (auto& system : systems)
		if (auto s = dynamic_cast<T*>(system.get()); s)
			erase(systems, [&](auto& sys) { return sys.get() == s; });
}

template<typename T>
inline void Scene::addComponentType()
{
	static_assert(is_component_v<T>);
	this->componentTable[T::id] = InternalComponentsData();
	this->componentTable.at(T::id).components = ComponentContainer<T>();
	this->componentTable.at(T::id).sizeOfComponent = sizeof(T);
}

template<typename T>
inline Scene::ComponentsData<T> Scene::getComponentsData()
{
	static_assert(is_component_v<T>);
	auto& data = this->componentTable.at(T::id);
	return ComponentsData<T>{
		any_cast<ComponentContainer<T>>(data.components),
			data.active,
			data.sizeOfComponent
	};
}

// returns [pointer to comp, index of comp]
template<typename T, typename ...Args>
inline auto Scene::createComponent(bool isActive, Args&& ...args)
{
	auto data = this->getComponentsData<T>();
	data.components.emplace_back(std::forward<Args>(args)...);
	data.active.push_back(isActive);
	return std::pair{&data.components.back(), data.components.size() - 1};
}
