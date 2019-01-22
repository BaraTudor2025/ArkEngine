#pragma once
#include "VectorEngine.hpp"

struct Animation : public Data<Animation> {

public:
	Animation(std::string fileName, sf::Vector2u frameCount, sf::Time frameTime, int state)
	: fileName(fileName), frameCount(frameCount), frameTime(frameTime), row(state)
	{ }

	int row;
	bool flipped = false;//maybe add flipX and flipY
	sf::Time frameTime;
	sf::Vector2f frameSize() { return static_cast<sf::Vector2f>(sf::Vector2i{ uvRect.width, uvRect.height }); }

private:
	const std::string fileName;
	sf::Vector2u currentFrame;
	const sf::Vector2u frameCount;
	sf::Time elapsedTime;
	sf::IntRect uvRect;
	sf::Texture* pTexture;
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

