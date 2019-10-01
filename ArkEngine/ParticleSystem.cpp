
#include "ParticleSystem.hpp"
#include "Util.hpp"
#include "Engine.hpp"

#include <iostream>

#if 0
void ParticleSystem::init()
{
	forEach<PointParticles>([](auto& p) { 
		p.vertices.resize(p.count);
		p.data.resize(p.count);
		if (p.spawn)
			p.deathTimer = sf::Time::Zero;
		else
			p.deathTimer = p.lifeTime;
	});

	forEach<PixelParticles>([](auto& p) {
		p.quads.resize(p.count);
		p.data.resize(p.count);
		for (auto& q : p.quads)
			q.setColors(p.colors.first, p.colors.second);
		if (p.spawn)
			p.deathTimer = sf::Time::Zero;
		else
			p.deathTimer = p.lifeTime;
	});
}

void ParticleSystem::render(sf::RenderTarget& target)
{
	forEach<PointParticles>([&](auto& p) {
		if (p.areDead())
			return;
		if (p.applyTransform) {
			sf::RenderStates rs;
			rs.transform = *p.entity()->getComponent<Transform>();
			target.draw(p.vertices.data(), p.vertices.size(), sf::Points, rs);
		}
		else {
			target.draw(p.vertices.data(), p.vertices.size(), sf::Points);
		}
	});

	forEach<PixelParticles>([&](auto& p) {
		if (p.areDead())
			return;
		for (auto& q : p.quads) {
			target.draw(q.data(), 4, sf::TriangleStrip);
		}
	});
}

void ParticleSystem::update()
{
	forEach<PointParticles>([this](auto& p) { updatePointBatch(p); });
	forEach<PixelParticles>([this](auto& p) { updatePixelBatch(p); });
}

void ParticleSystem::fixedUpdate()
{
	auto processDeathTime = [](auto& p) {
		if (p.spawn) {
			p.deathTimer = sf::Time::Zero;
		} else {
			if (!p.areDead())
				p.deathTimer += ArkEngine::fixedTime();
		}
	};
	forEach<PointParticles>(processDeathTime);
	forEach<PixelParticles>(processDeathTime);
	forEach<PixelParticles>([&](auto& pixels) {
		if (pixels.spawn)
			pixels.particlesToSpawn += pixels.particlesPerSecond * ArkEngine::fixedTime().asSeconds(); 
	});
}

inline void ParticleSystem::respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime)
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
		while (sf::seconds(time) > ps.lifeTime)
			time = RandomNumber(ps.lifeTimeDistribution);
		lifeTime = sf::seconds(time);
	} else
		lifeTime = sf::milliseconds(static_cast<int>(time));
}

void ParticleSystem::updatePointBatch(PointParticles& ps)
{
	if (ps.areDead())
		return;

	auto deltaTime = ArkEngine::deltaTime();
	auto dt = deltaTime.asSeconds();

	auto vert = ps.vertices.begin();
	auto data = ps.data.begin();
	for (; vert != ps.vertices.end() && data != ps.data.end(); ++vert, ++data)
	{
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

inline void ParticleSystem::respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime)
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

void ParticleSystem::updatePixelBatch(PixelParticles& ps)
{
	if (ps.areDead())
		return;

	auto deltaTime = ArkEngine::deltaTime();
	auto dt = deltaTime.asSeconds();
	int particleNum = std::floor(ps.particlesToSpawn);
	if(particleNum >= 1)
		ps.particlesToSpawn -= particleNum;

	for (int i = 0; i < ps.quads.size(); i++) {
		ps.data[i].lifeTime -= deltaTime;
		if (ps.data[i].lifeTime > sf::Time::Zero) {
			if (ps.platform.intersects(ps.quads[i].getGlobalRect())) 
				continue;
			ps.data[i].speed += ps.gravity * dt;
			ps.quads[i].move(ps.data[i].speed * dt);
		} 
		else
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
#endif



///////////////////////////////
//// POINT PARTICLE SYSTEM ////
///////////////////////////////

void PointParticleSystem::onEntityAdded(Entity entity)
{
	auto& p = entity.getComponent<PointParticles>();
	p.vertices.resize(p.count);
	p.data.resize(p.count);
	if (p.spawn)
		p.deathTimer = sf::Time::Zero;
	else
		p.deathTimer = p.lifeTime;
}

void PointParticleSystem::update()
{
	for (auto entity : getEntities()) {
		auto& ps = entity.getComponent<PointParticles>();
		//if (ps.areDead())
			//return;

		auto deltaTime = ArkEngine::deltaTime();
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

void PointParticleSystem::fixedUpdate()
{
	for (auto entity : getEntities()) {
		auto& p = entity.getComponent<PointParticles>();
		//std::cout << "fix update\n";
		if (p.spawn) {
			//std::cout << "spawn\n";
			p.deathTimer = sf::Time::Zero;
		} else {
			if (!p.areDead())
				p.deathTimer += ArkEngine::fixedTime();
		}
	}
}

void PointParticleSystem::render(sf::RenderTarget& target)
{
	//std::cout << "size: " << getEntities().size() << std::endl;
	for (auto entity : getEntities()) {

		auto& p = entity.getComponent<PointParticles>();
		//if (p.areDead()) {
			//std::cout << "dead";
			//return;
		//}
		//if (p.applyTransform) {
		//	sf::RenderStates rs;
		//	rs.transform = *p.entity()->getComponent<Transform>();
		//	target.draw(p.vertices.data(), p.vertices.size(), sf::Points, rs);
		//}
		//else {
		//std::cout << p.vertices.size() << "\n";
		target.draw(p.vertices.data(), p.vertices.size(), sf::Points);
		//}
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
		while (sf::seconds(time) > ps.lifeTime)
			time = RandomNumber(ps.lifeTimeDistribution);
		lifeTime = sf::seconds(time);
	} else
		lifeTime = sf::milliseconds(static_cast<int>(time));
}


///////////////////////////////
//// PIXEL PARTICLE SYSTEM ////
///////////////////////////////

void PixelParticleSystem::onEntityAdded(Entity entity)
{
	auto& p = entity.getComponent<PixelParticles>();
	p.quads.resize(p.count);
	p.data.resize(p.count);
	for (auto& q : p.quads)
		q.setColors(p.colors.first, p.colors.second);
	if (p.spawn)
		p.deathTimer = sf::Time::Zero;
	else
		p.deathTimer = p.lifeTime;
}

void PixelParticleSystem::update()
{
	for (auto entity : getEntities()) {
		auto& ps = entity.getComponent<PixelParticles>();
		//if (ps.areDead())
			//return;

		auto deltaTime = ArkEngine::deltaTime();
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

void PixelParticleSystem::fixedUpdate()
{
	for (auto entity : getEntities()) {
		auto& p = entity.getComponent<PixelParticles>();
		if (p.spawn) {
			p.deathTimer = sf::Time::Zero;
			p.particlesToSpawn += p.particlesPerSecond * ArkEngine::fixedTime().asSeconds(); 
		} else {
			if (!p.areDead())
				p.deathTimer += ArkEngine::fixedTime();
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

