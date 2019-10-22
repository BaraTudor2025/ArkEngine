#pragma once

#include "Engine.hpp"

#include <fstream>

namespace GeneralScripts
{
	class RegisterMousePath : public Script {
		std::vector<sf::Vector2f> path;
		std::string file;
	public:

		RegisterMousePath(std::string file) : file(file) { }

		void update()
		{
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
				path.push_back(ArkEngine::mousePositon());
		}

		~RegisterMousePath()
		{
			if (path.empty())
				return;
			std::ofstream fout(file);
			fout << path.size() << ' ';
			for (auto[x, y] : path)
				fout << x << ' ' << y << ' ';
		}
	};
}
