#include "Particles.hpp"
#include <iostream>

ParticleSystem ParticleSystem::instance;

void ParticleSystem::update(sf::Time deltaTime, sf::Vector2f mousePos)
{
	int pos = 0;
	for (Particles* p : this->particlesInfos) {
		this->updateBatch(
			deltaTime, *p,
			{ this->vertices.data() + pos, p->count },
			{ this->velocities.data() + pos, p->count },
			{ this->lifeTimes.data() + pos, p->count });
		pos += p->count;
	}
}

void ParticleSystem::add(Data* data)
{
	auto ps = dynamic_cast<Particles*>(data);
	auto addSize = [&](auto& range) { range.resize(range.size() + ps->count); };
	addSize(this->velocities);
	addSize(this->vertices);
	addSize(this->lifeTimes);
	this->particlesInfos.push_back(ps);
}

inline void ParticleSystem::draw(sf::RenderWindow & target)
{
	target.draw(this->vertices.data(), this->vertices.size(), sf::Points);
}

void ParticleSystem::updateBatch(
	sf::Time deltaTime, 
	const Particles & ps, 
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

void ParticleSystem::respawnParticle(const Particles & ps, sf::Vertex & vertex, sf::Vector2f & velocity, sf::Time & lifeTime)
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

void Particles::addThisToSystem()
{
	ParticleSystem::instance.add(this);
}

// arhiva
#if false
namespace cacheInefficient {

	struct Particle final {

		Particle(uint64_t count, sf::Time lifeTime, std::pair<float, float> speedArgs, std::pair<float, float> angleArgs, distribution d)
			: velocities(count),
			lifetimes(count),
			vertices(count),
			lifetime(lifeTime),
			angleDistributionType(d),
			speedDistributionType(d),
			speedDistribution(speedArgs),
			angleDistribution(angleArgs),
			lifetimeDistribution(lifeTime.asMilliseconds() / 2, lifeTime.asMilliseconds())
		{
		}

		/*
		Particles(uint64_t count, sf::Time lifeTime, float speed, float angle)
			: Particles(count, lifeTime, speed/2, speed/4, angle, 0.5f, distribution::normal)
		{ }

		Particles(uint64_t count, sf::Time lifeTime, float speed)
			: Particles(count, lifeTime, speed/2, speed/4, PI, PI, distribution::normal)
		{ }
		*/

		size_t count = 0;
		sf::Time lifetime;
		distribution angleDistributionType;
		std::pair<float, float> angleDistribution;
		distribution speedDistributionType;
		std::pair<float, float> speedDistribution;
		std::pair<int32_t, int32_t> lifetimeDistribution;

		std::vector<sf::Vertex>	vertices;
		std::vector<sf::Vector2f> velocities;
		std::vector<sf::Time> lifetimes;

		sf::Vector2f emitter;
		bool spawn = false;

		sf::Color(*getColor)() = []() { return sf::Color::White; };

		~Particle() { }
	};

	class ParticleSystem final : public sf::NonCopyable, public sf::Drawable, public sf::Transformable {

	public:

		ParticleSystem() = default;

		void update(sf::Time deltaTime)
		{
			for (Particle* p : this->particlesSets)
				this->updateComponent(*p, deltaTime);
		}

		void addParticles(Particle* ps)
		{
			this->particlesSets.push_back(ps);
		}

		~ParticleSystem() { }

	private:

		void updateComponent(Particle& ps, sf::Time deltaTime)
		{
			for (int i = 0; i < ps.vertices.size(); i++)
				ps.lifetimes[i] -= deltaTime;

			for (int i = 0; i < ps.vertices.size(); i++) {

				// if alive
				if (ps.lifetimes[i] > sf::Time::Zero) {
					auto dt = deltaTime.asSeconds();

					ps.velocities[i] += gravityVector * dt;
					ps.vertices[i].position += ps.velocities[i] * dt;

					float ratio = ps.lifetimes[i].asSeconds() / ps.lifetime.asSeconds();
					ps.vertices[i].color.a = static_cast<uint8_t>(ratio * 255);
				} else if (ps.spawn)
					this->respawnParticle(ps, i);
			}
		}

		virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override
		{
			states.transform *= this->getTransform();
			states.texture = nullptr;
			for (Particle* ps : this->particlesSets)
				target.draw(ps->vertices.data(), ps->vertices.size(), sf::Points);
		}

		void respawnParticle(Particle& ps, int i)
		{
			float angle = RandomNumbers(ps.angleDistribution, ps.angleDistributionType);
			float speed = RandomNumbers(ps.speedDistribution, ps.speedDistributionType);

			ps.vertices[i].position = ps.emitter;
			ps.vertices[i].color = ps.getColor();
			ps.velocities[i] = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
			ps.lifetimes[i] = sf::milliseconds(RandomNumbers(ps.lifetimeDistribution, distribution::uniform));
		}

	private:

		std::vector<Particle*> particlesSets;
	};
}
#endif

