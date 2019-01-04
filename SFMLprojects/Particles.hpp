#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <functional>
#include "RandomNumbers.hpp"
#include "gsl.hpp"
#include "Ecs.hpp"

static inline constexpr auto PI = 3.14f;

struct Particles final : public Data {

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
			lifeTimeDistribution = { { 0, lifeTime.asMilliseconds() }, ltd };
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

	sf::Color(*getColor)() = []() { return sf::Color::White; };

private:
	void addThisToSystem() override;

};

class ParticleSystem final : public System, public NonCopyable, public NonMovable {
public:

	static inline sf::Vector2f gravityVector{0.f, 0.f};

	static ParticleSystem instance;

	void update(sf::Time deltaTime, sf::Vector2f mousePos) override;

	void add(Data*) override;

	void draw(sf::RenderWindow& target) override;

private:

	void updateBatch(sf::Time, const Particles&, gsl::span<sf::Vertex>, gsl::span<sf::Vector2f>, gsl::span<sf::Time>);

	void respawnParticle(const Particles&, sf::Vertex&, sf::Vector2f&, sf::Time&);

private:
	std::vector<sf::Vertex>	vertices;
	std::vector<sf::Vector2f> velocities;
	std::vector<sf::Time> lifeTimes;

	std::vector<Particles*> particlesInfos;
};
