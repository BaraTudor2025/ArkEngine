#include "Particles.hpp"
#include <iostream>

ParticleSystem ParticleSystem::instance;

void ParticleSystem::update(sf::Time deltaTime)
{
	size_t pos = 0;
	for (Particles* p : this->particlesData) {
		this->updateBatch(
			deltaTime, *p,
			{ this->vertices.data() + pos, p->count },
			{ this->velocities.data() + pos, p->count },
			{ this->lifeTimes.data() + pos, p->count });
		pos += p->count;
	}
}

void ParticleSystem::add(Component* data)
{
	auto ps = dynamic_cast<Particles*>(data);
	auto addSize = [&](auto& range) { range.resize(range.size() + ps->count); };
	addSize(this->velocities);
	addSize(this->vertices);
	addSize(this->lifeTimes);
	this->particlesData.push_back(ps);
}

void ParticleSystem::remove(Component* c)
{
	auto p = dynamic_cast<Particles*>(c);
	erase(this->particlesData, p);
}

inline void ParticleSystem::render(sf::RenderWindow& target)
{
	target.draw(this->vertices.data(), this->vertices.size(), sf::Points);
}

void ParticleSystem::updateBatch(
	sf::Time deltaTime, 
	const Particles& ps, 
	gsl::span<sf::Vertex> vertices, 
	gsl::span<sf::Vector2f> velocities, 
	gsl::span<sf::Time> lifeTimes)
{
	auto dt = deltaTime.asSeconds();
	for (int i = 0; i < vertices.size(); i++) {
		lifeTimes[i] -= deltaTime;
		if (lifeTimes[i] > sf::Time::Zero) { // if alive

			velocities[i] += gravityVector * dt;
			vertices[i].position += velocities[i] * dt;

			float ratio = lifeTimes[i].asSeconds() / ps.lifeTime.asSeconds();
			vertices[i].color.a = static_cast<uint8_t>(ratio * 255);
		} else if (ps.spawn)
			this->respawnParticle(ps, vertices[i], velocities[i], lifeTimes[i]);
	}
}

void ParticleSystem::respawnParticle(const Particles& ps, sf::Vertex& vertex, sf::Vector2f& velocity, sf::Time& lifeTime)
{
	float angle = RandomNumber(ps.angleDistribution);
	float speed = RandomNumber(ps.speedDistribution);

	vertex.position = ps.emitter;
	vertex.color = ps.getColor();
	velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);

	if (ps.fireworks) {
		lifeTime = ps.lifeTime;
		return;
	}

	auto time = std::abs(RandomNumber(ps.lifeTimeDistribution));
	if (ps.lifeTimeDistribution.type == DistributionType::normal) {
		while (sf::seconds(time) > ps.lifeTime)
			time = RandomNumber(ps.lifeTimeDistribution);
		lifeTime = sf::seconds(time);
	} else
		lifeTime = sf::milliseconds(static_cast<int>(time));
}
