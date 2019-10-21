#pragma once

#include <imgui.h>
#include <imgui-SFML.h>
#include <libs/tinyformat.hpp>

#include <vector>
#include <functional>
#include <string>

class InternalGui final {

public:

	struct GuiTab {
		std::string name;
		std::function<void()> render;
	};

	static void init();

	static void render();

private:
	static inline std::vector<GuiTab> tabs;

};
