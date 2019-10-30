
#include "System.hpp"
#include "Entity.hpp"
#include "EntityManager.hpp"

#include <imgui.h>
#include <libs/tinyformat.hpp>

#if 0
template <typename T, typename F, typename F2>
void treeWithSeparators(std::string_view label, T& range, F getLabel, F2 renderElem)
{
	static std::vector<bool> viewSeparators;

	if (ImGui::TreeNode(label.data())) {
		if (viewSeparators.size() != range.size())
			viewSeparators.resize(range.size());

		int index = 0;
		for (auto& element : range) {

			if (index == 0 && viewSeparators.at(index)) {
				ImGui::Separator();
			} else if (viewSeparators.at(index) && viewSeparators.at(index - 1) == false) {
				ImGui::Separator();
			}

			auto label = getLabel(element);
			if (ImGui::TreeNode(label.c_str())) {
				viewSeparators.at(index) = true;
				float indent_w = 10;
				ImGui::Indent(indent_w);
				
				renderElem(element);

				if(viewSeparators.at(index))
					ImGui::Separator();

				ImGui::Unindent(indent_w);
				ImGui::TreePop();
			} else {
				viewSeparators.at(index) = false;
			}
			index += 1;
		}
		ImGui::TreePop();
	}
}

void SystemManager::renderInspector()
{
	auto getLabel = [](auto& system) {
		return tfm::format("%s: E(%d) C(%d)", system->name.data(), system->entities.size(), system->componentNames.size());
	};

	auto renderElem = [](auto& system) {
		ImGui::TextUnformatted("Components:");
		for (std::string_view name : system->componentNames) {
			ImGui::BulletText(name.data());
		}

		ImGui::TextUnformatted("Entities:");
		for (Entity e : system->entities) {
			ImGui::BulletText(e.getName().c_str());
		}
	};

	treeWithSeparators("Systems:", systems, getLabel, renderElem);
}
#endif

void SystemManager::renderInspector()
{
	if (ImGui::TreeNode("Systems:")) {
		for (auto& system : systems) {
			std::string text = tfm::format("%s: E(%d) C(%d)", system->name.data(), system->entities.size(), system->componentNames.size());
			if (ImGui::TreeNodeEx(text.c_str(), ImGuiTreeNodeFlags_CollapsingHeader)) {
				float indent_w = 10;
				ImGui::Indent(indent_w);

				ImGui::TextUnformatted("Components:");
				for (std::string_view name : system->componentNames) {
					ImGui::BulletText(name.data());
				}

				ImGui::TextUnformatted("Entities:");
				for (Entity e : system->entities) {
					ImGui::BulletText(e.getName().c_str());
				}
				ImGui::Unindent(indent_w);
				//ImGui::TreePop() // don't pop a Collapsing Header
			}
		}
		ImGui::TreePop();
	}
}

void EntityManager::renderInspector()
{
	if (ImGui::TreeNode("Entities:")) {
		for (auto& entity : entities) {
			ImGui::Separator();
			std::string text = tfm::format("%s: C(%d) S(%d)", entity.name.c_str(), entity.components.size(), 0);
			if (ImGui::TreeNode(text.c_str())) {
				float indent_w = 7;
				ImGui::Indent(indent_w);

				ImGui::TextUnformatted("Components:");
				for (auto& compData : entity.components) {
					auto type = componentManager.getTypeFromId(compData.id);
					ImGui::BulletText(Util::getNameOfType(type));
				}

				ImGui::Unindent(indent_w);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}
