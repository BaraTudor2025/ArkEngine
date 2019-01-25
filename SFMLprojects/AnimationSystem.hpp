#pragma once
#include "VectorEngine.hpp"

struct Animation : public Data<Animation> {

public:
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
	sf::VertexArray vertices;

	friend class AnimationSystem;

private:
	template <std::size_t, typename> friend struct std::tuple_element;
	template <std::size_t N> decltype(auto) get();
};

class AnimationSystem : public System {

public:
	void init() override {
		initFrom<Animation>();
	}

	void add(Component* c) override;

	void update() override;

	virtual void render(sf::RenderTarget& target) override;

};

