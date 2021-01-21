#pragma once

#include <vector>
#include <functional>
#include <string>

#include "ark/gui/ImGui.hpp"
#include "ark/core/Engine.hpp"
#include "ark/util/Util.hpp"

namespace ark {

	class ImGuiLayer : public ark::State {
	public:
		ImGuiLayer(ark::MessageBus& mb) : State(mb) {}
		~ImGuiLayer() { ImGui::SFML::Shutdown(); }

		int getStateId() override { return 2; }

		void init() override;

		void handleMessage(const ark::Message& m) override
		{

		}
		
		void handleEvent(const sf::Event& event) override
		{
			ImGui::SFML::ProcessEvent(event);
		}

		void update() override
		{
			ImGui::SFML::Update(Engine::getWindow(), Engine::deltaTime());
		}

		// calls registered tabs
		void preRender(sf::RenderTarget& win) override;
		void render(sf::RenderTarget& win) override;
		void postRender(sf::RenderTarget& win) override;

	public:

		struct GuiTab {
			std::string name;
			std::function<void()> render;
		};

		static void addTab(GuiTab tab)
		{
			tabs.push_back(tab);
		}

		static void removeTab(std::string name)
		{
			std::erase_if(tabs, [name](const auto& tab) {return tab.name == name; });
		}

	private:
		static inline std::vector<GuiTab> tabs;
	};
}
