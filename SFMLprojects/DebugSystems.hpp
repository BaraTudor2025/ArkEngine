#pragma once
#include <thread>
#include "VectorEngine.hpp"
#include "Util.hpp"
#include "ResourceManager.hpp"

class FpsCounterSystem : public System {
	sf::Time updateElapsed;
	sf::Text text;
	int updateFPS;

public:
	FpsCounterSystem(sf::Color color = sf::Color::White) {
		text.setFillColor(color);
	}

private:
	void init() {
		text.setFont(*load<sf::Font>("KeepCalm-Medium.ttf"));
		text.setCharacterSize(15);
	}

	void update() {
		updateElapsed += VectorEngine::deltaTime();
		updateFPS += 1;
		if (updateElapsed.asMilliseconds() >= 1000) {
			updateElapsed -= sf::milliseconds(1000);
			//log("fps: %d", updateFPS);
			text.setString("FPS:" + std::to_string(updateFPS));
			updateFPS = 0;
		}
	}

	void render(sf::RenderTarget& target) {
		target.draw(text);
	}
};

class DebugSystem : public System {
protected:
	sf::Time timeBetweenDisplays;
	bool pressEnterToDisplay = true;

private:
	sf::Time elapsedTime;
	std::once_flag flag;

	virtual void displayInfo() = 0;

	void update()
	{
		if (pressEnterToDisplay) {
			auto spawnThread = [this]() { 
				std::thread t([this]() { while (true) { std::cin.get(); displayInfo(); }});
				t.detach();
			};
			std::call_once(flag, spawnThread);
		} else {
			elapsedTime += VectorEngine::deltaTime();
			if (elapsedTime > timeBetweenDisplays) {
				displayInfo();
				elapsedTime -= timeBetweenDisplays;
			}
		}
	}

	void render(sf::RenderTarget&) override { }
};

class DebugParticleSystem : public DebugSystem {

	void init() { this->pressEnterToDisplay = true; }

	void displayInfo() {
		std::cout << "number of particle components " << getComponents<Particles>().size() << std::endl;
		for (auto p : getComponents<Particles>()) {
			std::cout
				<< "\n count: " << p->count
				<< "\n applyTransform:" << p->applyTransform
				<< "\n fireworks: " << p->fireworks
				<< "\n spawn: " << p->spawn
				<< "\n emitter: " << p->emitter.x << ' ' << p->emitter.y
				<< "\n life time: " << p->lifeTime.asSeconds()
				<< std::endl;
				if (p->entity() == nullptr)
					std::cout << "\ninvalid entity";
				else {
					std::cout << "entity id: " << p->entity()->id() << std::endl;
					// cast tag to string?
					//std::cout << "entity id: " << p->entity()->tag << std::endl;
				}
		}
	}
};

class DebugEntitySystem : public DebugSystem {
	void init() { this->pressEnterToDisplay = true; }
	void displayInfo() {
		std::cout << "number of entities " << getComponents<Entity>().size() << std::endl;
		for (auto e : getEntities()) {
			std::cout << "id: " << e->id() << '\n';
		}
	}
};
