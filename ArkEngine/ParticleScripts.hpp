#pragma once

#include "ParticleSystem.hpp"
#include "Engine.hpp"
#include "Transform.hpp"
#include "Script.hpp"

#include <fstream>
#include <thread>
#include <chrono>


#if 0
#define log_init() log("")
#else
#define log_init()
#endif

namespace ParticleScripts {

	using ark::ScriptT;
	using ark::Engine;

	class EmittFromMouse : public ScriptT<EmittFromMouse> {
		PointParticles* p;
	public:
		void init() override
		{
			log_init();
			p = getComponent<PointParticles>();
		}
		void update() override
		{
			p->emitter = Engine::mousePositon();
		}
	};

	class TraillingEffect : public ScriptT<TraillingEffect> {
		sf::Vector2f prevEmitter;
		PointParticles* p;
	public:
		bool spawn = true;

		void init() override
		{
			log_init();
			p = getComponent<PointParticles>();
		}

		void update() override
		{
			if (spawn) {
				auto dv = p->emitter - prevEmitter;
				auto[speed, angle] = Util::toPolar(-dv);
				if (speed != 0) {
					p->spawn = true;
					speed = std::clamp<float>(speed, 0, 5);
				} else
					p->spawn = false;
				auto at = p->angleDistribution.type;
				auto st = p->speedDistribution.type;
				p->angleDistribution = {angle - PI / 6, angle + PI / 6 , at};
				p->speedDistribution = {5 * speed , 20 * speed , st};
				prevEmitter = p->emitter;
			}
		}
	};

	class PlayModel : public ScriptT<PlayModel> {
		std::vector<sf::Vector2f> model;
		std::vector<sf::Vector2f>::iterator curr;
		std::string file;
		PointParticles* p;
		sf::Vector2f offset;
	public:

		PlayModel() = default;
		PlayModel(std::string file, sf::Vector2f offset = { 0.f, 0.f }) : file(file), offset(offset) { }

		void init() override
		{
			log_init();
			p = getComponent<PointParticles>();
			std::ifstream fin(file);
			if (!fin.is_open()) {
				//std::cerr << "nu-i fisierul: " + file + "\n";
				std::getchar();
				exit(-2025);
			}
			int size;
			fin >> size;
			model.resize(size);
			for (auto&[x, y] : model) {
				fin >> x >> y;
				x += offset.x;
				y += offset.y;
			}
			curr = model.begin();
			p->spawn = false;
		}

		void update() override
		{
			if (curr == model.begin())
				p->spawn = true;
			if (curr != model.end()) {
				auto pixel = *curr++;
				p->emitter = pixel;
				if (curr != model.end())
					curr++;
				if (curr != model.end())
					curr++;
			}
			else
				p->spawn = false;
		}
	};

	auto __IMPL__readColorDistributionFromStream = [](std::istream& is, PointParticles* p)
	{
		int r1, r2, g1, g2, b1, b2;
		is >> r1 >> r2 >> g1 >> g2 >> b1 >> b2;
		p->getColor = [=]() {
			auto r = RandomNumber<int>(r1, r2);
			auto g = RandomNumber<int>(g1, g2);
			auto b = RandomNumber<int>(b1, b2);
			return sf::Color(r, g, b);
		};
	};

	auto __IMPL__readColorFromStream = [](std::istream& is, PointParticles* p)
	{
		int r, g, b;
		is >> r >> g >> b;
		p->getColor = [=]() {
			return sf::Color(r, g, b);
		};
	};


	class RotateEmitter : public ScriptT<RotateEmitter> {
		sf::Transform t;
		PointParticles* p;
		//Transform* t;

	public:
		sf::Vector2f distance;
		sf::Vector2f around;
		float angleSpeed;

		RotateEmitter() = default;
		RotateEmitter(float angle, sf::Vector2f around, float distance)
			:angleSpeed(angle), around(around), distance(distance, 0) { }

		void init() override
		{
			p = getComponent<PointParticles>();
			//p->emitter = distance;
			//p->applyTransformOnEmitter = true;
			//t = getComponent<Transform>();
			//t->move(around);
			//t->setOrigin(around);
			//offset =  p->emitter - around;
		}

		void update() override
		{
			t.rotate(angleSpeed * ark::Engine::deltaTime().asSeconds(), around);
			p->emitter = t.transformPoint(around + distance);
		}
	};

	class Rotate : public ScriptT<Rotate> {
		ark::Transform* t;
		float angle;
		sf::Vector2f around;

	public:
		Rotate() = default;
		Rotate(float angle, sf::Vector2f around) : angle(angle), around(around) { }

		void init() override {
			t = getComponent<ark::Transform>();
			t->setPosition(around);
			t->setOrigin(around);
			
			auto p = getComponent<PointParticles>();
			p->emitter = around;
			p->applyTransform = true;
		}

		void update() override {
			t->rotate(angle * Engine::deltaTime().asSeconds());
		}
	};


	class SpawnOnRightClick : public ScriptT<SpawnOnRightClick> {
		PointParticles* p;
	public:
		void init()
		{
			log_init();
			p = getComponent<PointParticles>();
		}
		void handleEvent(const sf::Event& ev) override
		{
			switch (ev.type)
			{
			case sf::Event::MouseButtonPressed:
				if (ev.mouseButton.button == sf::Mouse::Right) {
					p->spawn = true;
				}
				break;
			case sf::Event::MouseButtonReleased:
				if (ev.mouseButton.button == sf::Mouse::Right) {
					p->spawn = false;
				}
				break;
			default:
				break;
			}
		}
	};

	class SpawnOnLeftClick : public ScriptT<SpawnOnLeftClick> {
		PointParticles* p;
	public:
		void init() override
		{
			log_init();
			p = getComponent<PointParticles>();
		}
		void handleEvent(const sf::Event& ev) override
		{
			switch (ev.type)
			{
			case sf::Event::MouseButtonPressed:
				if (ev.mouseButton.button == sf::Mouse::Left)
					p->spawn = true;
				break;
			case sf::Event::MouseButtonReleased:
				if (ev.mouseButton.button == sf::Mouse::Left)
					p->spawn = false;
				break;
			default:
				break;
			}
		}
	};

	template <typename T = PointParticles>
	class DeSpawnOnMouseClick : public ScriptT<DeSpawnOnMouseClick<T>> {
		T* p = nullptr;
	public:
		void init() override
		{
			log_init();
			if constexpr (std::is_base_of_v<ark::Script, T>)
				p = this->getScript<T>();
			if constexpr (std::is_same_v<PointParticles, T>)
				p = this->getComponent<T>();
		}
		void handleEvent(const sf::Event& event) override
		{
			switch (event.type) {
			case sf::Event::MouseButtonPressed: p->spawn = false; break;
			case sf::Event::MouseButtonReleased: p->spawn = true; break;
			default:
				break;
			}
		}
	};

}
