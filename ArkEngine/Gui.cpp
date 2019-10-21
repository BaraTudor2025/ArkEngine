#include "Gui.hpp"
#include "Logger.hpp"

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
		return "System M";

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

struct Logger final {

	static void Log(EngineLogData info)
	{
		engineLogData.push_back(std::move(info));
	}

	static void render()
	{
		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,3)); // Tighten spacing

		for (const auto& data : engineLogData) {
			ImGui::TextUnformatted("["); ImGui::SameLine(0, 0);
			ImGui::TextColored(sf::Color::Green, "%s", sourceToString(data.source).data()); ImGui::SameLine(0, 0);
			ImGui::Text("] ["); ImGui::SameLine(0,0);
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
	Logger::Log(std::move(data));
}

void InternalGui::init()
{
	//GuiTab consoleTab;
	//consoleTab.name = "Console";
	//consoleTab.render = [&]() {
	//
	//};
	//tabs.push_back(consoleTab);

	//GuiTab componentEditor;
	//componentEditor.name = "Component Editor";
	//tabs.push_back(componentEditor);

	GuiTab managersInspector;
	managersInspector.name = "Managers Inspector";

	GuiTab engineLogger;
	engineLogger.name = "Engine Log";
	engineLogger.render = Logger::render;
	tabs.push_back(engineLogger);
}

void InternalGui::render()
{
	ImGui::Begin("MyWindow");
	if (ImGui::BeginTabBar("MyTabBar")) {
		for (const auto& tab : tabs) {
			if (ImGui::BeginTabItem(tab.name.c_str())) {
				tab.render();
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
	//ImGui::ShowDemoWindow();
	//ImGui::Begin("Sample window"); // begin window

	//if (ImGui::ColorEdit3("Background color", color)) {
	//	// this code gets called if color value changes, so
	//	// the background color is upgraded automatically!
	//	bgColor.r = static_cast<sf::Uint8>(color[0] * 255.f);
	//	bgColor.g = static_cast<sf::Uint8>(color[1] * 255.f);
	//	bgColor.b = static_cast<sf::Uint8>(color[2] * 255.f);
	//}

	// Window title text edit
	//ImGui::InputText("Window title", windowTitle.data(), 20);

	//if (ImGui::Button("Update window title")) {
	// this code gets if user clicks on the button
	// yes, you could have written if(ImGui::InputText(...))
	// but I do this to show how buttons work :)
	//window.setTitle(windowTitle);
	//}
	//ImGui::End(); // end window
}
