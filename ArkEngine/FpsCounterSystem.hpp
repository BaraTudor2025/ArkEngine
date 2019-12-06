#pragma once

#include <thread>

#include <ark/core/Engine.hpp>
#include <ark/util/Util.hpp>
#include <ark/util/ResourceManager.hpp>
#include <ark/ecs/System.hpp>

class FpsCounterSystem : public ark::SystemT<FpsCounterSystem>, public ark::Renderer {
	sf::Time updateElapsed;
	sf::Text text;
	int updateFPS = 0;

public:
	FpsCounterSystem() {
		text.setFont(*ark::Resources::load<sf::Font>("KeepCalm-Medium.ttf"));
		text.setCharacterSize(15);
		text.setFillColor(sf::Color::White);
	}

private:
	void update() override {
		updateElapsed += ark::Engine::deltaTime();
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
