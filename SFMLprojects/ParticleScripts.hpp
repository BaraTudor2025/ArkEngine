#pragma once
#include "Ecs.hpp"
#include "Particles.hpp"
#include <fstream>
#include <thread>
#include <chrono>

namespace ParticlesScripts {

	class EmittFromMouse : public Script {
		Particles* p;
	public:
		void init()
		{
			p = getComponent<Particles>();
		}
		void update(sf::Time, sf::Vector2f mousePos) override
		{
			p->emitter = mousePos;
		}
	};

	class TraillingEffect : public Script {
		sf::Vector2f prevMousePos;
	public:
		void init()
		{
			traillingParticles = getComponent<Particles>();
		}
		void update(sf::Time, sf::Vector2f mousePos) override
		{
			if (spawn) {
				auto dv = mousePos - prevMousePos;
				auto[speed, angle] = toPolar(-dv);
				if (speed != 0) {
					traillingParticles->spawn = true;
					speed = std::clamp<float>(speed, 0, 4);
				} else
					traillingParticles->spawn = false;
				traillingParticles->angleDistribution.values = { angle - PI / 6, angle + PI / 6 };
				traillingParticles->speedDistribution.values = { 20 * speed, 30 * speed };
				prevMousePos = mousePos;
			}
		}

		Particles* traillingParticles;
		bool spawn = true;
	};

	class RegisterMousePath : public Script {
		std::vector<sf::Vector2f> path;
		std::string file;
	public:
		RegisterMousePath(std::string file) : file(file) { }
		void update(sf::Time, sf::Vector2f mousePos)
		{
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
				path.push_back(mousePos);
		}

		void write()
		{
			unRegister();
		}

		~RegisterMousePath()
		{
			std::ofstream fout(file);
			fout << path.size() << ' ';
			for (auto[x, y] : path)
				fout << x << ' ' << y << ' ';
		}
	};

	class PlayModel : public Script {
		std::vector<sf::Vector2f> model;
		std::vector<sf::Vector2f>::iterator curr;
		std::string file;
	public:

		PlayModel(std::string file) : file(file) { }

		void init()
		{
			p = getComponent<Particles>();
			std::ifstream fin(file);
			if (!fin.is_open()) {
				std::cerr << "nu-i fisierul: " + file + "\n";
				std::getchar();
				exit(-2025);
			}
			int size;
			fin >> size;
			model.resize(size);
			for (auto&[x, y] : model)
				fin >> x >> y;
			curr = model.begin();
			p->spawn = false;
		}

		void update(sf::Time, sf::Vector2f)
		{
			if (curr == model.begin())
				p->spawn = true;
			if (curr != model.end())
				p->emitter = *curr++;
			else
				p->spawn = false;
		}
		Particles* p;
	};

	class SpawnLater : public Script {
		std::vector<Particles*> particles;
		int seconds;
	public:
		SpawnLater(int seconds) : seconds(seconds) { }

		void init()
		{
			particles = getComponents<Particles>();
			for (auto& p : particles)
				p->spawn = false;

			std::thread waitToSpawn([sec = seconds, ps = std::move(particles)]() {
				std::this_thread::sleep_for(std::chrono::seconds(sec));
				for (auto& p : ps)
					p->spawn = true;
			});
			waitToSpawn.detach();
			this->unRegister();
		}
	};

	template <typename T = Particles>
	class DeSpawnOnMouseClick : public Script {
		T* p = nullptr;
	public:
		void init()
		{
			if constexpr (std::is_base_of_v<Script, T>)
				p = getScript<T>();
			else if constexpr (std::is_base_of_v<Data, T>)
				p = getComponent<T>();
		}
		void onMouseLeftPress(sf::Vector2f) override
		{
			p->spawn = false;
		}
		void onMouseLeftRelease()
		{
			p->spawn = true;
		}
		void onMouseRightPress(sf::Vector2f) override
		{
			p->spawn = false;
		}
		void onMouseRightRelease()
		{
			p->spawn = true;
		}
	};

	class SpawnOnRightClick : public Script {
		Particles* p;
	public:
		void init()
		{
			p = getComponent<Particles>();
		}
		void onMouseRightPress(sf::Vector2f) override
		{
			p->spawn = true;
		}
		void onMouseRightRelease() override
		{
			p->spawn = false;
		}
	};

	class SpawnOnLeftClick : public Script {
		Particles* p;
	public:
		void init()
		{
			p = getComponent<Particles>();
		}
		void onMouseLeftPress(sf::Vector2f) override
		{
			p->spawn = true;
		}
		void onMouseLeftRelease() override
		{
			p->spawn = false;
		}
	};
}
