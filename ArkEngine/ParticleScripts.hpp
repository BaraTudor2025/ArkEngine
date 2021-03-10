#pragma once

#include <fstream>
#include <thread>
#include <chrono>

#include <ark/core/Engine.hpp>
#include <ark/ecs/components/Transform.hpp>

#include "ParticleSystem.hpp"
#include "ScriptingSystem.hpp"

namespace ParticleScripts {

	class EmittFromMouse : public ScriptT<EmittFromMouse> {
		PointParticles* p;
	public:
		void bind() noexcept override
		{
			p = getComponent<PointParticles>();
		}
		void update() noexcept override
		{
			p->emitter = ark::Engine::mousePositon();
		}
	};

	class TraillingEffect : public ScriptT<TraillingEffect> {
		sf::Vector2f prevEmitter;
		PointParticles* p;
	public:
		bool spawn = true;

		void bind() noexcept override
		{
			p = getComponent<PointParticles>();
		}

		void update() noexcept override
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

		void bind() noexcept override
		{
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

		void update() noexcept override
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

		void bind() noexcept override
		{
			p = getComponent<PointParticles>();
			//p->emitter = distance;
			//p->applyTransformOnEmitter = true;
			//t = getComponent<Transform>();
			//t->move(around);
			//t->setOrigin(around);
			//offset =  p->emitter - around;
		}

		void update() noexcept override
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

		void bind() noexcept override {
			t = getComponent<ark::Transform>();
			t->setPosition(around);
			t->setOrigin(around);
			
			auto p = getComponent<PointParticles>();
			p->emitter = around;
			p->applyTransform = true;
		}

		void update() noexcept override {
			t->rotate(angle * ark::Engine::deltaTime().asSeconds());
		}
	};


	class SpawnOnRightClick : public ScriptT<SpawnOnRightClick> {
		PointParticles* p;
	public:
		void bind() noexcept override
		{
			p = getComponent<PointParticles>();
		}
		void handleEvent(const sf::Event& ev) noexcept override
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
		void bind() noexcept override
		{
			p = getComponent<PointParticles>();
		}
		void handleEvent(const sf::Event& ev) noexcept override
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
		void bind() noexcept override
		{
			if constexpr (std::is_base_of_v<Script, T>)
				p = this->getComponent<ScriptingComponent>()->getScript<T>();
			if constexpr (std::is_same_v<PointParticles, T>)
				p = this->getComponent<T>();
		}
		void handleEvent(const sf::Event& event) noexcept override
		{
			switch (event.type) {
			case sf::Event::MouseButtonReleased: p->spawn = true; break;
			case sf::Event::MouseButtonPressed: p->spawn = false; break;
			default:
				break;
			}
		}
	};

}
