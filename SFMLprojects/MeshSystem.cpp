#include "MeshSystem.hpp"
#include "ResourceManager.hpp"

template <typename Range>
inline void updateVerticesWithRect(Range& vertices, const sf::IntRect& uvRect)
{
	auto h = std::abs(uvRect.height);
	auto w = std::abs(uvRect.width);
	vertices[0].position = sf::Vector2f(0, 0);
	vertices[1].position = sf::Vector2f(0, h);
	vertices[2].position = sf::Vector2f(w, 0);
	vertices[3].position = sf::Vector2f(w, h);

	float left = uvRect.left;
	float right = left + uvRect.width;
	float top = uvRect.top;
	float bottom = top + uvRect.height;

	vertices[0].texCoords = sf::Vector2f(left, top);
	vertices[1].texCoords = sf::Vector2f(left, bottom);
	vertices[2].texCoords = sf::Vector2f(right, top);
	vertices[3].texCoords = sf::Vector2f(right, bottom);
}

void MeshSystem::add(Component * c)
{
	if (auto mesh = static_cast<Mesh*>(c); mesh) {
		mesh->texture = load<sf::Texture>(mesh->fileName);
		mesh->vertices.resize(4);
		mesh->vertices.setPrimitiveType(sf::TriangleStrip);
		mesh->entity()->getComponent<Transform>()->setOrigin(static_cast<sf::Vector2f>(mesh->texture->getSize()) / 2.f);
		auto[a, b, c, d] = mesh->uvRect;
		if (a == 0 && b == 0 && c == 0 && d == 0) { // undefined uvRect
			mesh->uvRect.width = mesh->texture->getSize().x;
			mesh->uvRect.height = mesh->texture->getSize().y;
			mesh->uvRect.left = 0;
			mesh->uvRect.top = 0;
		}

		//sf::IntRect uvRect;

		//if (mesh->flipX) {
		//	uvRect.left = uvRect.width;
		//	uvRect.width = -uvRect.width;
		//} else {
		//	uvRect.left = 0;
		//} 
		//if (mesh->flipY) {
		//	uvRect.top = uvRect.height;
		//	uvRect.height = -uvRect.height;
		//} else {
		//	uvRect.top = 0;
		//}

		updateVerticesWithRect(mesh->vertices, mesh->uvRect);
	}
}

void MeshSystem::render(sf::RenderTarget & target)
{
	for (auto& m : this->getComponents<Mesh>()) {
		m->texture->setRepeated(m->repeatTexture);
		m->texture->setSmooth(m->smoothTexture);
		sf::RenderStates rs;
		rs.texture = m->texture;
		rs.transform = *m->entity()->getComponent<Transform>();
		target.draw(m->vertices, rs);
	}
}
