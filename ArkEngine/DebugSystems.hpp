#pragma once

#include <thread>

#include "Engine.hpp"
#include "Util.hpp"
#include "ResourceManager.hpp"

class FpsCounterSystem : public System, public Renderer {
	sf::Time updateElapsed;
	sf::Text text;
	int updateFPS = 0;

public:

	FpsCounterSystem() :System(typeid(FpsCounterSystem)) {
		text.setFont(*Resources::load<sf::Font>("KeepCalm-Medium.ttf"));
		text.setCharacterSize(15);
		text.setFillColor(sf::Color::White);
	}


private:
	void update() override {
		updateElapsed += ArkEngine::deltaTime();
		updateFPS += 1;
		if (updateElapsed.asMilliseconds() >= 1000) {
			updateElapsed -= sf::milliseconds(1000);
			//log("fps: %d", updateFPS);
			text.setString("FPS:" + std::to_string(updateFPS));
			updateFPS = 0;
		}
	}

	void render(sf::RenderTarget& target) override {
		target.draw(text);
	}
};

class DebugSystem : public System {

public:
	DebugSystem() : System(typeid(DebugSystem)) { }

protected:
	sf::Time timeBetweenDisplays;
	bool pressEnterToDisplay = true;

private:
	sf::Time elapsedTime;
	std::once_flag flag;

	virtual void displayInfo() = 0;

	void update() override
	{
		if (pressEnterToDisplay) {
			auto spawnThread = [this]() { 
				std::thread t([this]() { while (true) { std::cin.get(); displayInfo(); }});
				t.detach();
			};
			std::call_once(flag, spawnThread);
		} else {
			elapsedTime += ArkEngine::deltaTime();
			if (elapsedTime > timeBetweenDisplays) {
				displayInfo();
				elapsedTime -= timeBetweenDisplays;
			}
		}
	}
};

class DebugParticleSystem : public DebugSystem {

	DebugParticleSystem() { this->pressEnterToDisplay = true; }

	void displayInfo() {
		//std::cout << "number of particle components " << getComponents<PointParticles>().size() << std::endl;
		//for (auto& p : getComponents<PointParticles>()) {
		//	std::cout
		//		<< "\n count: " << p.count
		//		<< "\n applyTransform:" << p.applyTransform
		//		<< "\n fireworks: " << p.fireworks
		//		<< "\n spawn: " << p.spawn
		//		<< "\n emitter: " << p.emitter.x << ' ' << p.emitter.y
		//		<< "\n life time: " << p.lifeTime.asSeconds()
		//		<< std::endl;
		//		if (p.entity() == nullptr)
		//			std::cout << "\ninvalid entity";
		//		else {
		//			std::cout << "entity id: " << p.entity()->id() << std::endl;
		//			// cast tag to string?
		//			//std::cout << "entity id: " << p->entity()->tag << std::endl;
		//		}
		//}
	}
};

class DebugEntitySystem : public DebugSystem {
	DebugEntitySystem() { this->pressEnterToDisplay = true; }
	void displayInfo() {
		//std::cout << "number of entities " << getComponents<Entity>().size() << std::endl;
		//for (auto& e : getEntities()) {
		//	std::cout << "id: " << e->id() << '\n';
		//}
	}
};
