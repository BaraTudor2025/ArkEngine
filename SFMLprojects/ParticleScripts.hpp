#pragma once
#include <fstream>
#include <thread>
#include <chrono>
#include "Particles.hpp"
#include "VectorEngine.hpp"

#if 0
#define log_init() log("")
#else
#define log_init()
#endif

namespace ParticlesScripts {

	class EmittFromMouse : public Script {
		Particles* p;
	public:
		void init() override
		{
			log_init();
			p = getComponent<Particles>();
		}
		void update() override
		{
			p->emitter = VectorEngine::mousePositon();
		}
	};

	class TraillingEffect : public Script {
		sf::Vector2f prevMousePos;
		Particles* traillingParticles;
	public:
		bool spawn = true;
		void init()
		{
			log_init();
			traillingParticles = getComponent<Particles>();
		}
		void update() override
		{
			if (spawn) {
				auto mousePos = VectorEngine::mousePositon();
				auto dv = mousePos - prevMousePos;
				auto[speed, angle] = toPolar(-dv);
				if (speed != 0) {
					traillingParticles->spawn = true;
					speed = std::clamp<float>(speed, 0, 5);
				} else
					traillingParticles->spawn = false;
				traillingParticles->angleDistribution.values = { angle - PI / 6, angle + PI / 6 };
				traillingParticles->speedDistribution.values = { 5 * speed , 20 * speed };
				prevMousePos = mousePos;
			}
		}
	};

	class RegisterMousePath : public Script {
		std::vector<sf::Vector2f> path;
		std::string file;
	public:
		RegisterMousePath(std::string file) : file(file) { }
		void update()
		{
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
				path.push_back(VectorEngine::mousePositon());
		}

		void write()
		{
			//unRegister();
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
		Particles* p;
	public:

		PlayModel(std::string file) : file(file) { }

		void init()
		{
			log_init();
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

		void update()
		{
			if (curr == model.begin())
				p->spawn = true;
			if (curr != model.end())
				p->emitter = *curr++;
			else
				p->spawn = false;
		}
	};

	class SpawnLater : public Script {
		int seconds;
		Particles* p;
	public:
		SpawnLater(int seconds) : seconds(seconds) { }

		void init()
		{
			static int i = 1;
			log_init("%d", i++);
			p = getComponent<Particles>();
			p->spawn = false;

			std::thread waitToSpawn([pp = p, sec = seconds]() {
				std::this_thread::sleep_for(std::chrono::seconds(sec));
				pp->spawn = true;
			});
			waitToSpawn.detach();

			this->seppuku();
		}
	};

	template <typename T = Particles>
	class DeSpawnOnMouseClick : public Script {
		T* p = nullptr;
	public:
		void init() override
		{
			log_init();
			if (std::is_base_of_v<Script, T>)
				p = getScript<T>();
			if (std::is_same_v<Particles, T>)
				p = getComponent<T>();
		}
		void onMouseLeftPress() override
		{
			p->spawn = false;
		}
		void onMouseLeftRelease()
		{
			p->spawn = true;
		}
		void onMouseRightPress() override
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
			log_init();
			p = getComponent<Particles>();
		}
		void onMouseRightPress() override
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
			log_init();
			p = getComponent<Particles>();
		}
		void onMouseLeftPress() override
		{
			p->spawn = true;
		}
		void onMouseLeftRelease() override
		{
			p->spawn = false;
		}
	};
}
