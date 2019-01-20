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

namespace ParticleScripts {

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
		sf::Vector2f prevEmitter;
		Particles* p;
	public:
		bool spawn = true;

		void init()
		{
			log_init();
			p = getComponent<Particles>();
		}

		void update() override
		{
			if (spawn) {
				auto dv = p->emitter - prevEmitter;
				auto[speed, angle] = toPolar(-dv);
				if (speed != 0) {
					p->spawn = true;
					speed = std::clamp<float>(speed, 0, 5);
				} else
					p->spawn = false;
				p->angleDistribution = { angle - PI / 6, angle + PI / 6 };
				p->speedDistribution = { 5 * speed , 20 * speed };
				prevEmitter = p->emitter;
			}
		}
	};

	class PlayModel : public Script {
		std::vector<sf::Vector2f> model;
		std::vector<sf::Vector2f>::iterator curr;
		std::string file;
		Particles* p;
		sf::Vector2f offset;
	public:

		PlayModel(std::string file, sf::Vector2f offset = { 0.f, 0.f }) : file(file), offset(offset) { }

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
			for (auto&[x, y] : model) {
				fin >> x >> y;
				x += offset.x;
				y += offset.y;
			}
			curr = model.begin();
			p->spawn = false;
		}

		void update()
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

	auto __IMPL__readColorDistributionFromStream = [](std::istream& is, Particles* p)
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

	auto __IMPL__readColorFromStream = [](std::istream& is, Particles* p)
	{
		int r, g, b;
		is >> r >> g >> b;
		p->getColor = [=]() {
			return sf::Color(r, g, b);
		};
	};

	class ReadColorDistributionFromFile : public Script {
		Particles* p;
		std::string fileName;
	public:

		ReadColorDistributionFromFile(std::string fileName) : fileName(fileName) { }

		void init()
		{
			p = getComponent<Particles>();
			std::thread t([&](){
				while (true) {
					std::cout << "press enter to read colors from: " + fileName;
					std::cin.get();
					std::ifstream fin(fileName);
					__IMPL__readColorDistributionFromStream(fin, p);
					fin.close();
				}
			});
			t.detach();
		}
	};

	class ReadColorDistributionFromConsole : public Script {
		Particles* p;
	public:
		void init()
		{
			p = getComponent<Particles>();
			std::thread t([&]() {
				while (true) {
					std::cout << "enter rgb low and high: ";
					__IMPL__readColorDistributionFromStream(std::cin, p);
				}
			});
			t.detach();
		}
	};

	class ReadColorFromConsole : public Script {
		Particles* p;
	public:
		void init()
		{
			p = getComponent<Particles>();
			std::thread t([&]() {
				while (true) {
					std::cout << "enter rgb : ";
					__IMPL__readColorFromStream(std::cin, p);
				}
			});
			t.detach();
		}
	};

	class ReadColorFromFile : public Script {
		Particles* p;
		std::string fileName;
	public:

		ReadColorFromFile(std::string fileName) : fileName(fileName) { }

		void init()
		{
			p = getComponent<Particles>();
			std::thread t([&](){
				while (true) {
					std::cout << "press enter to read colors from: " + fileName;
					std::cin.get();
					std::ifstream fin(fileName);
					__IMPL__readColorFromStream(fin, p);
					fin.close();
				}
			});
			t.detach();
		}
	};

	class RoatateEmitter : public Script {
		sf::Transform t;
		Particles* p;
		sf::Vector2f offset;
		sf::Vector2f rotateAroundThis;
		float angle;

	public:
		RoatateEmitter(float a) :angle(a) { }

		void init()
		{
			p = getComponent<Particles>();
			rotateAroundThis = VectorEngine::center();
			offset =  p->emitter - rotateAroundThis;
		}

		void update()
		{
			t.rotate(angle * VectorEngine::deltaTime().asSeconds(), rotateAroundThis);
			p->emitter = t.transformPoint(rotateAroundThis + offset);
		}
	};

	//class Rotate : public Script {
	//	//Transform* t;
	//	float angle;
	//	sf::Vector2f around;
	//	Particles* p;
	//public:
	//	Rotate(float a, sf::Vector2f around) : angle(a), around(around) { }
	//	void init()
	//	{
	//		//t = getComponent<Transform>();
	//		p = getComponent<Particles>();
	//		p->transform = std::make_unique<Transform>();
	//		p->transform->setOrigin(around);
	//		p->transform->setPosition(around);
	//		p->emitter = around;
	//		//p->applyTransform = true;
	//	}
	//	void update()
	//	{
	//		p->transform->rotate(angle * VectorEngine::deltaTime().asSeconds());
	//	}
	//};

	class SpawnLater : public Script {
		int seconds;
		Particles* p;
	public:
		SpawnLater(int seconds) : seconds(seconds) { }

		void init()
		{
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
		void handleInput(sf::Event event)
		{
			switch (event.type) {
			case sf::Event::MouseButtonPressed: p->spawn = false; break;
			case sf::Event::MouseButtonReleased: p->spawn = true; break;
			default:
				break;
			}
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
		void handleInput(sf::Event ev)
		{
			switch (ev.type)
			{
			case sf::Event::MouseButtonPressed:
				if (ev.mouseButton.button == sf::Mouse::Right)
					p->spawn = true;
				break;
			case sf::Event::MouseButtonReleased:
				if (ev.mouseButton.button == sf::Mouse::Right)
					p->spawn = false;
				break;
			default:
				break;
			}
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
		void handleInput(sf::Event ev)
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
}
