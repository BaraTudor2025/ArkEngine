#pragma once
#include "VectorEngine.hpp"

struct Mesh : public Data<Mesh> {

	Mesh(std::string fileName, bool smoothTexture = false)
		:fileName(fileName), smoothTexture(smoothTexture), uvRect(0,0,0,0), repeatTexture(false) { }

	Mesh(std::string fileName, bool smoothTexture, sf::IntRect rect, bool repeatTexture)
		:fileName(fileName),  smoothTexture(smoothTexture), uvRect(uvRect), repeatTexture(repeatTexture){ }

private:
	const bool repeatTexture;
	const bool smoothTexture;
	//const bool flipX;
	//const bool flipY;
	const std::string fileName;
	sf::IntRect uvRect;
	sf::Texture* texture;
	sf::VertexArray vertices;
	friend class MeshSystem;
};

class MeshSystem : public System {

	void init() {
		initFrom<Mesh>();
	}

	void add(Component* c);

	void render(sf::RenderTarget& target);
};
