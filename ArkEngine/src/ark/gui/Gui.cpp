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

		case LogSource::Scene:
			return "Scene";

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

	void InternalGui::init()
	{
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
		ImGuiStyle& imguiStyle = ImGui::GetStyle();
		imguiStyle.ChildRounding = 3;
		imguiStyle.FrameRounding = 3;
		imguiStyle.GrabRounding = 2;
		imguiStyle.AntiAliasedFill = true;
		imguiStyle.AntiAliasedLines = true;

		ImGuiIO& io = ImGui::GetIO();
		std::string file = Resources::resourceFolder + "fonts/Inconsolata.otf";
		arkFont = io.Fonts->AddFontFromFileTTF(file.c_str(), 15);
		ImGui::SFML::UpdateFontTexture();
	}

	void InternalGui::render()
	{
		ImGui::Begin("MyWindow");
		ImGui::PushFont(arkFont);
		if (ImGui::BeginTabBar("GameTabBar")) {
			for (const auto& tab : tabs) {
				if (ImGui::BeginTabItem(tab.name.c_str())) {
					tab.render();
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
		ImGui::PopFont();
		ImGui::End();
	}
}
