#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include "Util.hpp"

static sf::Vector2f mouseCoords(sf::RenderWindow& window)
{
	sf::Vector2i mouse = sf::Mouse::getPosition(window);
	return window.mapPixelToCoords(mouse);
}

class VectorEngine : public NonCopyable, public NonMovable {
public:
	VectorEngine(sf::VideoMode vm, std::string name) 
		: width(vm.width), height(vm.height),
		view(sf::Vector2f(0, 0), sf::Vector2f(vm.width, vm.height)) 
	{
		window.create(sf::VideoMode(800, 600), "Articifii!", sf::Style::Close | sf::Style::Resize);
	}

	void run();

private:
	friend class Entity;
	static inline bool appStarted = false;
	sf::RenderWindow window;
	sf::View view;
	uint32_t width, height;
};
