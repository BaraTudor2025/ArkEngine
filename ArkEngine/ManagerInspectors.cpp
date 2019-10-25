
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"

#include <imgui.h>
#include <libs/tinyformat.hpp>

void SystemManager::renderInspector()
{
	ImGui::TextUnformatted("Systems: ");
	for (auto& system : systems) {
		std::string text = tfm::format("%s: E(%d) C(%d)", system->name.data(), system->entities.size(), system->componentNames.size());
		if (ImGui::TreeNode(text.c_str())) {
			float indent_w = 10;
			ImGui::Indent(indent_w);

			ImGui::TextUnformatted("Components: ");
			for (std::string_view name : system->componentNames) {
				ImGui::BulletText(name.data());
			}

			ImGui::TextUnformatted("Entities: ");
			for (Entity e : system->entities) {
				ImGui::BulletText(e.getName().c_str());
			}

			ImGui::Unindent(indent_w);
			ImGui::TreePop();
		}
	}
}
