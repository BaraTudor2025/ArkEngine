#include "VectorEngine.hpp"
#include "Ecs.hpp"

// TODO: luat systemele ca parametrii
void VectorEngine::run()
{
	for (auto e : Entity::entities)
		for (auto& c : e->components)
			c->addThisToSystem();

	for (auto s : Script::scripts)
		s->init();

	appStarted = true;

	sf::Clock clock;
	while (window.isOpen()) {
		sf::Event ev;
		while (window.pollEvent(ev)) {
			sf::Vector2f mousePos = mouseCoords(window);
			switch (ev.type) {
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::Resized: {
				auto[x, y] = window.getSize();
				auto aspectRatio = float(x) / float(y);
				view.setSize({ width / aspectRatio, height / aspectRatio });
			}
				break;
			case sf::Event::MouseButtonPressed:
				if (ev.mouseButton.button == sf::Mouse::Left)
					for (auto s : Script::scripts)
						s->onMouseLeftPress(mousePos);
				if (ev.mouseButton.button == sf::Mouse::Right)
					for (auto s : Script::scripts)
						s->onMouseRightPress(mousePos);
				break;
			case sf::Event::MouseButtonReleased:
				if (ev.mouseButton.button == sf::Mouse::Left)
					for (auto s : Script::scripts)
						s->onMouseLeftRelease();
				if (ev.mouseButton.button == sf::Mouse::Right)
					for (auto s : Script::scripts)
						s->onMouseRightRelease();
				break;
			default:
				break;
			}
		}
		window.clear();
		sf::Vector2f mousePos = mouseCoords(window);

		sf::Time deltaTime = clock.restart();

		for (auto script : Script::scripts)
			script->update(deltaTime, mousePos);

		for (auto system : System::systems)
			system->update(deltaTime, mousePos);

		for (auto system : System::systems)
			system->draw(window);

		window.display();
	}
}
