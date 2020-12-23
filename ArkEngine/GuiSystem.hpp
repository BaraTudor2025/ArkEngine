#pragma once

#include <ark/core/Engine.hpp>
#include <ark/ecs/Component.hpp>
#include <ark/ecs/System.hpp>
#include <ark/util/ResourceManager.hpp>

#include <fstream>
#include "ScriptingSystem.hpp"

struct Text : ark::Component<Text>, sf::Text { 

	Text(std::string fontName = "KeepCalm-Medium.ttf") : fileName(fontName) 
	{
		setFont(fontName);
	}

	bool moveWithMouse = false;

	void setFont(std::string fileName) {
		this->fileName = fileName;
		this->sf::Text::setFont(*ark::Resources::load<sf::Font>(fileName));
	}

	std::string_view getFontFamily() {
		return this->getFont()->getInfo().family.c_str();
	}

	std::string_view getFileName() {
		return fileName;
	}

private:
	std::string fileName;

	friend class TextSystem;
};

ARK_REGISTER_TYPE(Text, "TextComponent", ARK_DEFAULT_SERVICES) { return members(); }

struct Button : ark::Component<Button>, sf::RectangleShape {

	Button() = default;

	Button(sf::FloatRect rect, std::string texture = "")
	{
		setRect(rect);
		if(!texture.empty())
			setTexture(texture);
	}

	std::function<void()> onClick = []() {};

	void savePosition(std::string file) { 
		std::ofstream fout(ark::Resources::resourceFolder + "gui_data/" + file);
		auto[x, y] = this->getPosition();
		fout << x << ' ' << y;
	}

	void loadPosition(std::string file) {
		std::ifstream fin(ark::Resources::resourceFolder + "gui_data/" + file);
		float x, y;
		fin >> x >> y;
		this->setPosition(x, y);
	}

	void setRect(sf::FloatRect rect)
	{
		this->rect = rect;
		this->setSize({ this->rect.width, this->rect.height });
		this->move(this->rect.left, this->rect.top);
	}

	void setTexture(std::string fileName)
	{
		this->textureName = fileName;
		this->sf::RectangleShape::setTexture(ark::Resources::load<sf::Texture>(fileName));
	}

	bool moveWithMouse = false;

private:
	sf::FloatRect rect;
	std::string textureName;

	friend class ButtonSystem;
};

ARK_REGISTER_TYPE(Button, "ButtonComponent", ARK_DEFAULT_SERVICES) { return members(); }

class ButtonSystem : public ark::SystemT<ButtonSystem>, public ark::Renderer {

public:
	void init() override
	{
		querry = entityManager.makeQuerry<Button>();
	}

	void update() override
	{
		for (auto entity : getEntities()) {
			auto& b = entity.getComponent<Button>();
			if (b.moveWithMouse && isLeftMouseButtonPressed) {
				auto mouse = ark::Engine::mousePositon();
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

class TextSystem : public ark::SystemT<TextSystem>, public ark::Renderer {

public:
	void init() override
	{
		querry = entityManager.makeQuerry<Text>();
	}

	void update() override {}

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
	// TODO: update to ScriptingSystem 
	template <typename T>
	class MoveWithMouse : public ScriptT<MoveWithMouse<T>> {
		
		static_assert(std::is_base_of_v<ark::Component, T>);
		bool isLeftMouseButtonPressed = false;
		bool isRightMouseButtonPressed = false;
		sf::Mouse::Button mouseButton;
		ark::Transform* transform;
		T* component;

	public:
		MoveWithMouse(sf::Mouse::Button mouseButton = sf::Mouse::Left) : mouseButton(mouseButton) {}

	private:
		void bind() noexcept override
		{
			component = this->getComponent<T>();
			transform = this->getComponent<ark::Transform>();
		}

		void handleEvent(sf::Event event) noexcept override
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
				auto mouse = ark::Engine::mousePositon();
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

	//template<typename T>
	//class SaveGuiElementPosition : public Script {

	//public:
	//	SaveGuiElementPosition(std::string file, sf::Keyboard::Key key): file(file), key(key) { }

	//private:
	//	void init() {
	//		//component = getComponent<T>();
	//	}
	//	void handleEvent(sf::Event event)
	//	{
	//		switch (event.type) {
	//		case sf::Event::KeyPressed:
	//			if (event.key.code == key)
	//				component->savePosition(file);
	//		}
	//	}

	//private:
	//	std::string file;
	//	sf::Keyboard::Key key;
	//	T* component;
	//};
}

