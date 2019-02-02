#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <vector>
#include "RandomNumbers.hpp"
#include "VectorEngine.hpp"
#include "Quad.hpp"

static inline constexpr auto PI = 3.14159f;

struct PointParticles final : public Component<PointParticles> {

	COPYABLE(PointParticles)

	PointParticles(int count, sf::Time lifeTime,
	          Distribution<float> speedArgs = { 0, 0 },
	          Distribution<float> angleArgs = { 0.f, 2 * 3.14159f },
	          DistributionType lifeTimeDistType = DistributionType::uniform,
	          std::function<sf::Color()> getColor = []() { return sf::Color::White; })
	    : count(count),
	    lifeTime(lifeTime),
	    speedDistribution(speedArgs),
	    angleDistribution(angleArgs),
	    getColor(getColor)
	{ 
		lifeTimeDistribution.type = lifeTimeDistType;
		this->makeLifeTimeDist();
	}

	size_t count;
	sf::Time lifeTime;

	Distribution<float> speedDistribution;
	Distribution<float> angleDistribution;

	sf::Vector2f emitter{ 0.f, 0.f };
	std::function<sf::Color()> getColor;

	bool spawn = false;
	bool fireworks = false;
	bool applyTransform = false;

	// call if lifeTime is modified
	void makeLifeTimeDist() noexcept {
		if (lifeTimeDistribution.type == DistributionType::uniform)
			this->makeLifeTimeDistUniform();
		else
			this->makeLifeTimeDistNormal();
	}
	void makeLifeTimeDistUniform(float divLowerBound = 4) noexcept { 
		lifeTimeDistribution = {
			lifeTime.asMilliseconds() / divLowerBound,
			(float)lifeTime.asMilliseconds(),
			DistributionType::uniform };
	}
	void makeLifeTimeDistNormal() noexcept {
		lifeTimeDistribution = {
			lifeTime.asMilliseconds() / 2.f,
			lifeTime.asMilliseconds() / 2.f,
			DistributionType::normal };
	}

private:
	struct InternalData {
		sf::Vector2f speed;
		sf::Time lifeTime = sf::Time::Zero;
	};
	std::vector<sf::Vertex>	vertices;
	std::vector<InternalData> data;
	sf::Time deathTimer = sf::Time::Zero;
	Distribution<float> lifeTimeDistribution;
	bool areDead() const { return deathTimer >= lifeTime; }
	friend class ParticleSystem;
};

struct PixelParticles : public Component<PixelParticles> {

	using Colors = std::pair<sf::Color, sf::Color>;

	COPYABLE(PixelParticles)

	PixelParticles(size_t count, sf::Time lifeTime, sf::Vector2f size, Colors colors)
		:count(count), particlesPerSecond(count), size(size), colors(colors), lifeTime(lifeTime) { }

	size_t count;
	float particlesPerSecond;
	sf::Vector2f size;
	Colors colors;
	sf::Vector2f emitter;
	float speed;
	sf::Time lifeTime;
	Distribution<float> angleDistribution = { 0, 2 * PI, DistributionType::normal };
	bool spawn = false;
	sf::Vector2f gravity;
	sf::FloatRect platform; // particles can't go through this

private:
	struct InternalData {
		sf::Vector2f speed;
		sf::Time lifeTime = sf::Time::Zero;
	};
	float particlesToSpawn = 0; // internal counter of particles to spawn per second
	int spawnBeingPos = 0;
	std::vector<Quad> quads;
	std::vector<InternalData> data;
	sf::Time deathTimer = sf::Time::Zero;
	bool areDead() const { return deathTimer >= lifeTime; }
	friend class ParticleSystem;
};

// templates

static auto makeRed = []() {
	auto green = RandomNumber<uint32_t>(0, 200);
	return sf::Color(255, green, 0);
};

static auto makeGreen = []() {
	auto red = RandomNumber<uint32_t>(0, 200);
	return sf::Color(red, 255, 0);
};

static auto makeBrightBlue = []() {
	auto green = RandomNumber<uint32_t>(100, 255);
	return sf::Color(0, green, 255);
};

static auto makeDarkBlue = []() {
	auto green = RandomNumber<uint32_t>(20, 150);
	return sf::Color(10, green, 240);
};

static auto makeColor = []() {
	return sf::Color(RandomNumber<uint32_t>(0x000000ff, 0xffffffff));
};

static auto makeBlue = []() {
	auto green = RandomNumber<uint32_t>(20, 250);
	return sf::Color(10, green, 240);
};

static inline std::vector<std::function<sf::Color()>> makeColorsVector{ makeRed, makeGreen, makeBlue, makeColor };

inline PointParticles getFireParticles(int count=1000) 
{
	PointParticles fireParticles = { count, sf::seconds(3), { 1, 100 } };
	fireParticles.getColor = makeRed;
	return fireParticles;
}

inline PointParticles getRainbowParticles()
{
	PointParticles rainbowParticles(2000, sf::seconds(3), { 1, 100 , DistributionType::normal});
	rainbowParticles.getColor = makeColor;
	return rainbowParticles;
}

inline PointParticles getGreenParticles()
{
	auto greenParticles = getFireParticles();
	greenParticles.getColor = makeGreen;
	return greenParticles;
}

class ParticleSystem final : public System {

public:
	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

private:
	void init() override;

	void update() override;
	void fixedUpdate(sf::Time) override;
	void render(sf::RenderTarget& target) override;

	void updatePointBatch(PointParticles&);
	void updatePixelBatch(PixelParticles&);
	void respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime);
	void respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime);

private:
};
