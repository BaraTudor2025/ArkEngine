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

struct PointParticles final {

	PointParticles() : vertices(0), data(0) { }

	PointParticles(int count, sf::Time lifeTime,
	          Distribution<float> speedArgs = { 0, 0 },
	          Distribution<float> angleArgs = { 0.f, 2 * 3.14159f },
	          DistributionType lifeTimeDistType = DistributionType::uniform,
	          std::function<sf::Color()> getColor = []() { return sf::Color::White; })
	    : count(count),
	    lifeTime(lifeTime),
	    speedDistribution(speedArgs),
	    angleDistribution(angleArgs)
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

	sf::Color colorLowerBound = sf::Color::Cyan;
	sf::Color colorUpperBound = sf::Color::Magenta;

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

ARK_REGISTER_COMPONENT(PointParticles, registerServiceDefault<PointParticles>())
{
	auto* type = ark::meta::type<PointParticles>();
	type->func("export_to_lua", exportTypeToLua<PointParticles>);
	type->func("lua_table_from_pointer", tableFromPointer<PointParticles>);
	using PP = PointParticles;
	return members<PointParticles>(
		member_property("particleNumber", &PP::getParticleNumber, &PP::setParticleNumber),
		member_property("lifeTime", &PP::getLifeTime, &PP::setLifeTime),
		member_property("spawn", &PP::spawn),
		member_property("fireworks", &PP::fireworks),
		member_property("emitter", &PP::emitter),
		member_property("speedDistribution", &PP::speedDistribution),
		member_property("angleDistribution", &PP::angleDistribution),
		member_property("colorLowerBound", &PP::colorLowerBound),
		member_property("colorUpperBound", &PP::colorUpperBound)
	);
}

struct PixelParticles {

	using Colors = std::pair<sf::Color, sf::Color>;

	PixelParticles() : quads(0), data(0) { }

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

ARK_REGISTER_COMPONENT_WITH_TAG(sf::FloatRect, floatrect ,registerServiceDefault<sf::FloatRect>())
{
	auto m = members<sf::FloatRect>(
		member_property("top", &sf::FloatRect::top),
		member_property("left", &sf::FloatRect::left),
		member_property("height", &sf::FloatRect::height),
		member_property("width", &sf::FloatRect::width)
	);
	return m;
}

ARK_REGISTER_COMPONENT_WITH_NAME_TAG(PixelParticles::Colors, "ColorPair", pixelcolorpair, registerServiceDefault<PixelParticles::Colors>())
{
	return members<PixelParticles::Colors>(
		member_property("dominant", &PixelParticles::Colors::first),
		member_property("subdominant", &PixelParticles::Colors::second)
	);
}

ARK_REGISTER_COMPONENT(PixelParticles, registerServiceDefault<PixelParticles>())
{
	return members<PixelParticles>(
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

inline void colorRangeRed(PointParticles& ps) {
	ps.colorLowerBound = sf::Color(255, 0, 0);
	ps.colorUpperBound = sf::Color(255, 200, 0);
}
inline void colorRangeGreen(PointParticles& ps) {
	ps.colorLowerBound = sf::Color(0, 255, 0);
	ps.colorUpperBound = sf::Color(200, 255, 0);
}
inline void colorRangeBrightBlue(PointParticles& ps) {
	ps.colorLowerBound = sf::Color(0, 100, 255);
	ps.colorUpperBound = sf::Color(0, 255, 255);
}
inline void colorRangeDarkBlue(PointParticles& ps) {
	ps.colorLowerBound = sf::Color(10, 20, 240);
	ps.colorUpperBound = sf::Color(10, 150, 240);
}
inline void colorRangeAll(PointParticles& ps) {
	ps.colorLowerBound = sf::Color(0, 0, 0);
	ps.colorUpperBound = sf::Color(255, 255, 255);
}
inline void colorRangeBlue(PointParticles& ps) {
	ps.colorLowerBound = sf::Color(10, 20, 240);
	ps.colorUpperBound = sf::Color(10, 250, 240);
}

inline PointParticles getFireParticles(int count=1000) 
{
	PointParticles fireParticles = { count, sf::seconds(3), { 1, 100 } };
	colorRangeRed(fireParticles);
	return fireParticles;
}

inline PointParticles getRainbowParticles()
{
	PointParticles rainbowParticles(2000, sf::seconds(3), { 1, 100 , DistributionType::normal});
	colorRangeAll(rainbowParticles);
	return rainbowParticles;
}

inline PointParticles getGreenParticles()
{
	auto greenParticles = getFireParticles();
	colorRangeGreen(greenParticles);
	return greenParticles;
}

class PointParticleSystem : public ark::SystemT<PointParticleSystem>, public ark::Renderer {
	ark::View<PointParticles> view;
public:
	void init() override
	{
		view = entityManager.view<PointParticles>();
	}

	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	void update() override;
	void render(sf::RenderTarget&) override;

private:
	void respawnPointParticle(const PointParticles& ps, sf::Vertex& vertex, sf::Vector2f& speed, sf::Time& lifeTime);
};



class PixelParticleSystem : public ark::SystemT<PixelParticleSystem>, public ark::Renderer {
	ark::View<PixelParticles> view;
public:
	void init() override
	{
		view = entityManager.view<PixelParticles>();
		//querry.onEntityAdd([this](ark::Entity entity) {
		//	auto& p = entity.getComponent<PixelParticles>();
		//	if (p.spawn)
		//		p.deathTimer = sf::Time::Zero;
		//	else
		//		p.deathTimer = p.lifeTime;
		//});
	}

	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

	void update() override;
	void render(sf::RenderTarget&) override;

private:
	void respawnPixelParticle(const PixelParticles& ps, Quad& quad, sf::Vector2f& speed, sf::Time& lifeTime);
};

