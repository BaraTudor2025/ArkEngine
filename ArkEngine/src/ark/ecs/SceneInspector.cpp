#include <libs/tinyformat.hpp>

#include "ark/ecs/Entity.hpp"
#include "ark/ecs/EntityManager.hpp"
#include "ark/ecs/Scene.hpp"
#include "ark/ecs/components/Transform.hpp"
#include "ark/ecs/System.hpp"
#include "ark/gui/ImGui.hpp"
#include "ark/ecs/SceneInspector.hpp"
#include <imgui_internal.h>

template <typename T, typename F, typename F2>
void treeWithSeparators(std::string_view label, const T& range, F getLabel, F2 renderElem)
{
	static std::vector<bool> viewSeparators;

	if (ImGui::TreeNode(label.data())) {
		if (viewSeparators.size() != range.size())
			viewSeparators.resize(range.size());

		int index = 0;
		for (const auto& element : range) {

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

namespace ark {
	bool _DrawVec2Control(std::string_view label, sf::Vector2f& values, float resetValue, float columnWidth);
	void _Transform_editor_render(int* widgetId, void* pValue);


	void SceneInspector::renderSystemInspector()
	{
		auto getLabel = [](const auto& usys) {
			const System* system = usys.get();
			auto count = system->getQuerry().isValid() ? system->getQuerry().getMask().count() : 0;
			return tfm::format("%s: E(%d) C(%d)", system->name.data(), system->getEntities().size(), count);
		};

		auto renderElem = [this](const auto& usys) {
			const System* system = usys.get();
			if (!system->getQuerry().isValid())
				return;
			ImGui::TextUnformatted("Components:");
			system->getQuerry().forComponents([](const ark::meta::Metadata& meta) {
				ImGui::BulletText(meta.name.c_str());
			});

			ImGui::TextUnformatted("Entities:");
			for (Entity e : system->getEntities()) {
				ImGui::BulletText(e.getComponent<TagComponent>().name.c_str());
			}
		};

		treeWithSeparators("Systems:", systemManager.getSystems(), getLabel, renderElem);
	}

#if 0
	void SceneInspector::renderSystemInspector()
	{
		if (ImGui::TreeNode("Systems:")) {
			for (const auto& uSys : registry.getSystems()) {
				const System* system = uSys.get();
				std::string text = tfm::format("%s: E(%d) C(%d)", system->name.data(), system->getEntities().size(), system->getComponentNames().size());
				if (ImGui::TreeNodeEx(text.c_str(), ImGuiTreeNodeFlags_CollapsingHeader)) {
					float indent_w = 10;
					ImGui::Indent(indent_w);

					ImGui::TextUnformatted("Components:");
					for (std::string_view name : system->getComponentNames()) {
						ImGui::BulletText(name.data());
					}

					ImGui::TextUnformatted("Entities:");
					for (const Entity e : system->getEntities()) {
						ImGui::BulletText(e.getName().c_str());
					}
					ImGui::Unindent(indent_w);
					//ImGui::TreePop() // don't pop a Collapsing Header
				}
			}
			ImGui::TreePop();
		}
	}
#endif

#if 0
	void SceneInspector::renderEntityInspector()
	{
		if (ImGui::TreeNode("Entities:")) {
			int i = 0;
			for (const auto& entity : mEntityManager->entities) {

				// skip free/unused entities
				if (auto it = std::find(mEntityManager->freeEntities.begin(), mEntityManager->freeEntities.end(), i); it != mEntityManager->freeEntities.end()) {
					i += 1;
					continue;
				}
				ImGui::Separator();

				const std::string text = tfm::format("%s: C(%d)", entity.name.c_str(), entity.components.size());
				if (ImGui::TreeNode(text.c_str())) {
					const float indent_w = 7;
					ImGui::Indent(indent_w);

					ImGui::TextUnformatted("Components:");
					for (const auto& compData : entity.components) {
						const auto type = registry.componentTypeFromId(compData.id);
						ImGui::BulletText(Util::getNameOfType(type));
					}

					ImGui::Unindent(indent_w);
					ImGui::TreePop();
				}
				i += 1;
			}
			ImGui::TreePop();
		}
	}
#endif

	bool AlignButtonToRight(const char* str, std::function<void()> callback)
	{
		const ImVec2 typeNameSize = ImGui::GetItemRectSize();
		const ImVec2 buttonTextSize = ImGui::CalcTextSize(str);
		const float width = ImGui::GetContentRegionAvailWidth();
		ImGui::SameLine(0, width - typeNameSize.x - buttonTextSize.x + 20);

		ImGui::PushID(&str);
		bool pressed;
		if (pressed = ImGui::Button(str))
			callback();
		ImGui::PopID();
		return pressed;
	}

	std::string getNameOfEntity(ark::Entity entity)
	{
		if (auto tag = entity.tryGetComponent<TagComponent>())
			return tag->name;
		else
			return std::string("entity_") + std::to_string(entity.getID());
	}

	void SceneInspector::renderEntityEditor()
	{
		const int windowHeight = 700;
		const int windowWidth = 600;
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
		static bool _open = true;

		if (ImGui::Begin("Entity editor", &_open)) {

			// select an entity from list
			static int selectedEntity = -1;
			ImGui::BeginChild("ark_entity_editor_left_pane", ImVec2(150, 0), true);
			for (const auto entity : entityManager.entitiesView()) {
				auto name = getNameOfEntity(entity);
				if (ImGui::Selectable(name.c_str(), selectedEntity == entity.getID()))
					selectedEntity = entity.getID();
			}
			ImGui::EndChild();
			ImGui::SameLine();

			// edit selected entity
			ImGui::BeginChild("ark_entity_editor_right_pane", ImVec2(0, 0), false);
			if (selectedEntity != -1) {
				auto entity = entityManager.entityFromId(selectedEntity);

				int widgetId = 0;
				ImGui::TextUnformatted("Components:");
				// edit component
				for (ark::RuntimeComponent component : entity.runtimeComponentView()) {
					if (auto render = ark::meta::getService<bool(int*, void*)>(component.type, serviceName)) {
						ImGui::AlignTextToFramePadding();
						const auto* mdata = ark::meta::getMetadata(component.type);
						auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
						if (ImGui::TreeNodeEx(mdata->name.c_str(), flags)) {

							// can't delete Tag or Transform
							if (component.type != typeid(TagComponent) && component.type != typeid(ark::Transform)) {
								AlignButtonToRight("remove component", [&]() {
									entityManager.safeRemoveComponent(entity, type);
								});
							}
							if (component.type == typeid(ark::Transform))
								_Transform_editor_render(&widgetId, component.ptr);
							else
								render(&widgetId, component.ptr);
							ImGui::TreePop();
						}
					}
					ImGui::Separator();
				}

				// add components list
				auto componentGetter = [](void* data, int index, const char** out_text) -> bool {
					const auto& types = *static_cast<const std::vector<std::type_index>*> (data);
					*out_text = ark::meta::getMetadata(types.at(index))->name.c_str();
					return true;
				};
				int componentItemIndex;
				const auto& types = entityManager.getComponentTypes();
				if (ImGui::Combo("add_component", &componentItemIndex, componentGetter, (void*)(&types), types.size())) {
					Entity e = entityManager.entityFromId(selectedEntity);
					e.addComponent(types.at(componentItemIndex));
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	bool renderTransformField(std::string_view label1, std::string_view label2, ImVec2 buttonSize, float& value, float resetValue, ImVec4 color) {
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ color.x + .1f, color.y + .1f, color.z + .1f, color.w });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
		//ArkAlign(label1, 0.20);
		if (ImGui::Button(label1.data(), buttonSize))
			value = resetValue;
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		bool changed = ImGui::DragFloat(label2.data(), &value, std::abs(value) > 1 ? 0.1 : 0.001, 0, 0, "%.2f");
		ImGui::PopItemWidth();
		return changed;
	}

	bool _DrawVec2Control(std::string_view label, sf::Vector2f& values, float resetValue = 0, float columnWidth = 150) {
		//ArkSetFieldName(label);
		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.data());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFrameHeight();
		ImVec2 buttonSize = { lineHeight + 3, lineHeight };
		
		bool changed = renderTransformField("X", "##X", buttonSize, values.x, resetValue, ImVec4{ .8f, .1f, .15f, 1.f });
		ImGui::SameLine();
		changed = changed || renderTransformField("Y", "##Y", buttonSize, values.y, resetValue, ImVec4{ .2f, .7f, .3f, 1.f });

		ImGui::PopStyleVar();
		ImGui::Columns(1);
		return changed;
	}

	void _Transform_editor_render(int* widgetId, void* pValue) 
	{
		ark::Transform& trans = *static_cast<ark::Transform*>(pValue);
		ImGui::PushID(*widgetId);
		auto pos = trans.getPosition();
		if(_DrawVec2Control("Position", pos))
			trans.setPosition(pos);
		ImGui::PopID();
		*widgetId += 1;

		ImGui::PushID(*widgetId);
		auto scale = trans.getScale();
		if(_DrawVec2Control("Scale", scale))
			trans.setScale(scale);
		ImGui::PopID();
		*widgetId += 1;

		ImGui::PushID(*widgetId);
		auto orig = trans.getOrigin();
		if(_DrawVec2Control("Origin", orig))
			trans.setOrigin(orig);
		ImGui::PopID();
		*widgetId += 1;

		// TODO: rotation
	}

	void ArkAlign(std::string_view str, float widthPercentage)
	{
		ImVec2 textSize = ImGui::CalcTextSize(str.data());
		float width = ImGui::GetContentRegionAvailWidth();
		ImGui::SameLine(0, width * widthPercentage - textSize.x);
		ImGui::SetNextItemWidth(-1);
	}

	void ArkSetFieldName(std::string_view name)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(name.data());
		ArkAlign(name, 0.5);
	}

	bool ArkSelectable(const char* label, bool selected)
	{
		return ImGui::Selectable(label, selected);
	}

	void ArkFocusHere(int i = -1)
	{
		ImGui::SetKeyboardFocusHere(i);
	}

	std::unordered_map<std::type_index, SceneInspector::RenderPropFunc> SceneInspector::sPropertyRendererTable = {

		{ typeid(int), [](std::string_view name, const void* pField) {
			int field = *static_cast<const int*>(pField);
			ArkSetFieldName(name);
			if (ImGui::InputInt("", &field, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
				ArkFocusHere();
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(float), [](std::string_view name, const void* pField) {
			float field = *static_cast<const float*>(pField);
			ArkSetFieldName(name);
			if (ImGui::InputFloat("", &field, 0, 0, 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
				ArkFocusHere();
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(bool), [](std::string_view name, const void* pField) {
			bool field = *static_cast<const bool*>(pField);
			ArkSetFieldName(name);
			if (ImGui::Checkbox("", &field)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(std::string), [](std::string_view name, const void* pField) {
			std::string field = *static_cast<const std::string*>(pField);
			char buff[40];
			std::memset(buff, 0, sizeof(buff));
			strcpy_s(buff, field.c_str());
			ArkSetFieldName(name);
			if (ImGui::InputText("", buff, sizeof(buff), ImGuiInputTextFlags_EnterReturnsTrue)) {
				return std::any{std::string(buff, std::strlen(buff))};
			}
			return std::any{};
		} },

		{ typeid(char), [](std::string_view name, const void* pField) {
			char field = *static_cast<const char*>(pField);
			char buff[2] = { field, 0};
			ArkSetFieldName(name);
			if (ImGui::InputText("", buff, 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
				return std::any{buff[0]};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2f), [](std::string_view name, const void* pField) {
			sf::Vector2f vec = *static_cast<const sf::Vector2f*>(pField);
			ArkSetFieldName(name);
			float v[2] = {vec.x, vec.y};
			if (ImGui::InputFloat2("", v, 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
				vec.x = v[0];
				vec.y = v[1];
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2u), [](std::string_view name, const void* pField) {
			sf::Vector2u vec = *static_cast<const sf::Vector2u*>(pField);
			ArkSetFieldName(name);
			int v[2] = {vec.x, vec.y};
			if (ImGui::InputInt2("", v, ImGuiInputTextFlags_EnterReturnsTrue)) {
				vec.x = v[0];
				vec.y = v[1];
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::Color), [](std::string_view name, const void* pField) {
			sf::Color color = *static_cast<const sf::Color*>(pField);
			ArkSetFieldName(name);
			float v[4] = {
				static_cast<float>(color.r) / 255.f,
				static_cast<float>(color.g) / 255.f,
				static_cast<float>(color.b) / 255.f,
				static_cast<float>(color.a) / 255.f
			};

			if (ImGui::ColorEdit4("", v)) {
				color.r = static_cast<uint8_t>(v[0] * 255.f);
				color.g = static_cast<uint8_t>(v[1] * 255.f);
				color.b = static_cast<uint8_t>(v[2] * 255.f);
				color.a = static_cast<uint8_t>(v[3] * 255.f);
				return std::any{color};
			}
			return std::any{};
		} },

		{ typeid(sf::Time), [](std::string_view name, const void* pField) {
			sf::Time time = *static_cast<const sf::Time*>(pField);
			std::string label = std::string(name) + " (as seconds)";
			ArkSetFieldName(label);
			float sec = time.asSeconds();
			if (ImGui::InputFloat("", &sec, 0, 0, 3, ImGuiInputTextFlags_EnterReturnsTrue)) {
				return std::any{sf::seconds(sec)};
			}
			return std::any{};
		} }
	};
}
