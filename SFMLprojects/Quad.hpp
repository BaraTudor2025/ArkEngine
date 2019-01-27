#pragma once
#include <array>
#include <SFML/Graphics.hpp>

struct Quad {

	Quad(sf::PrimitiveType type) :type(type) { }

	sf::Vertex* data() {
		return vertices.data();
	}

	sf::PrimitiveType getPrimitive() { return type; }

	// updateaza pozitia local, cu TopLeft(0,0)
	void updatePosTex(const sf::IntRect& uvRect)
	{
		auto h = std::abs(uvRect.height);
		auto w = std::abs(uvRect.width);
		vertices[0].position = sf::Vector2f(0, 0);
		vertices[1].position = sf::Vector2f(0, h);
		if (type == sf::TriangleStrip) {
			vertices[2].position = sf::Vector2f(w, 0);
			vertices[3].position = sf::Vector2f(w, h);
		} else if (type == sf::Quads) {
			vertices[2].position = sf::Vector2f(w, h);
			vertices[3].position = sf::Vector2f(w, 0);
		}

		float left = uvRect.left;
		float right = left + uvRect.width;
		float top = uvRect.top;
		float bottom = top + uvRect.height;

		vertices[0].texCoords = sf::Vector2f(left, top);
		vertices[1].texCoords = sf::Vector2f(left, bottom);
		if (type == sf::TriangleStrip) {
			vertices[2].texCoords = sf::Vector2f(right, top);
			vertices[3].texCoords = sf::Vector2f(right, bottom);
		} else if (type == sf::Quads) {
			vertices[2].texCoords = sf::Vector2f(right, bottom);
			vertices[3].texCoords = sf::Vector2f(right, top);
		}
	}

	template <auto member>
	void update(sf::IntRect rect)
	{
		float left = rect.left;
		float right = left + rect.width;
		float top = rect.top;
		float bottom = top + rect.height;

		vertices[0].*member = sf::Vector2f(left, top);
		vertices[1].*member = sf::Vector2f(left, bottom);
		if (type == sf::TriangleStrip) {
			vertices[2].*member = sf::Vector2f(right, top);
			vertices[3].*member = sf::Vector2f(right, bottom);
		} else if(type == sf::Quads){
			vertices[2].*member = sf::Vector2f(right, bottom);
			vertices[3].*member = sf::Vector2f(right, top);
		}
	}

private:
	std::array<sf::Vertex, 4> vertices;
	sf::PrimitiveType type;
};
