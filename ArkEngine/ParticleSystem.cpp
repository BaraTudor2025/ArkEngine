#include <iostream>

#include "ParticleSystem.hpp"
#include "ark/util/Util.hpp"
#include "ark/core/Engine.hpp"


///////////////////////////////
//// POINT PARTICLE SYSTEM ////
///////////////////////////////

void PointParticleSystem::update()
{
	// disabling deathTimer
	/*
	for (auto entity : getEntities()) {
		auto& p = entity.getComponent<PointParticles>();
		if (p.spawn) {
			p.deathTimer = sf::Time::Zero;
		} else {
			if (!p.areDead())
				p.deathTimer += Engine::deltaTime();
		}
	}
	*/

	for (auto entity : getEntities()) {
		auto& ps = entity.getComponent<PointParticles>();
		//if (ps.areDead())
			//return;

		auto deltaTime = ark::Engine::deltaTime();
		auto dt = deltaTime.asSeconds();

		auto vert = ps.vertices.begin();
		auto data = ps.data.begin();
		for (; vert != ps.vertices.end() && data != ps.data.end(); ++vert, ++data) {
			data->lifeTime -= deltaTime;
			if (data->lifeTime > sf::Time::Zero) { // if alive

				if (hasUniversalGravity)
					data->speed += gravityVector * dt;
				else {
					auto r = gravityPoint - vert->position;
					auto dist = std::hypot(r.x, r.y);
					auto g = r / (dist * dist);
					data->speed += gravityMagnitude * 1000.f * g * dt;
				}
				vert->position += data->speed * dt;

				float ratio = data->lifeTime.asSeconds() / ps.lifeTime.asSeconds();
				vert->color.a = static_cast<uint8_t>(ratio * 255);
			} else if (ps.spawn)
				respawnPointParticle(ps, *vert, data->speed, data->lifeTime);
		}

		//for (int i = 0; i < ps.vertices.size(); i++) {
		//	ps.data[i].lifeTime -= deltaTime;
		//	if (ps.data[i].lifeTime > sf::Time::Zero) { // if alive

		//		if (hasUniversalGravity)
		//			ps.data[i].speed += gravityVector * dt;
		//		else {
		//			auto r = gravityPoint - ps.vertices[i].position;
		//			auto dist = std::hypot(r.x, r.y);
		//			auto g = r / (dist * dist);
		//			ps.data[i].speed += gravityMagnitude * 1000.f * g * dt;
		//		}
		//		ps.vertices[i].position += ps.data[i].speed * dt;

		//		float ratio = ps.data[i].lifeTime.asSeconds() / ps.lifeTime.asSeconds();
		//		ps.vertices[i].color.a = static_cast<uint8_t>(ratio * 255);
		//	} else if (ps.spawn)
		//		respawnPointParticle(ps, ps.vertices[i], ps.data[i].speed, ps.data[i].lifeTime);
		//}
	}
}

void PointParticleSystem::render(sf::RenderTarget& target)
{
	for (auto entity : getEntities()) {
		const auto& p = entity.getComponent<PointParticles>();
		//if (p.areDead())
			//return;
		target.draw(p.vertices.data(), p.vertices.size(), sf::Points);
	}
}

void PointParticleSystem::respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime)
{
	float angle = RandomNumber(ps.angleDistribution);
	float speedMag = RandomNumber(ps.speedDistribution);

	vertex.position = ps.emitter;
	vertex.color = ps.getColor();
	speed = Util::toCartesian({ speedMag, angle });

	if (ps.fireworks) {
		lifeTime = ps.lifeTime;
		return;
	}

	auto time = std::abs(RandomNumber(ps.lifeTimeDistribution));
	if (ps.lifeTimeDistribution.type == DistributionType::normal) {
		if (sf::seconds(time / 1000) > ps.lifeTime)
			lifeTime = ps.lifeTime;
	} else
		lifeTime = sf::milliseconds(static_cast<int>(time));
}


///////////////////////////////
//// PIXEL PARTICLE SYSTEM ////
///////////////////////////////

void PixelParticleSystem::update()
{
	/*
	for (auto entity : getEntities()) {
		auto& p = entity.getComponent<PixelParticles>();
		if (p.spawn) {
			p.deathTimer = sf::Time::Zero;
			p.particlesToSpawn += p.particlesPerSecond * Engine::deltaTime().asSeconds(); 
		} else {
			if (!p.areDead())
				p.deathTimer += Engine::deltaTime();
		}
	}
	*/
	
	for (auto entity : getEntities()) {
		auto& p = entity.getComponent<PixelParticles>();
		if (p.spawn) {
			p.particlesToSpawn += p.particlesPerSecond * ark::Engine::deltaTime().asSeconds(); 
		}
	}

	for (auto entity : getEntities()) {
		auto& ps = entity.getComponent<PixelParticles>();
		//if (ps.areDead())
			//return;

		auto deltaTime = ark::Engine::deltaTime();
		auto dt = deltaTime.asSeconds();
		int particleNum = std::floor(ps.particlesToSpawn);
		if (particleNum >= 1)
			ps.particlesToSpawn -= particleNum;

		for (int i = 0; i < ps.quads.size(); i++) {
			ps.data[i].lifeTime -= deltaTime;
			if (ps.data[i].lifeTime > sf::Time::Zero) {
				if (ps.platform.intersects(ps.quads[i].getGlobalRect()))
					continue;
				ps.data[i].speed += ps.gravity * dt;
				ps.quads[i].move(ps.data[i].speed * dt);
			} else
				ps.quads[i].setAlpha(0);
		}

		auto process = [&](int begin, int end) {
			for (int i = begin; i < end; i++)
				respawnPixelParticle(ps, ps.quads[i], ps.data[i].speed, ps.data[i].lifeTime);
		};

		if (particleNum != 0 && ps.spawn)
			if (ps.spawnBeingPos + particleNum > ps.count) {
				process(ps.spawnBeingPos, ps.count);
				auto endPos = particleNum - (ps.count - ps.spawnBeingPos);
				process(0, endPos);
				ps.spawnBeingPos = endPos;
			} else {
				process(ps.spawnBeingPos, ps.spawnBeingPos + particleNum);
				ps.spawnBeingPos += particleNum;
			}
	}
}

void PixelParticleSystem::render(sf::RenderTarget& target)
{
	for (auto entity : getEntities()) {
		auto& p = entity.getComponent<PixelParticles>();
		//if (p.areDead())
			//return;
		for (auto& q : p.quads) {
			target.draw(q.data(), 4, sf::TriangleStrip);
		}
	}
}

void PixelParticleSystem::respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime)
{
	quad.setAlpha(ps.colors.first.a);
	float angle = RandomNumber(ps.angleDistribution);
	float speedMag = RandomNumber(ps.speed / 2, ps.speed);
	speed = Util::toCartesian({ speedMag, angle });

	auto center = ps.emitter - ps.size / 2.f;
	quad.updatePosition({center, ps.size});

	auto milis = ps.lifeTime.asMilliseconds();
	auto time = RandomNumber<int>(milis/10, milis); 
	lifeTime = sf::milliseconds(time);
}

