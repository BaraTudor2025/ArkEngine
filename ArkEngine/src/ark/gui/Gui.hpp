#pragma once

#include <vector>
#include <functional>
#include <string>

#include "ark/gui/ImGui.hpp"

#include "ark/util/Util.hpp"

namespace ark {

	class InternalGui final {

	public:

		struct GuiTab {
			std::string name;
			std::function<void()> render;
		};

		static void init();

		static void addTab(GuiTab tab)
		{
			tabs.push_back(tab);
		}

		static void removeTab(std::string name)
		{
			Util::erase_if(tabs, [name](const auto& tab) {return tab.name == name; });
		}

		static void render();

	private:
		static inline std::vector<GuiTab> tabs;

	};
}
