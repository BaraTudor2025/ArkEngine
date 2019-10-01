#pragma once

#include "Engine.hpp"
#include "Component.hpp"
#include "System.hpp"
#include "ResourceManager.hpp"

#include <fstream>

struct Text : Component, sf::Text { 

	Text(std::string fontName = "KeepCalm-Medium.ttf") : fontName(fontName) { }

	bool moveWithMouse = false;

private:
	std::string fontName;

	friend class TextSystem;
};

struct Button : Component, sf::RectangleShape {

	Button(sf::FloatRect rect, std::string texture = "")
		: textureName(texture), rect(rect)
	{
		this->setSize({ this->rect.width, this->rect.height });
		this->move(this->rect.left, this->rect.top);
	}

	std::function<void()> onClick;
	void savePosition(std::string file) { 
		std::ofstream fout("./res/gui_data/" + file);
		std::cout << "mama\n";
		auto[x, y] = this->getPosition();
		fout << x << ' ' << y;
	}
	void loadPosition(std::string file) {
		std::ifstream fin("./res/gui_data/" + file);
		float x, y;
		fin >> x >> y;
		this->setPosition(x, y);
	}
	bool moveWithMouse = false;

private:
	sf::FloatRect rect;
	std::string textureName;

	friend class ButtonSystem;
};


class ButtonSystem : public System {

public:
	ButtonSystem() : System(typeid(ButtonSystem))
	{
		requireComponent<Button>();
	}

	void onEntityAdded(Entity entity) override
	{
		auto& b = entity.getComponent<Button>();
		if (!b.textureName.empty())
			b.setTexture(load<sf::Texture>(b.textureName)); 
	}

	void update() override
	{
		for (auto entity : getEntities()) {
			auto& b = entity.getComponent<Button>();
			if (b.moveWithMouse && isLeftMouseButtonPressed) {
				auto mouse = ArkEngine::mousePositon();
				if (b.getGlobalBounds().contains(mouse)) {
					b.setPosition(mouse);
				}
			}
		}
	}

	void handleEvent(sf::Event event) {
		switch (event.type)
		{
		case sf::Event::MouseButtonPressed: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = true;
				for (auto entity : getEntities()) {
					auto& b = entity.getComponent<Button>();
					if (b.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y))
						b.onClick();
				}
			}
			if (event.mouseButton.button == sf::Mouse::Right) {
				isRightMouseButtonPressed = true;
			}
		} break;
		case sf::Event::MouseButtonReleased: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = false;
			}
			if (event.mouseButton.button == sf::Mouse::Right) {
				isRightMouseButtonPressed = false;
			}

		} break;
		default:
			break;
		}
	}

	void render(sf::RenderTarget& target) override
	{
		for (auto entity : getEntities())
			target.draw(entity.getComponent<Button>());
	}

private:
	bool isLeftMouseButtonPressed = false;
	bool isRightMouseButtonPressed = false;
	sf::Vector2f prevMousePos{ 0,0 };

};

class TextSystem : public System {

public:
	TextSystem() : System(typeid(TextSystem))
	{
		requireComponent<Text>();
	}

	void onEntityAdded(Entity entity) override
	{
		auto& t = entity.getComponent<Text>();
		t.setFont(*load<sf::Font>(t.fontName));
	}

	void render(sf::RenderTarget& target) override
	{
		for (auto entity : getEntities())
			target.draw(entity.getComponent<Text>());
	}
};

#if 0
class GuiSystem : public System {

	void init() {
		forEach<Button>([](auto& b) {
			if (!b.textureName.empty())
				b.setTexture(load<sf::Texture>(b.textureName)); 
		});
		forEach<Text>([](auto& t) {
			t.setFont(*load<sf::Font>(t.fontName));
		});
	}

	void update() {
		auto process = [&](auto& c) {
			if (c.moveWithMouse && isLeftMouseButtonPressed) {
				auto mouse = ArkEngine::mousePositon();
				if (c.getGlobalBounds().contains(mouse)) {
					c.setPosition(mouse);
				}
			}
		};
		forEach<Button>(process);
		forEach<Text>(process);
	}

	void handleEvent(sf::Event event) {
		switch (event.type)
		{
		case sf::Event::MouseButtonPressed: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = true;
				forEach<Button>([&](auto& b){
						if (b.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y))
							b.onClick();
				});
			}
			if (event.mouseButton.button == sf::Mouse::Right) {
				isRightMouseButtonPressed = true;
			}
		} break;
		case sf::Event::MouseButtonReleased: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = false;
			}
			if (event.mouseButton.button == sf::Mouse::Right) {
				isRightMouseButtonPressed = false;
			}

		} break;
		default:
			break;
		}
	}

	void render(sf::RenderTarget& target) {
		auto proc = [&](auto& t) { 
			target.draw(t);
		};

		forEach<Button>(proc);
		forEach<Text>(proc);
	}

private:
	bool isLeftMouseButtonPressed = false;
	bool isRightMouseButtonPressed = false;
	sf::Vector2f prevMousePos{ 0,0 };
};
#endif

namespace GuiScripts {

	// component T must have getGlobalBounds() defined
	// momentan nefunctional
	template <typename T>
	class MoveWithMouse : public Script {
		
		static_assert(std::is_base_of_v<Component, T>);
		bool isLeftMouseButtonPressed = false;
		bool isRightMouseButtonPressed = false;
		sf::Mouse::Button mouseButton;
		Transform* transform;
		T* component;

	public:
		MoveWithMouse(sf::Mouse::Button mouseButton = sf::Mouse::Left) : mouseButton(mouseButton) {}

	private:
		void init() override
		{
			component = getComponent<T>();
			transform = getComponent<Transform>();
		}

		void handleEvent(sf::Event event) override
		{
			switch (event.type) 
			{
			case sf::Event::MouseButtonPressed: {
				if (event.mouseButton.button == sf::Mouse::Left)
					isLeftMouseButtonPressed = true;
				if (event.mouseButton.button == sf::Mouse::Right)
					isRightMouseButtonPressed = true;
			} break;

			case sf::Event::MouseButtonReleased: {
				if (event.mouseButton.button == sf::Mouse::Left)
					isLeftMouseButtonPressed = false;
				if (event.mouseButton.button == sf::Mouse::Right)
					isRightMouseButtonPressed = false;

			} break;

			default:
				break;
			}
		}

		void update() override
		{
			auto procces = [&]() {
				auto mouse = ArkEngine::mousePositon();
				// daca componenta mosteneste de la sf::Transformable atunci asta nu prea merge
				// TODO? : tre' sa transform rect-ul de la globalBounds cu componenta transform
				if (component->getGlobalBounds().contains(mouse))
					transform->setPosition(mouse);
			};

			if (mouseButton == sf::Mouse::Left && isLeftMouseButtonPressed)
				procces();
			else if(mouseButton == sf::Mouse::Right && isRightMouseButtonPressed)
				procces();
		}
	};

	template<typename T>
	class SaveGuiElementPosition : public Script {

	public:
		SaveGuiElementPosition(std::string file, sf::Keyboard::Key key): file(file), key(key) { }

	private:
		void init() {
			component = getComponent<T>();
		}
		void handleEvent(sf::Event event)
		{
			switch (event.type) {
			case sf::Event::KeyPressed:
				if (event.key.code == key)
					component->savePosition(file);
			}
		}

	private:
		std::string file;
		sf::Keyboard::Key key;
		T* component;
	};
}

