#pragma once
#include <array>
#include <SFML/Graphics.hpp>

// use sf::TriangleStrip as primitive when drawing
struct Quad {

	sf::Vertex* data() {
		return vertices.data();
	}

	sf::FloatRect getGlobalRect() { 
		auto topLeft = vertices[0].position;
		auto size = sf::Vector2f{ vertices[2].position.x - topLeft.x, vertices[1].position.y - topLeft.y };
		return { topLeft , size };
	}

	sf::IntRect getIntRect() { return static_cast<sf::IntRect>(getGlobalRect()); }

	// updateaza pozitia local, cu TopLeft(0,0)
	void updatePosTex(const sf::IntRect& uvRect)
	{
		auto h = std::abs(uvRect.height);
		auto w = std::abs(uvRect.width);
		vertices[0].position = sf::Vector2f(0, 0);
		vertices[1].position = sf::Vector2f(0, h);
		vertices[2].position = sf::Vector2f(w, 0);
		vertices[3].position = sf::Vector2f(w, h);
		this->updateTexCoords(uvRect);
	}

	void move(sf::Vector2f pos) {
		for (auto& v : vertices)
			v.position += pos;
	}

	void setColor(sf::Color a) {
		for (auto& v : vertices)
			v.color = a;
	}

	void setColors(sf::Color a, sf::Color b) {
		vertices[0].color = b;
		vertices[1].color = a;
		vertices[2].color = a;
		vertices[3].color = a;
	}

	void setAlpha(int8_t a) {
		for (auto& v : vertices)
			v.color.a = a;
	}

	//void updatePosition(sf::IntRect rect) { updatePosition(static_cast<sf::FloatRect>(rect)); }
	//void updateTexCoords(sf::IntRect rect) { updateTexCoords(static_cast<sf::FloatRect>(rect)); }

	void updatePosition(sf::FloatRect rect)
	{
		float left = rect.left;
		float right = left + rect.width;
		float top = rect.top;
		float bottom = top + rect.height;

		vertices[0].position = sf::Vector2f(left, top);
		vertices[1].position = sf::Vector2f(left, bottom);
		vertices[2].position = sf::Vector2f(right, top);
		vertices[3].position = sf::Vector2f(right, bottom);
	}

	template <typename T>
	void updateTexCoords(sf::Rect<T> rect)
	{
		float left = rect.left;
		float right = left + rect.width;
		float top = rect.top;
		float bottom = top + rect.height;

		vertices[0].texCoords = sf::Vector2f(left, top);
		vertices[1].texCoords = sf::Vector2f(left, bottom);
		vertices[2].texCoords = sf::Vector2f(right, top);
		vertices[3].texCoords = sf::Vector2f(right, bottom);
	}

private:
	std::array<sf::Vertex, 4> vertices;
};
