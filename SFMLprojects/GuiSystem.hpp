#pragma once
#include "VectorEngine.hpp"
#include "ResourceManager.hpp"
#include <fstream>

// ? 
struct TextBox : Data<TextBox> {

};

struct Text : Data<Text>, sf::Text { 

	Text(std::string fontName = "KeepCalm-Medium.ttf") : fontName(fontName) { }

	bool moveWithMouse = false;

private:
	std::string fontName;
	friend class GuiSystem;
};

struct Button : Data<Button>, sf::RectangleShape {

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
	friend class GuiSystem;
};

class GuiSystem : public System {

	void init() {
		initFrom<Button>();
		initFrom<Text>();
	}

	void add(Component* c) {
		if (auto b = dynamic_cast<Button*>(c); b) {
			if (!b->textureName.empty()) {
				b->setTexture(load<sf::Texture>(b->textureName));
			}
		}
		if (auto t = dynamic_cast<Text*>(c); t) {
			t->setFont(*load<sf::Font>(t->fontName));
		}
	}

	void update() {
		auto process = [&](auto& components) {
			for (auto& c : components) {
				if (c->moveWithMouse && isLeftMouseButtonPressed) {
					auto mouse = VectorEngine::mousePositon();
					if (c->getGlobalBounds().contains(mouse)) {
						c->setPosition(mouse);
					}
				}
			}
		};
		process(this->getComponents<Button>());
		process(this->getComponents<Text>());
	}

	void handleEvent(sf::Event event) {
		switch (event.type)
		{
		case sf::Event::MouseButtonPressed: {
			if (event.mouseButton.button == sf::Mouse::Left) {
				isLeftMouseButtonPressed = true;
				for (auto& b : this->getComponents<Button>())
					if(b->getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y))
						b->onClick(); 
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
		for (auto& b : this->getComponents<Button>())
			target.draw(*b);
		for (auto& t : this->getComponents<Text>()) {
			target.draw(*t);
		}
	}

private:
	bool isLeftMouseButtonPressed = false;
	bool isRightMouseButtonPressed = false;
	sf::Vector2f prevMousePos{ 0,0 };
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
