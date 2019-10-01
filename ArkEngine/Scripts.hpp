#pragma once

#include "Engine.hpp"

#include <fstream>

namespace GeneralScripts
{
	template <typename T>
	class ReadVarFromConsole : public Script {
		T* var;
		std::string prompt;
	public:
		ReadVarFromConsole(T* var, std::string p) : prompt(p), var(var) { }

		void init()
		{
			std::thread t([prompt = prompt, &var = *var]() {
				while (true) {
					std::cout << prompt;
					std::cin >> var;
					std::cout << std::endl;
				}
			});
			t.detach();
		}
	};

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
