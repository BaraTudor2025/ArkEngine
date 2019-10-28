#pragma once

#include <variant>

#include "Component.hpp"
#include "System.hpp"
#include "Transform.hpp"
#include "Quad.hpp"
#include "Util.hpp"
#include "ResourceManager.hpp"


struct Mesh : public Component<Mesh> {

	Mesh() = default;

	Mesh(std::string fileName, bool smoothTexture = false)
		:fileName(fileName), smoothTexture(smoothTexture), uvRect(0,0,0,0), repeatTexture(false) 
	{
		setTexture(fileName);
	}

	Mesh(std::string fileName, bool smoothTexture, sf::IntRect rect, bool repeatTexture)
		:fileName(fileName),  smoothTexture(smoothTexture), uvRect(uvRect), repeatTexture(repeatTexture) 
	{
		setTexture(fileName);
	}

	sf::Vector2f meshSize() { 
	    return static_cast<sf::Vector2f>(
		    sf::Vector2i{ std::abs(uvRect.width), std::abs(uvRect.height) }); 
	}

	void setTexture(std::string name)
	{
		fileName = name;
		texture = Resources::load<sf::Texture>(name);
	}

private:
	bool repeatTexture = true;
	bool smoothTexture = false;
	//const bool flipX;
	//const bool flipY;
	std::string fileName = "";
	sf::IntRect uvRect;
	sf::Texture* texture = nullptr;
	Quad vertices;
	friend class MeshSystem;
};

struct Animation : public Component<Animation> {

	Animation() = default;
	// frameCount: number of frames of every row and number of rows; example: sf::Vector2u{6, 2} means 6 frames and 2 rows
	Animation(std::string fileName, sf::Vector2u frameCount, sf::Time frameTime, int row, bool smoothTexture)
		: fileName(fileName), frameCount(frameCount), frameTime(frameTime), row(row), smoothTexture(smoothTexture) 
	{
		setTexture(fileName);
	}

	// frameCount: numbers of frames of each row
	Animation(std::string fileName, std::initializer_list<uint32_t> frameCount, sf::Time frameTime, int row, bool smoothTexture)
		: fileName(fileName), 
		frameCount(std::pair{ *std::max_element(frameCount.begin(), frameCount.end()), frameCount }), 
		frameTime(frameTime), row(row), smoothTexture(smoothTexture)
	{
		setTexture(fileName);
	}

	Animation& operator=(const Animation& a) = default;

	sf::Time frameTime = sf::Time::Zero;
	int row = 0;
	bool flipX = false;
	bool flipY = false;

	void setTexture(std::string name)
	{
		fileName = name;
		auto visitor = [](const std::pair<uint32_t, std::vector<uint32_t>>& frameCounts) {
			return sf::Vector2u{ frameCounts.first, (uint32_t)frameCounts.second.size() }; // size means the number of columns
		}; 
		auto[fcX, fcY] = std::visit(overloaded{
			visitor,
			[](sf::Vector2u frameCount) { return frameCount; }
		}, frameCount);

		texture = Resources::load<sf::Texture>(fileName);
		uvRect.width = texture->getSize().x / (float)fcX;
		uvRect.height = texture->getSize().y / (float)fcY;
	}

	std::string_view getTexture() {
		return fileName;
	}

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

	bool smoothTexture = true;
	std::string fileName;
	std::variant<const sf::Vector2u, 
		std::pair<uint32_t, std::vector<uint32_t>>> frameCount;
	sf::Vector2u currentFrame;
	sf::Time elapsedTime;
	sf::IntRect uvRect;
	sf::Texture* texture = nullptr;
	Quad vertices;

	friend class AnimationSystem;

private:
	template <std::size_t, typename> friend struct std::tuple_element;
	template <std::size_t N> decltype(auto) get();
};

class AnimationSystem : public System, public Renderer {

public:
	AnimationSystem() : System(typeid(AnimationSystem)) { }

	void init() override
	{
		requireComponent<Transform>();
		requireComponent<Animation>();
	}

	void update() override;

	void render(sf::RenderTarget& target) override;
};

class MeshSystem : public System, public Renderer {
public:
	MeshSystem() : System(typeid(MeshSystem)) { }

	void init() override
	{
		requireComponent<Transform>();
		requireComponent<Mesh>();
	}

	void onEntityAdded(Entity) override;

	void render(sf::RenderTarget& target) override;
};
