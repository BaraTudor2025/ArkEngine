#include "ark/gui/Gui.hpp"
#include "ark/core/Logger.hpp"
#include "ark/util/ResourceManager.hpp"

namespace ark {

	sf::Color levelToColor(LogLevel level)
	{
		switch (level) {

		case LogLevel::Info:
			return sf::Color::Cyan;

		case LogLevel::Warning:
			return sf::Color::Yellow;

		case LogLevel::Error:
			return sf::Color::Red;

		case LogLevel::Critical:
			return sf::Color::Red;

		case LogLevel::Debug:
			return sf::Color::Blue;

		default:
			break;
		}
	}

	std::string_view levelToString(LogLevel level)
	{
		switch (level) {

		case LogLevel::Info:
			return "info";

		case LogLevel::Warning:
			return "warning";

		case LogLevel::Error:
			return "error";

		case LogLevel::Critical:
			return "critical";

		case LogLevel::Debug:
			return "debug";

		default:
			break;
		}
	}

	std::string_view sourceToString(LogSource source)
	{
		switch (source) {

		case LogSource::EntityM:
			return "Entity M";

		case LogSource::SystemM:
			return "Sys M";

		case LogSource::ScriptM:
			return "Script M";

		case LogSource::ComponentM:
			return "Comp M";

		case LogSource::ResourceM:
			return "Resource M";

		case LogSource::StateStack:
			return "State Stack";

		case LogSource::MessageBus:
			return "Message Bus";

		case LogSource::Message:
			return "Message";

		case LogSource::Registry:
			return "Registry";

		case LogSource::Engine:
			return "Engine";

		default:
			break;
		}
	}

	struct EngineLogger final {

		static void render()
		{
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
			ImGui::BeginChild("EngineLoggerScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));

			for (const auto& data : engineLogData) {
				ImGui::TextUnformatted("["); ImGui::SameLine(0, 0);
				ImGui::TextColored(sf::Color::Green, "%s", sourceToString(data.source).data()); ImGui::SameLine(0, 0);
				ImGui::Text("] ["); ImGui::SameLine(0, 0);
				ImGui::TextColored(levelToColor(data.level), "%s", levelToString(data.level).data()); ImGui::SameLine(0, 0);
				ImGui::Text("] %s", data.text.c_str());
			}

			ImGui::PopStyleVar();
			ImGui::EndChild();
		}

		static inline std::vector<EngineLogData> engineLogData;
	};

	void InternalEngineLog(EngineLogData data)
	{
#if USE_NATIVE_CONSOLE
		std::string text = tfm::format("[%s] [%s]: %s\n", sourceToString(data.source).data(), levelToString(data.level).data(), data.text.c_str());
		std::cout << text;
#else
		EngineLogger::engineLogData.push_back(std::move(data));
#endif
	}



	struct GameLogger final {

		static void render()
		{
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
			ImGui::BeginChild("EngineLoggerScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));

			ImGui::TextUnformatted(buffer.c_str());

			ImGui::PopStyleVar();
			ImGui::EndChild();
		}

		static inline std::string buffer;
	};

	void InternalGameLog(std::string text)
	{
#if USE_NATIVE_CONSOLE
		std::cout << "[GameLog]: " << text << '\n';
#else
		GameLogger::buffer.append(text + '\n');
#endif
	}



	// also deals with stderr
	struct StdOutLogger final {

		inline static std::stringstream ssout;
		inline static std::stringstream sserr;

		inline static std::streambuf* stdout_buf;
		inline static std::streambuf* stderr_buf;

		static void render()
		{
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
			ImGui::BeginChild("StdOutScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));

			if (ssout.rdbuf()->in_avail() != 0) {
				text.append("\n[stdout]:\n");
				text.append(ssout.str());

				ssout.str("");
				ssout.clear();
			}

			if (sserr.rdbuf()->in_avail() != 0) {
				text.append("\n[stderr]:\n");
				text.append(sserr.str());

				sserr.str("");
				sserr.clear();
			}

			ImGui::TextUnformatted(text.c_str());

			ImGui::PopStyleVar();
			ImGui::EndChild();
		}

		inline static std::string text;
	};

	static ImFont* arkFont;

	void SetDarkColors();

	void ImGuiLayer::init()
	{
		ImGui::SFML::Init(Engine::getWindow());
#if not USE_NATIVE_CONSOLE
		// redirecting stdout and stderr to stringstream to print the output to the gui console
		StdOutLogger::stdout_buf = std::cout.rdbuf();
		StdOutLogger::stderr_buf = std::cerr.rdbuf();
		std::cout.set_rdbuf(StdOutLogger::ssout.rdbuf());
		std::cerr.set_rdbuf(StdOutLogger::sserr.rdbuf());

		tabs.push_back({"Engine Log", EngineLogger::render});
		tabs.push_back({"Game Log", GameLogger::render});
		tabs.push_back({"stdout", StdOutLogger::render});
#endif
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& imguiStyle = ImGui::GetStyle();
		imguiStyle.ChildRounding = 3;
		imguiStyle.FrameRounding = 3;
		imguiStyle.GrabRounding = 2;
		imguiStyle.AntiAliasedFill = true;
		imguiStyle.AntiAliasedLines = true;

		ImGui::StyleColorsDark();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		//ImGuiStyle& style = ImGui::GetStyle();
		//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		//{
		//	style.WindowRounding = 0.0f;
		//	style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		//}

		SetDarkColors();

		std::string file = Resources::resourceFolder + "fonts/OpenSans-Regular.ttf";
		io.FontDefault = io.Fonts->AddFontFromFileTTF(file.c_str(), 20);

		ImGui::SFML::UpdateFontTexture();
	}

	void SetDarkColors() {
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 0.98f };

		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}

	void ImGuiLayer::preRender(sf::RenderTarget&)
	{
		ImGui::Begin("MyWindow");
		if (ImGui::BeginTabBar("GameTabBar")) {
			for (const auto& tab : tabs) {
				if (ImGui::BeginTabItem(tab.name.c_str())) {
					tab.render();
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void ImGuiLayer::render(sf::RenderTarget& win)
	{
		ImGui::SFML::Render(win);
	}

	void ImGuiLayer::postRender(sf::RenderTarget& win)
	{
	}

}
