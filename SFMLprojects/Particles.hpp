#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <functional>
#include "RandomNumbers.hpp"
#include "gsl.hpp"
#include "VectorEngine.hpp"

static inline constexpr auto PI = 3.14f;

struct Particles final : public Data<Particles> {

	COPYABLE(Particles)

	Particles(size_t count, sf::Time lifeTime,
			 std::pair<float, float> speedArgs,
			 std::pair<float, float> angleArgs = { 0.f, 2 * PI },
			 DistributionType ltd = DistributionType::normal)
		: count(count),
		lifeTime(lifeTime),
		speedDistribution{ speedArgs, DistributionType::normal },
		angleDistribution{ angleArgs, DistributionType::uniform }
	{
		// daca e dist::uniform atunci prima emitere are efect de artificii
		if (ltd == DistributionType::uniform)
			lifeTimeDistribution = { { lifeTime.asMilliseconds() / 4, lifeTime.asMilliseconds() }, ltd };
		else
			lifeTimeDistribution = { { lifeTime.asMilliseconds() / 2,  lifeTime.asMilliseconds() / 2 }, ltd };
	}

	size_t count;
	sf::Time lifeTime;

	Distribution<float> angleDistribution;
	Distribution<float> speedDistribution;
	Distribution<float> lifeTimeDistribution;

	sf::Vector2f emitter;
	bool spawn = false;
	bool fireworks = false;

	std::function<sf::Color()> getColor = [](){ return sf::Color::White; };

	friend class ParticleSystem;
};

// templates

inline Particles getFireParticles() 
{
	Particles fireParticles(1'000, sf::seconds(2), { 2, 100 }, { 0, 2 * PI }, DistributionType::uniform);
	fireParticles.speedDistribution.type = DistributionType::uniform;
	fireParticles.getColor = []() {
		auto green = RandomNumber<uint32_t>(0, 150);
		return sf::Color(255, green, 0);
	}; 
	return fireParticles;
}

inline Particles getWhiteParticles(int count = 1000, float life = 6)
{
	Particles whiteParticles(count, sf::seconds(life), { 0, 2 });
	whiteParticles.speedDistribution.type = DistributionType::uniform;
	whiteParticles.angleDistribution.type = DistributionType::uniform;

	return whiteParticles;
}

inline Particles getRainbowParticles()
{
	Particles rainbowParticles(2000, sf::seconds(3), { 0, 100 }, { 0,2 * PI }, DistributionType::uniform);
	rainbowParticles.getColor = []() {
		return sf::Color(RandomNumber<uint32_t>(0x000000ff, 0xffffffff));
	};
	return rainbowParticles;
}

inline Particles getGreenParticles()
{
	auto greenParticles = getFireParticles();
	greenParticles.getColor = []() {
		auto red = RandomNumber<uint32_t>(0, 150);
		return sf::Color(red, 255, 0);
	};
	return greenParticles;
}

class ParticleSystem final : public System {

public:
	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	static ParticleSystem instance;

	void init() override {
		initWith<Particles>();
	}

	void update() override;

	void add(Component*) override;

	void remove(Component*) override;

	void render(sf::RenderWindow& target) override;

private:
	template<typename F, typename...Spans>
	void forEach(F f, Spans&...);

	void updateBatch(const Particles&, gsl::span<sf::Vertex>, gsl::span<sf::Vector2f>, gsl::span<sf::Time>);

	void respawnParticle(const Particles&, sf::Vertex&, sf::Vector2f&, sf::Time&);

private:
	std::vector<sf::Vertex>	vertices;
	std::vector<sf::Vector2f> velocities;
	std::vector<sf::Time> lifeTimes;

	std::vector<Particles*> particlesData;
};
