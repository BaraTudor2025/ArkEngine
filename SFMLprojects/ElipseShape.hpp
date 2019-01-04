#include <SFML/Graphics.hpp>
#include <cmath>
#pragma once

class EllipseShape : public sf::Shape
{
public:

	explicit EllipseShape(const sf::Vector2f& radius = sf::Vector2f(0.f, 0.f), size_t point_count=30)
		: m_radius(radius), m_point_count(point_count)
	{
		update();
	}

	explicit EllipseShape(float x, float y, size_t point_count=30)
		: EllipseShape(sf::Vector2f(x,y), point_count)
	{ }

	void setRadius(const sf::Vector2f& radius)
	{
		m_radius = radius;
		update();
	}

	const sf::Vector2f& getRadius() const { return m_radius; }

	virtual size_t getPointCount() const override { return m_point_count; }

	void setPointCount(size_t pc) { m_point_count = pc; }

	virtual sf::Vector2f getPoint(size_t index) const override
	{
		static const float pi = 3.141592654f;

		float angle = index * 2 * pi / getPointCount() - pi / 2;
		float x = std::cos(angle) * m_radius.x;
		float y = std::sin(angle) * m_radius.y;

		return sf::Vector2f(m_radius.x + x, m_radius.y + y);
	}
private:
	sf::Vector2f m_radius;
	size_t m_point_count;
};
