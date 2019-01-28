#pragma once
#include <fstream>
#include <thread>
#include <chrono>
#include "ParticleSystem.hpp"
#include "VectorEngine.hpp"

#if 0
#define log_init() log("")
#else
#define log_init()
#endif

namespace ParticleScripts {

	class EmittFromMouse : public Script {
		PointParticles* p;
	public:
		void init() override
		{
			log_init();
			p = getComponent<PointParticles>();
		}
		void update() override
		{
			p->emitter = VectorEngine::mousePositon();
		}
	};

	class TraillingEffect : public Script {
		sf::Vector2f prevEmitter;
		PointParticles* p;
	public:
		bool spawn = true;

		void init()
		{
			log_init();
			p = getComponent<PointParticles>();
		}

		void fixedUpdate(sf::Time) override
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
		PointParticles* p;
		sf::Vector2f offset;
	public:

		PlayModel(std::string file, sf::Vector2f offset = { 0.f, 0.f }) : file(file), offset(offset) { }

		void init()
		{
			log_init();
			p = getComponent<PointParticles>();
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


	class RoatateEmitter : public Script {
		sf::Transform t;
		PointParticles* p;
		//Transform* t;
		sf::Vector2f distance;
		sf::Vector2f around;
		float angleSpeed;

	public:
		RoatateEmitter(float angle, sf::Vector2f around, float distance)
			:angleSpeed(angle), around(around), distance(distance, 0) { }

		void init()
		{
			p = getComponent<PointParticles>();
			//p->emitter = distance;
			//p->applyTransformOnEmitter = true;
			//t = getComponent<Transform>();
			//t->move(around);
			//t->setOrigin(around);
			//offset =  p->emitter - around;
		}

		void update()
		{
			t.rotate(angleSpeed * VectorEngine::deltaTime().asSeconds(), around);
			p->emitter = t.transformPoint(around + distance);
		}
	};

	class Rotate : public Script {
		Transform* t;
		float angle;
		sf::Vector2f around;

	public:
		Rotate(float angle, sf::Vector2f around) : angle(angle), around(around) { }

		void init() {
			t = getComponent<Transform>();
			t->setPosition(around);
			t->setOrigin(around);
			
			auto p = getComponent<PointParticles>();
			p->emitter = around;
			p->applyTransform = true;
		}

		void update() {
			t->rotate(angle * VectorEngine::deltaTime().asSeconds());
		}
	};


	class SpawnOnRightClick : public Script {
		PointParticles* p;
	public:
		void init()
		{
			log_init();
			p = getComponent<PointParticles>();
		}
		void handleEvent(sf::Event ev)
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
		PointParticles* p;
	public:
		void init()
		{
			log_init();
			p = getComponent<PointParticles>();
		}
		void handleEvent(sf::Event ev)
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
	class DeSpawnOnMouseClick : public Script {
		T* p = nullptr;
	public:
		void init() override
		{
			log_init();
			if (is_script_v<T>)
				p = getScript<T>();
			if (std::is_same_v<PointParticles, T>)
				p = getComponent<T>();
		}
		void handleEvent(sf::Event event)
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

namespace ParticleScripts::Action {

	void SpawnLater(Entity& e, std::any a)
	{
		std::thread waitToSpawn([&e, a = std::move(a)]() {
			auto p = e.getComponent<PointParticles>();
			p->spawn = false;
			auto seconds = std::any_cast<int>(a);
			std::this_thread::sleep_for(std::chrono::seconds(seconds));
			p->spawn = true;
		});
		waitToSpawn.detach();
	}

	void ReadColorDistributionFromFile(Entity& e, std::any fileName){
		std::thread t([&e, arg = std::move(fileName)](){
			auto fileName = std::any_cast<std::string>(arg);
			auto p = e.getComponent<PointParticles>();
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

	void ReadColorDistributionFromConsole(Entity& e, std::any){
		std::thread t([&e]() {
			auto p = e.getComponent<PointParticles>();
			while (true) {
				std::cout << "enter rgb low and high: ";
				__IMPL__readColorDistributionFromStream(std::cin, p);
			}
		});
		t.detach();
	}

	void ReadColorFromConsole(Entity& e, std::any){
		std::thread t([&e]() {
			auto p = e.getComponent<PointParticles>();
			while (true) {
				std::cout << "enter rgb : ";
				__IMPL__readColorFromStream(std::cin, p);
			}
		});
		t.detach();
	}

	void ReadColorFromFile(Entity& e, std::any fileName){
		std::thread t([&e, arg = std::move(fileName)](){
			auto p = e.getComponent<PointParticles>();
			auto fileName = std::any_cast<std::string>(arg);
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
}
