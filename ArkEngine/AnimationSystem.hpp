#pragma once

#include <variant>

#include "Component.hpp"
#include "System.hpp"
#include "Transform.hpp"
#include "Quad.hpp"

struct Mesh : public Component {

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
	friend class MeshSystem;
};

struct Animation : public Component {

	// frameCount: number of frames of every row and number of rows; example: sf::Vector2u{6, 2} means 6 frames and 2 rows
	Animation(std::string fileName, sf::Vector2u frameCount, sf::Time frameTime, int row, bool smoothTexture)
		: fileName(fileName), frameCount(frameCount), frameTime(frameTime), row(row), smoothTexture(smoothTexture) { }

	// frameCount: numbers of frames of each row
	Animation(std::string fileName, std::initializer_list<uint32_t> frameCount, sf::Time frameTime, int row, bool smoothTexture)
		: fileName(fileName), 
		frameCount(std::pair{ *std::max_element(frameCount.begin(), frameCount.end()), frameCount }), 
		frameTime(frameTime), row(row), smoothTexture(smoothTexture)
	{
	}

	Animation& operator=(const Animation& a) = default;

	sf::Time frameTime;
	int row;
	bool flipX = false;
	bool flipY = false;

	// plays animation one time and then plays the normal animation
	//struct {
	//	int row;
	//	sf::Time frameTime;
	//} intermediary;

	sf::Vector2f frameSize() { 
	    return static_cast<sf::Vector2f>(
		    sf::Vector2i{ std::abs(uvRect.width), std::abs(uvRect.height) }); 
	}

private:

	const bool smoothTexture;
	const std::string fileName;
	std::variant<const sf::Vector2u, 
		std::pair<uint32_t, std::vector<uint32_t>>> frameCount;
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

class AnimationSystem : public System, public Renderer {

public:
	AnimationSystem() : System(typeid(AnimationSystem))
	{
		requireComponent<Transform>();
		requireComponent<Animation>();
	}

	void onEntityAdded(Entity) override;

	void update() override;

	void render(sf::RenderTarget& target) override;
};

class MeshSystem : public System, public Renderer {
public:
	MeshSystem() : System(typeid(MeshSystem))
	{
		requireComponent<Transform>();
		requireComponent<Mesh>();
	}

	void onEntityAdded(Entity) override;

	void render(sf::RenderTarget& target) override;
};
