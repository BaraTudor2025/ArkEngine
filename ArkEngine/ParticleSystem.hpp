#pragma once

#include "Component.hpp"
#include "System.hpp"
#include "RandomNumbers.hpp"
#include "Quad.hpp"
#include "Util.hpp"

#include <functional>
#include <optional>
#include <vector>

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>

#include <libs/Meta.h>

static inline constexpr auto PI = 3.14159f;

struct PointParticles final : public Component<PointParticles> {

	PointParticles() : vertices(0), data(0) { }
	COPYABLE(PointParticles)
	MOVABLE(PointParticles)

	PointParticles(int count, sf::Time lifeTime,
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
		setParticleNumber(count);
		setLifeTime(lifeTime);
	}

	void setParticleNumber(int count) { 
		this->count = count;
		this->vertices.resize(count);
		this->data.resize(count);
	}

	int getParticleNumber() const {
		return count;
	}

	sf::Time getLifeTime() const {
		return lifeTime;
	}

	void setLifeTime(sf::Time lifeTime) {
		setLifeTime(lifeTime, lifeTimeDistribution.type);
	}

	void setLifeTime(sf::Time lifeTime, DistributionType type) {
		this->lifeTime = lifeTime;
		if(type == DistributionType::uniform)
			this->makeLifeTimeDistUniform();
		else
			this->makeLifeTimeDistNormal();
	}

	Distribution<float> speedDistribution{0.f, 0.f};
	Distribution<float> angleDistribution{0.f, 2 * 3.14159f};

	sf::Vector2f emitter{ 0.f, 0.f };
	std::function<sf::Color()> getColor = []() { return sf::Color::White; };

	bool spawn = false;
	bool fireworks = false;
	bool applyTransform = false;

private:

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

	struct InternalData {
		sf::Vector2f speed;
		sf::Time lifeTime = sf::Time::Zero;
	};

	int count = 0;
	sf::Time lifeTime = sf::Time::Zero;
	std::vector<sf::Vertex>	vertices;
	std::vector<InternalData> data;
	sf::Time deathTimer = sf::Time::Zero;
	Distribution<float> lifeTimeDistribution{0.f, 0.f};
	bool areDead() const { return deathTimer >= lifeTime; }
	friend class PointParticleSystem;
};

namespace meta {
	template <> inline auto registerMembers<PointParticles>()
	{
		return members(
			member("particleNumber", &PointParticles::getParticleNumber, &PointParticles::setParticleNumber),
			member("lifeTime", &PointParticles::getLifeTime, &PointParticles::setLifeTime),
			member("spawn", &PointParticles::spawn),
			member("fireworks", &PointParticles::fireworks),
			member("emitter", &PointParticles::emitter),
			member("speedDistribution", &PointParticles::speedDistribution),
			member("angleDistribution", &PointParticles::angleDistribution)
		);
	}

	template <> inline auto registerMembers<Distribution<float>>()
	{
		return members(
			member("lower", &Distribution<float>::a),
			member("upper", &Distribution<float>::b),
			member("type", &Distribution<float>::type)
		);
	}

	template <> inline auto registerEnum<DistributionType>()
	{
		return enumValues<DistributionType>(
			enumValue("uniform", DistributionType::uniform),
			enumValue("normal", DistributionType::normal)
		);
	}
}

struct PixelParticles : public Component<PixelParticles> {

	using Colors = std::pair<sf::Color, sf::Color>;

	PixelParticles() : quads(0), data(0) { }
	COPYABLE(PixelParticles)
	MOVABLE(PixelParticles)

	PixelParticles(size_t count, sf::Time lifeTime, sf::Vector2f size, Colors colors)
		:count(count), particlesPerSecond(count), size(size), colors(colors), lifeTime(lifeTime) 
	{
		setParticleNumber(count);
		setColors(colors);
	}

	void setParticleNumber(int count) { 
		this->count = count;
		this->quads.resize(count);
		this->data.resize(count);
	}

	int getParticleNumber() {
		return count;
	}

	void setColors(Colors colors)
	{
		this->colors = colors;
		for (auto& q : quads)
			q.setColors(colors.first, colors.second);
	}

	float particlesPerSecond = 0;
	sf::Vector2f size{1.f, 1.f};
	Colors colors{sf::Color::Red, sf::Color::Yellow};
	sf::Vector2f emitter{0.f, 0.f};
	float speed = 0;
	sf::Time lifeTime = sf::Time::Zero;
	Distribution<float> angleDistribution = { 0, 2 * PI, DistributionType::normal };
	bool spawn = false;
	sf::Vector2f gravity{0.f, 0.f};
	sf::FloatRect platform; // particles can't go through this

private:
	struct InternalData {
		sf::Vector2f speed;
		sf::Time lifeTime = sf::Time::Zero;
	};
	int count = 0;
	float particlesToSpawn = 0; // internal counter of particles to spawn per second
	int spawnBeingPos = 0;
	std::vector<Quad> quads;
	std::vector<InternalData> data;
	sf::Time deathTimer = sf::Time::Zero;
	bool areDead() const { return deathTimer >= lifeTime; }
	friend class PixelParticleSystem;
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

inline PointParticles getFireParticles(int count=1000) 
{
	PointParticles fireParticles = { count, sf::seconds(3), { 1, 100 } };
	fireParticles.getColor = makeRed;
	return fireParticles;
}

inline PointParticles getRainbowParticles()
{
	PointParticles rainbowParticles(2000, sf::seconds(3), { 1, 100 , DistributionType::normal});
	rainbowParticles.getColor = makeColor;
	return rainbowParticles;
}

inline PointParticles getGreenParticles()
{
	auto greenParticles = getFireParticles();
	greenParticles.getColor = makeGreen;
	return greenParticles;
}

class PointParticleSystem : public SystemT<PointParticleSystem>, public Renderer {

public:
	void init() override
	{
		requireComponent<PointParticles>();
	}

	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	void onEntityAdded(Entity) override;
	void update() override;
	void render(sf::RenderTarget&) override;

private:
	void respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime);
};



class PixelParticleSystem : public SystemT<PixelParticleSystem>, public Renderer {

public:
	void init() override
	{
		requireComponent<PixelParticles>();
	}

	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	void onEntityAdded(Entity) override;
	void update() override;
	void render(sf::RenderTarget&) override;

private:
	void respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime);
};

