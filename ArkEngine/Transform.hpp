#pragma once

#include "Core.hpp"
#include "Component.hpp"

#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/RenderStates.hpp>


struct ARK_ENGINE_API Transform : public Component, sf::Transformable {

	using sf::Transformable::Transformable;
	operator const sf::Transform&() const { return this->getTransform();  }
	operator const sf::RenderStates&() const { return this->getTransform(); }

	// TODO (Transform): add children hierarchy and move ctor

	//void addChild(Transform& child);
	//void removeChild(Transform& child);

	//sf::Transform getWorldTransform()
	//{
	//	if (m_parent)
	//		return this->getTransform() * m_parent->getWorldTransform();
	//	else
	//		return this->getTransform();
	//}
	
	//int depth() { return m_depth; }

private:
	//Transform* m_parent = nullptr;
	//std::vector<Transform*> m_children;
	//int m_depth;
};
