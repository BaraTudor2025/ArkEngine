#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>

#include <ark/ecs/Component.hpp>
#include <ark/ecs/System.hpp>
#include <ark/ecs/Meta.hpp>
#include <ark/util/RandomNumbers.hpp>
#include <ark/util/Util.hpp>
#include <ark/ecs/DefaultServices.hpp>

#include "Quad.hpp"
#include "LuaScriptingSystem.hpp"

static inline constexpr auto PI = 3.14159f;

struct PointParticles final : public ark::Component<PointParticles> {

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
			(float)lifeTime.asMilliseconds() * (4.f / 10.f),
			(float)lifeTime.asMilliseconds() / 4.5f,
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

ARK_REGISTER_TYPE(PointParticles, "PointParticles", ARK_DEFAULT_SERVICES, 
	ark::meta::service<PointParticles>("export_to_lua", exportTypeToLua<PointParticles>), 
	ark::meta::service<PointParticles>("lua_table_from_pointer", tableFromPointer<PointParticles>))
{
	using PP = PointParticles;
	return members(
		member_property("particleNumber", &PP::getParticleNumber, &PP::setParticleNumber),
		member_property("lifeTime", &PP::getLifeTime, &PP::setLifeTime),
		member_property("spawn", &PP::spawn),
		member_property("fireworks", &PP::fireworks),
		member_property("emitter", &PP::emitter),
		member_property("speedDistribution", &PP::speedDistribution),
		member_property("angleDistribution", &PP::angleDistribution)
	);
}

struct PixelParticles : public ark::Component<PixelParticles> {

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
		this->setColors(this->colors);
	}

	int getParticleNumber() const {
		return count;
	}

	void setColors(Colors colors)
	{
		this->colors = colors;
		for (auto& q : quads)
			q.setColors(colors.first, colors.second);
	}

	Colors getColors() const {
		return colors;
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

ARK_REGISTER_TYPE(sf::FloatRect, "FloatRect", ARK_DEFAULT_SERVICES)
{
	constexpr auto m = members(
		member_property("top", &sf::FloatRect::top),
		member_property("left", &sf::FloatRect::left),
		member_property("height", &sf::FloatRect::height),
		member_property("width", &sf::FloatRect::width)
	);
	return m;
}

ARK_REGISTER_TYPE(PixelParticles::Colors, "ColorPair", ARK_DEFAULT_SERVICES)
{
	return members(
		member_property("dominant", &PixelParticles::Colors::first),
		member_property("subdominant", &PixelParticles::Colors::second)
	);
}

ARK_REGISTER_TYPE(PixelParticles, "PixelParticles", ARK_DEFAULT_SERVICES)
{
	return members(
		member_property("particle_number", &PixelParticles::getParticleNumber, &PixelParticles::setParticleNumber),
		member_property("particles_per_second", &PixelParticles::particlesPerSecond),
		member_property("spawn", &PixelParticles::spawn),
		member_property("speed", &PixelParticles::speed),
		member_property("gravity", &PixelParticles::gravity),
		member_property("emitter", &PixelParticles::emitter),
		member_property("size", &PixelParticles::size),
		member_property("life_time", &PixelParticles::lifeTime),
		member_property("angle_dist", &PixelParticles::angleDistribution),
		member_property("platform", &PixelParticles::platform),
		member_property("colors", &PixelParticles::getColors, &PixelParticles::setColors)
	);
}


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

class PointParticleSystem : public ark::SystemT<PointParticleSystem>, public ark::Renderer {

public:
	void init() override
	{
		requireComponent<PointParticles>();
	}

	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	void onEntityAdded(ark::Entity) override;
	void update() override;
	void render(sf::RenderTarget&) override;

private:
	void respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime);
};



class PixelParticleSystem : public ark::SystemT<PixelParticleSystem>, public ark::Renderer {

public:
	void init() override
	{
		requireComponent<PixelParticles>();
	}

	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	void onEntityAdded(ark::Entity) override;
	void update() override;
	void render(sf::RenderTarget&) override;

private:
	void respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime);
};

