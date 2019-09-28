#pragma once

#include <vector>
#include <memory>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Drawable.hpp>

class State {

public:

private:

};

class StateStack final : public sf::Drawable {

public:


private:
	std::vector<std::unique_ptr<State>> stack;

private:
	void draw(sf::RenderTarget& target, const sf::RenderStates& states)
	{

	}

};
