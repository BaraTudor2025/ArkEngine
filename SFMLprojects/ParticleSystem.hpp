#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <functional>
#include "RandomNumbers.hpp"
#include "gsl.hpp"
#include "VectorEngine.hpp"

static inline constexpr auto PI = 3.14159f;

struct Particles final : public Data<Particles> {

	COPYABLE(Particles)

	Particles(int count, sf::Time lifeTime,
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
	Distribution<float> lifeTimeDistribution;

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
	sf::Time deathTimer = sf::Time::Zero;
	bool isDead() { return deathTimer >= lifeTime; }
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

inline Particles getFireParticles() 
{
	Particles fireParticles = { 1000, sf::seconds(3), { 1, 100 } };
	fireParticles.getColor = makeRed;
	return fireParticles;
}

inline Particles getRainbowParticles()
{
	Particles rainbowParticles(2000, sf::seconds(3), { 1, 100 , DistributionType::normal});
	rainbowParticles.getColor = makeColor;
	return rainbowParticles;
}

inline Particles getGreenParticles()
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
	void init() override {
		this->initFrom<Particles>();
	}

	void update() override;
	void fixedUpdate(sf::Time) override;
	void add(Component*) override;
	void remove(Component*) override;
	void render(sf::RenderTarget& target) override;

	struct Data {
		sf::Vector2f speed;
		sf::Time lifeTime;
	};

	void updateBatch(Particles&, gsl::span<sf::Vertex>, gsl::span<Data>);
	void respawnParticle(const Particles&, sf::Vertex&, sf::Vector2f&, sf::Time&);

private:
	// benchmark-ul spune ca memory layout-ul asta nu ajuta, are acelasi fps
	std::vector<sf::Vertex>	vertices;
	std::vector<Data> data;
};
