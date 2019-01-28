#include <iostream>
#include "ParticleSystem.hpp"
#include "Util.hpp"
#include "ResourceManager.hpp"

void ParticleSystem::init()
{
	this->initFrom<PointParticles>();
	this->initFrom<PixelParticles>();
	auto initPixelP = [](PixelParticles& p, gsl::span<Quad> quads) {
		for (auto& q : quads) {
			//q.updatePosition({ {0,0}, p.size });
			q.setColors(p.colors.first, p.colors.second);
		}
	};
	forEachSpan(initPixelP, this->getComponents<PixelParticles>(), this->pixelQuads);
}

void ParticleSystem::add(Component* data)
{
	auto addSize = [](size_t count, auto&... ranges) { (ranges.resize(ranges.size() + count) , ...); };
	if (auto p = dynamic_cast<PointParticles*>(data); p) {
		addSize(p->count, this->pointVertices, this->pointParticles);
		if (p->spawn)
			p->deathTimer = sf::Time::Zero;
		else
			p->deathTimer = p->lifeTime;
	} 
	if (auto p = dynamic_cast<PixelParticles*>(data); p) {
		addSize(p->count, this->pixelQuads, this->pixelParticles);
		if (p->spawn)
			p->deathTimer = sf::Time::Zero;
		else
			p->deathTimer = p->lifeTime;
	}
}

void ParticleSystem::remove(Component* data)
{
	auto removeSize = [](size_t count, auto&... ranges) { (ranges.resize(ranges.size() - count), ...); };
	if (auto p = dynamic_cast<PointParticles*>(data); p) {
		removeSize(p->count, this->pointVertices, this->pointParticles);
	}
	if (auto p = dynamic_cast<PixelParticles*>(data); p) {
		// ?
		removeSize(p->count, this->pixelQuads, this->pixelParticles);
	}
}

void ParticleSystem::render(sf::RenderTarget& target)
{
	auto drawPoints = [&](PointParticles& ps, gsl::span<sf::Vertex> v) {
		if (ps.areDead())
			return;
		//if (ps.applyTransformOnEmitter) {
			//auto t = ps.entity()->getComponent<Transform>();
			//ps.emitter = t->getTransform().transformPoint(ps.emitter);
		//}
		if (ps.applyTransform) {
			sf::RenderStates rs;
			rs.transform = *ps.entity()->getComponent<Transform>();
			target.draw(v.data(), v.size(), sf::Points, rs);
		}
		else {
			target.draw(v.data(), v.size(), sf::Points);
		}
	};

	forEachSpan(drawPoints, this->getComponents<PointParticles>(), this->pointVertices);

	//auto pixelParticlesBuff = reinterpret_cast<sf::Vertex*>(this->pixelQuads.data());
	//target.draw(pixelParticlesBuff, this->pixelQuads.size() * 4, sf::Quads);
	auto drawPixels = [&](PixelParticles& ps, gsl::span<Quad> quads) {
		if (ps.areDead())
			return;
		//if (auto t = ps.entity()->getComponent<Transform>(); t)
			//ps.emitter = t->getTransform().transformPoint(ps.emitter);
		//target.draw(quads.data()->data(), quads.size() * 4, sf::TriangleStrip);
		for (auto& q : quads) {
			target.draw(q.data(), 4, sf::TriangleStrip);
		}
	};
	forEachSpan(drawPixels, this->getComponents<PixelParticles>(), this->pixelQuads);
}

void ParticleSystem::update()
{
	forEachSpan(&ParticleSystem::updatePointBatch, this, this->getComponents<PointParticles>(), this->pointVertices, this->pointParticles);
	forEachSpan(&ParticleSystem::updatePixelBatch, this, this->getComponents<PixelParticles>(), this->pixelQuads, this->pixelParticles);
}

void ParticleSystem::fixedUpdate(sf::Time dt)
{
	auto processDeathTime = [dt](auto& ps) {
		for (auto& p : ps) {
			if (p->spawn) {
				p->deathTimer = sf::Time::Zero;
			} else {
				if (!p->areDead())
					p->deathTimer += dt;
			}
		}
	};
	processDeathTime(this->getComponents<PointParticles>());
	processDeathTime(this->getComponents<PixelParticles>());
}

inline void ParticleSystem::respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime)
{
	float angle = RandomNumber(ps.angleDistribution);
	float speedMag = RandomNumber(ps.speedDistribution);

	vertex.position = ps.emitter;
	vertex.color = ps.getColor();
	//speed = sf::Vector2f(std::cos(angle) * speedMag, std::sin(angle) * speedMag);
	speed = toCartesian({ speedMag, angle });

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

void ParticleSystem::updatePointBatch(const PointParticles& ps, gsl::span<sf::Vertex> vertices, gsl::span<InternalData> pointParticles)
{
	if (ps.areDead())
		return;

	auto deltaTime = VectorEngine::deltaTime();
	auto dt = deltaTime.asSeconds();
	for (int i = 0; i < vertices.size(); i++) {
		pointParticles[i].lifeTime -= deltaTime;
		if (pointParticles[i].lifeTime > sf::Time::Zero) { // if alive

			if (hasUniversalGravity)
				pointParticles[i].speed += gravityVector * dt;
			else {
				auto r = gravityPoint - vertices[i].position;
				auto dist = std::hypot(r.x, r.y);
				auto g = r / (dist * dist);
				pointParticles[i].speed += gravityMagnitude * 1000.f * g * dt;
			}
			vertices[i].position += pointParticles[i].speed * dt;

			float ratio = pointParticles[i].lifeTime.asSeconds() / ps.lifeTime.asSeconds();
			vertices[i].color.a = static_cast<uint8_t>(ratio * 255);
		} else if (ps.spawn)
			respawnPointParticle(ps, vertices[i], pointParticles[i].speed, pointParticles[i].lifeTime);
	}
}

inline void ParticleSystem::respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime)
{
	quad.setAlpha(ps.colors.first.a);
	float angle = RandomNumber(ps.angleDistribution);
	float speedMag = RandomNumber(ps.speed / 2, ps.speed);
	speed = toCartesian({ speedMag, angle });

	auto center = ps.emitter - ps.size / 2.f;
	quad.updatePosition({center, ps.size});

	auto milis = ps.lifeTime.asMilliseconds();
	auto time = RandomNumber<int>(milis/10, milis); 
	lifeTime = sf::milliseconds(time);
}

void ParticleSystem::updatePixelBatch(const PixelParticles& ps, gsl::span<Quad> quads, gsl::span<InternalData> pixelParticles)
{
	if (ps.areDead())
		return;
	auto deltaTime = VectorEngine::deltaTime();
	auto dt = deltaTime.asSeconds();
	for (int i = 0; i < quads.size(); i++) {
		pixelParticles[i].lifeTime -= deltaTime;
		if (pixelParticles[i].lifeTime > sf::Time::Zero) {
			if (ps.platform.intersects(quads[i].getGlobalRect())) 
				continue;
			pixelParticles[i].speed += ps.gravity * dt;
			quads[i].move(pixelParticles[i].speed * dt);
		} else if (ps.spawn)
			respawnPixelParticle(ps, quads[i], pixelParticles[i].speed, pixelParticles[i].lifeTime);
		else {
			quads[i].setAlpha(0);
		}
	}
}
