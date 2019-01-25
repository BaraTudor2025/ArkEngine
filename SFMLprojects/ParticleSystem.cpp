#include <iostream>
#include "ParticleSystem.hpp"
#include "Util.hpp"
#include "ResourceManager.hpp"

void ParticleSystem::update()
{
	forEachSpan(&ParticleSystem::updateBatch, this, this->getComponents<Particles>(), this->vertices, this->data);
}

void ParticleSystem::fixedUpdate(sf::Time dt)
{
	for (auto p : this->getComponents<Particles>()) {
		if (p->spawn) {
			p->deathTimer = sf::Time::Zero;
		} else {
			if (!p->areDead())
				p->deathTimer += dt;
		}
	}
}

void ParticleSystem::add(Component* data)
{
	if (auto ps = static_cast<Particles*>(data); ps) {
		auto addSize = [&](auto& range) { range.resize(range.size() + ps->count); };
		if (ps->spawn)
			ps->deathTimer = sf::Time::Zero;
		else
			ps->deathTimer = ps->lifeTime;
		addSize(this->data);
		addSize(this->vertices);
	}
}

void ParticleSystem::remove(Component* data)
{
	if (auto ps = static_cast<Particles*>(data); ps) {
		auto removeSize = [&](auto& range) { range.resize(range.size() - ps->count); };
		removeSize(this->vertices);
		removeSize(this->data);
	}
}

void ParticleSystem::render(sf::RenderTarget& target)
{
	auto draw = [&](Particles& ps, gsl::span<sf::Vertex> v) {
		if (ps.areDead())
			return;
		if (ps.applyTransform) {
			sf::RenderStates rs;
			rs.transform = *ps.entity()->getComponent<Transform>();
			target.draw(v.data(), v.size(), sf::Points, rs);
		}
		else {
			target.draw(v.data(), v.size(), sf::Points);
		}
	};

	forEachSpan(draw, this->getComponents<Particles>(), this->vertices);
}

void respawnParticle(const Particles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime);

void ParticleSystem::updateBatch(
	Particles& ps, 
	gsl::span<sf::Vertex> vertices, 
	gsl::span<Data> data)
{
	if (ps.areDead())
		return;

	auto deltaTime = VectorEngine::deltaTime();
	auto dt = deltaTime.asSeconds();
	for (int i = 0; i < vertices.size(); i++) {
		data[i].lifeTime -= deltaTime;
		if (data[i].lifeTime > sf::Time::Zero) { // if alive

			if (hasUniversalGravity)
				data[i].speed += gravityVector * dt;
			else {
				auto r = gravityPoint - vertices[i].position;
				auto dist = std::hypot(r.x, r.y);
				auto g = r / (dist * dist);
				data[i].speed += gravityMagnitude * 1000.f * g * dt;
			}
			vertices[i].position += data[i].speed * dt;

			float ratio = data[i].lifeTime.asSeconds() / ps.lifeTime.asSeconds();
			vertices[i].color.a = static_cast<uint8_t>(ratio * 255);
		} else if (ps.spawn)
			respawnParticle(ps, vertices[i], data[i].speed, data[i].lifeTime);
	}
}

void respawnParticle(const Particles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime)
{
	float angle = RandomNumber(ps.angleDistribution);
	float speedMag = RandomNumber(ps.speedDistribution);

	vertex.position = ps.emitter;
	vertex.color = ps.getColor();
	speed = sf::Vector2f(std::cos(angle) * speedMag, std::sin(angle) * speedMag);

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
