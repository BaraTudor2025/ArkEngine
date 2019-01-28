#pragma once
#include "VectorEngine.hpp"
#include "Quad.hpp"

struct Mesh : public Data<Mesh> {

	Mesh(std::string fileName, bool smoothTexture = false)
		:fileName(fileName), smoothTexture(smoothTexture), uvRect(0,0,0,0), repeatTexture(false) { }

	Mesh(std::string fileName, bool smoothTexture, sf::IntRect rect, bool repeatTexture)
		:fileName(fileName),  smoothTexture(smoothTexture), uvRect(uvRect), repeatTexture(repeatTexture){ }

	sf::Vector2f meshSize() { 
	    return static_cast<sf::Vector2f>(
		    sf::Vector2i{ std::abs(uvRect.width), std::abs(uvRect.height) }); 
	}

private:
	const bool repeatTexture;
	const bool smoothTexture;
	//const bool flipX;
	//const bool flipY;
	const std::string fileName;
	sf::IntRect uvRect;
	sf::Texture* texture;
	Quad vertices;
	friend class AnimationSystem;
};

struct Animation : public Data<Animation> {

	Animation(std::string fileName, sf::Vector2u frameCount, sf::Time frameTime, int row, bool smoothTexture)
	: fileName(fileName), frameCount(frameCount), frameTime(frameTime), row(row), smoothTexture(smoothTexture)
	{ }

	sf::Time frameTime;
	int row;
	bool flipX = false;
	bool flipY = false;

	sf::Vector2f frameSize() { 
	    return static_cast<sf::Vector2f>(
		    sf::Vector2i{ std::abs(uvRect.width), std::abs(uvRect.height) }); 
	}

private:
	const bool smoothTexture;
	const std::string fileName;
	const sf::Vector2u frameCount;
	sf::Vector2u currentFrame;
	sf::Time elapsedTime;
	sf::IntRect uvRect;
	sf::Texture* texture;
	Quad vertices;

	friend class AnimationSystem;

private:
	template <std::size_t, typename> friend struct std::tuple_element;
	template <std::size_t N> decltype(auto) get();
};

class AnimationSystem : public System {

public:
	void init() override {
		initFrom<Animation>();
		initFrom<Mesh>();
	}

	void add(Component* c) override;

	void update() override;

	void render(sf::RenderTarget& target) override;

};

