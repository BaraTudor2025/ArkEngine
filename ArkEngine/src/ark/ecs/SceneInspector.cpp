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
			system->getQuerry().forComponentTypes([](std::type_index type) {
				auto* meta = ark::meta::getMetadata(type);
				ImGui::BulletText(meta->name.c_str());
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

	bool SceneInspector::renderPropertiesOfType(std::type_index type, int* widgetId, void* pValue)
	{
		auto* mdata = ark::meta::getMetadata(type);
		const auto* options = mdata->data<VectorOptions>(serviceOptions);
		auto newValue = std::any{};
		bool modified = false; // used in recursive call to check if the property was modified

		for(const auto& property : mdata->prop()) {
			ImGui::PushID(*widgetId);

			if (ark::meta::hasProperties(property.type)) {
				// recursively render members that are registered
				std::any propValue = property.get(pValue);
				ImGui::Text("--%s:", property.name.data());
				if (renderPropertiesOfType(property.type, widgetId, property.fromAny(propValue))) {
					property.set(pValue, propValue);
					modified = true;
				}
			}
			else if (property.isEnum) {
				std::any propValue = property.get(pValue);
				const auto& fields = meta::getEnumValues(property.type);
				auto enumVal = *static_cast<ark::meta::EnumValue::value_type*>(property.fromAny(propValue));
				const char* fieldName = meta::getNameOfEnumValue(property.type, enumVal).data();

				// render enum values in a list
				ArkSetFieldName(property.name);
				if (ImGui::BeginCombo("", fieldName, 0)) {
					for (const auto& field : *fields) {
						if (ArkSelectable(field.name.data(), property.isEqual(propValue, &field.value))) {
							propValue = field.value;
							modified = true;
							property.set(pValue, propValue);
							break;
						}
					}
					ImGui::EndCombo();
				}
			}
			else {
				// render field using predefined table
				auto& renderProperty = sPropertyRendererTable.at(property.type);

				// find custom editor options for field
				auto editopt = EditorOptions{};
				if (options) {
					for (auto& opt : *options)
						if (opt.property_name == property.name)
							editopt = opt;
				}
				std::any local = property.get(pValue);
				newValue = renderProperty(property.name, property.fromAny(local), editopt);

				if (newValue.has_value()) {
					property.set(pValue, newValue);
					modified = true;
				}
				newValue.reset();
			}
			ImGui::PopID();
			*widgetId += 1;
		}
		return modified;
	}

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

	void SceneInspector::renderEntityEditor()
	{
		const int windowHeight = 700;
		const int windowWidth = 600;
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
		static bool _open = true;

		if (ImGui::Begin("Entity editor", &_open)) {

			// select an entity from list
			static ark::Entity selectedEntity;
			ImGui::BeginChild("ark_entity_editor_left_pane", ImVec2(150, 0), true);
			for (const auto entity : entityManager.entitiesView()) {
				const auto& name = entity.getComponent<TagComponent>().name;
				if (ImGui::Selectable(name.c_str(), selectedEntity == entity))
					selectedEntity = entity;
			}
			ImGui::EndChild();
			ImGui::SameLine();

			// edit selected entity
			ImGui::BeginChild("ark_entity_editor_right_pane", ImVec2(0, 0), false);
			if (selectedEntity.isValid()) {

				int widgetId = 0;
				ImGui::TextUnformatted("Components:");
				// edit component
				for (ark::RuntimeComponent component : selectedEntity.runtimeComponentView()) {
					ImGui::AlignTextToFramePadding();
					const auto* mdata = ark::meta::getMetadata(component.type);
					auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
					if (ImGui::TreeNodeEx(mdata->name.c_str(), flags)) {
						// can't delete Tag or Transform
						if (component.type != typeid(TagComponent) && component.type != typeid(ark::Transform)) {
							AlignButtonToRight("remove component", [&]() {
								entityManager.safeRemoveComponent(selectedEntity, component.type);
							});
						}
						// maybe use custom function
						if (auto render = mdata->func<bool(int*, void*)>(serviceName))
							render(&widgetId, component.ptr);
						else if(ark::meta::hasProperties(component.type))
							renderPropertiesOfType(component.type, &widgetId, component.ptr);
						ImGui::TreePop();
					}
					ImGui::Separator();
				}

				// list of components that can be added
				const auto& types = entityManager.getComponentTypes();
				auto componentGetter = [](void* data, int index, const char** out_text) -> bool {
					using CompVecT = std::decay_t<decltype(std::declval<Registry>().getComponentTypes())>;
					const auto& types = *static_cast<const CompVecT*> (data);
					*out_text = ark::meta::getMetadata(types.at(index))->name.c_str();
					return true;
				};
				int componentItemIndex;
				if (ImGui::Combo("add_component", &componentItemIndex, componentGetter, (void*)(&types), types.size())) {
					selectedEntity.addComponent(types.at(componentItemIndex));
				}
			}
			else {
				selectedEntity = {};
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

		{ typeid(int), [](std::string_view name, const void* pField, EditorOptions opt) {
			int field = *static_cast<const int*>(pField);
			ArkSetFieldName(name);
			if (ImGui::DragInt("", &field, opt.drag_speed, opt.drag_min, opt.drag_max)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(float), [](std::string_view name, const void* pField, EditorOptions opt) {
			float field = *static_cast<const float*>(pField);
			ArkSetFieldName(name);
			if (ImGui::DragFloat("", &field, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(bool), [](std::string_view name, const void* pField, EditorOptions opt) {
			bool field = *static_cast<const bool*>(pField);
			ArkSetFieldName(name);
			if (ImGui::Checkbox("", &field)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(std::string), [](std::string_view name, const void* pField, EditorOptions opt) {
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

		{ typeid(char), [](std::string_view name, const void* pField, EditorOptions opt) {
			char field = *static_cast<const char*>(pField);
			char buff[2] = { field, 0};
			ArkSetFieldName(name);
			if (ImGui::InputText("", buff, 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
				return std::any{buff[0]};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2f), [](std::string_view name, const void* pField, EditorOptions opt) {
			sf::Vector2f vec = *static_cast<const sf::Vector2f*>(pField);
			ArkSetFieldName(name);
			if (ImGui::DragFloat2("", &vec.x, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2i), [](std::string_view name, const void* pField, EditorOptions opt) {
			sf::Vector2i vec = *static_cast<const sf::Vector2i*>(pField);
			ArkSetFieldName(name);
			if (ImGui::DragInt2("", &vec.x, opt.drag_speed, opt.drag_min, opt.drag_max)) {
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::IntRect), [](std::string_view name, const void* pField, EditorOptions opt) {
			sf::IntRect rect = *static_cast<const sf::IntRect*>(pField);
			auto extname = std::string(name) + " L/T/W/H";
			ArkSetFieldName({ extname.c_str(), extname.size() });
			if (ImGui::DragInt4("", (int*)&rect, opt.drag_speed, opt.drag_min, opt.drag_max)) {
				return std::any{rect};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2u), [](std::string_view name, const void* pField, EditorOptions opt) {
			sf::Vector2u vec = *static_cast<const sf::Vector2u*>(pField);
			ArkSetFieldName(name);
			int v[2] = {vec.x, vec.y};
			if (ImGui::InputInt2("", v, ImGuiInputTextFlags_EnterReturnsTrue)) {
				vec.x = v[0] >= 0? v[0] : 0;
				vec.y = v[1] >= 0? v[1] : 0;
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::Color), [](std::string_view name, const void* pField, EditorOptions opt) {
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

		{ typeid(sf::Time), [](std::string_view name, const void* pField, EditorOptions opt) {
			sf::Time time = *static_cast<const sf::Time*>(pField);
			std::string label = std::string(name) + " (as seconds)";
			ArkSetFieldName(label);
			float sec = time.asSeconds();
			if (ImGui::DragFloat("", &sec, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				return std::any{sf::seconds(sec)};
			}
			return std::any{};
		} }
	};
}
