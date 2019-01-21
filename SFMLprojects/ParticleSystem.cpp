#include "ParticleSystem.hpp"
#include "Util.hpp"
#include <iostream>

void ParticleSystem::update()
{
	forEachSpan(&ParticleSystem::updateBatch, this, this->getComponents<Particles>(), this->vertices, this->velocities, this->lifeTimes);
}

void ParticleSystem::add(Component* data)
{
	if (auto ps = static_cast<Particles*>(data); ps) {
		auto addSize = [&](auto& range) { range.resize(range.size() + ps->count); };
		addSize(this->velocities);
		addSize(this->vertices);
		addSize(this->lifeTimes);
	}
	//	static int cnt = 0;
	//	std::cout << "add " << ++cnt << '\n';
	//} else {
	//	static int cnt = 0;
	//	std::cout << "add nothing " << ++cnt << '\n';
	//}
}

void ParticleSystem::remove(Component* data)
{
	if (auto ps = static_cast<Particles*>(data); ps) {
		auto removeSize = [&](auto& range) { range.resize(range.size() - ps->count); };
		removeSize(this->velocities);
		removeSize(this->vertices);
		removeSize(this->lifeTimes);
	}
	//	static int cnt = 0;
	//	std::cout << "remove " << ++cnt << '\n';
	//} else {
	//	static int cnt = 0;
	//	std::cout << "remove nothing " << ++cnt << '\n';
	//}
}

void ParticleSystem::render(sf::RenderTarget& target)
{
	auto draw = [&](Particles& ps, gsl::span<sf::Vertex> v) {
		//if(ps.transform)
		//	target.draw(v.data(), v.size(), sf::Points, ps.transform->getTransform());
		//	//target.draw(v.data(), v.size(), sf::Points );
		//else
			target.draw(v.data(), v.size(), sf::Points);
	};

	forEachSpan(draw, this->getComponents<Particles>(), this->vertices);
}

void ParticleSystem::updateBatch(
	const Particles& ps, 
	gsl::span<sf::Vertex> vertices, 
	gsl::span<sf::Vector2f> velocities, 
	gsl::span<sf::Time> lifeTimes)
{
	auto deltaTime = VectorEngine::deltaTime();
	auto dt = deltaTime.asSeconds();
	for (int i = 0; i < vertices.size(); i++) {
		lifeTimes[i] -= deltaTime;
		if (lifeTimes[i] > sf::Time::Zero) { // if alive

			if (hasUniversalGravity)
				velocities[i] += gravityVector * dt;
			else {
				auto r = gravityPoint - vertices[i].position;
				auto dist = std::hypot(r.x, r.y);
				auto g = r / (dist * dist);
				velocities[i] += gravityMagnitude * 1000.f * g * dt;
			}
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
