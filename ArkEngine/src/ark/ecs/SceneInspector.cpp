#include <libs/tinyformat.hpp>


#include "ark/ecs/Entity.hpp"
#include "ark/ecs/EntityManager.hpp"
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

	void SceneInspector::renderSystemInspector()
	{
		//std::unique_ptr<int, void(*)()> ptr(0, []);
		//auto ptr = std::make_unique<int, void(*)()>();
		//auto getLabel = [](const auto& usys) {
		//	const System* system = usys.get();
		//	auto count = system->getQuerry().isValid() ? system->getQuerry().getMask().count() : 0;
		//	return tfm::format("%s: E(%d) C(%d)", system->name.data(), system->getEntities().size(), count);
		//};

		//auto renderElem = [this](const auto& usys) {
		//	const System* system = usys.get();
		//	if (!system->getQuerry().isValid())
		//		return;
		//	ImGui::TextUnformatted("Components:");
		//	system->getQuerry().forComponentTypes([](std::type_index type) {
		//		auto* meta = ark::meta::resolve(type);
		//		ImGui::BulletText(meta->name.c_str());
		//	});

		//	ImGui::TextUnformatted("Entities:");
		//	for (Entity e : system->getEntities()) {
		//		ImGui::BulletText(e.getComponent<TagComponent>().name.c_str());
		//	}
		//};

		//treeWithSeparators("Systems:", systemManager.getSystems(), getLabel, renderElem);
	}

#if 0
	void SceneInspector::renderSystemInspector()
	{
		if (ImGui::TreeNode("Systems:")) {
			for (const auto& uSys : registry.getSystems()) {
				const System* system = uSys.get();
				std::string text = tfm::format("%s: E(%d) C(%d)", system->name.data(), system->getEntities().m_size(), system->getComponentNames().m_size());
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

				const std::string text = tfm::format("%s: C(%d)", entity.name.c_str(), entity.components.m_size());
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

	bool SceneInspector::renderPropertiesOfType(std::type_index type, int* widgetId, void* pInstance, std::type_index parentType, std::string_view thisPropertyName)
	{
		auto* mdata = ark::meta::resolve(type);
		// ma doare
		// TODO(meta): poate combin RuntimeProperty cu Metadata + ark::any care se ocupa de conversii automat(cu specializare pe int-enum)? 
		// asta ca sa fac codul asta mai clean, ew
		auto* options = [&] {
			if (parentType != typeid(void)) {
				// foloseste optiunile speciale pentru proprietatea asta
				if (const auto parent = ark::meta::resolve(parentType)) {
					if (const auto opts = parent->data<SceneInspector::VectorOptions>(SceneInspector::serviceOptions)) {
						if (const auto prop = parent->prop(thisPropertyName)) {
							auto it2 = std::find_if(opts->begin(), opts->end(), [&](const auto& opt) { return opt.property_name == prop->name; });
							if (it2 != opts->end()) {
								if (auto& opt = *it2; !opt.options.empty())
									return &opt.options;
							}
						}
					}
				}
			}
			// altfel folosestele pe cele din type-ul proprietatii
			return mdata->data<SceneInspector::VectorOptions>(SceneInspector::serviceOptions);
		}();

		bool modified = false; // used in recursive call to check if the property was modified

		for(const auto& property : mdata->prop()) {
			ImGui::PushID(*widgetId);

			if (ark::meta::hasProperties(property.type)) {
				// recursively render members that are registered
				std::any propValue = property.get(pInstance);
				ImGui::Text("--%s:", property.name.data());
				if (renderPropertiesOfType(property.type, widgetId, property.fromAny(propValue), type, property.name)) {
					property.set(pInstance, propValue);
					modified = true;
				}
			}
			else if (property.isEnum) {
				//std::any enumValue = property.get(pInstance);
				//auto enumType = ark::meta::resolve(property.type);
				//const char* preview = [&] { 
				//	for (auto [ename, evalue] : enumType->data())
				//		if (property.isEqual(pInstance, evalue))
				//		//if (evalue == property.fromAny(propValue))
				//			return ename;
				//	return "";
				//}();

				//// render enum values in a list
				//ArkSetFieldName(property.name);
				//if (ImGui::BeginCombo("", preview, 0)) {
				//	for (auto [ename, evalue] : enumType->data()) { // enumType->data() -> vector<{std::string_view name, ark::any value}>
				//		if (ImGui::Selectable(ename.data(), property.isEqual(pInstance, evalue))) {
				//			property.set(pInstance, evalue);
				//			modified = true;
				//			break;
				//		}
				//	}
				//	ImGui::EndCombo();
				//}
				std::any propValue = property.get(pInstance);
				const auto& values = meta::getEnumValues(property.type);
				auto previewValue = *static_cast<ark::meta::EnumValue::value_type*>(property.fromAny(propValue));
				const char* preview = meta::getNameOfEnumValue(property.type, previewValue).data();

				// render enum values in a list
				ArkSetFieldName(property.name);
				if (ImGui::BeginCombo("", preview, 0)) {
					for (const auto& field : *values) {
						if (ImGui::Selectable(field.name.data(), property.isEqual(propValue, &field.value))) {
							propValue = field.value;
							property.set(pInstance, propValue);
							modified = true;
							break;
						}
					}
					ImGui::EndCombo();
				}
			}
			else {
				// render field using predefined table
				auto& renderProperty = SceneInspector::s_renderPropertyTable.at(property.type);

				// find custom editor options for field
				auto defaultOpt = EditorOptions{};
				auto& editopt = [&]() -> auto& {
					if (options) {
						for (auto& opt : *options)
							if (opt.property_name == property.name)
								return opt;
					} 
					return defaultOpt;
				}(); 

				std::any local = property.get(pInstance);
				if (auto newValue = renderProperty(property.name, property.fromAny(local), editopt); newValue.has_value()) {
					property.set(pInstance, newValue);
					modified = true;
				}
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
		//ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
		static bool _open = true;

		if (ImGui::Begin("Entity editor", &_open)) {
			// select an entity from list
			static ark::Entity selectedEntity;
			ImGui::BeginChild("ark_entity_editor_left_pane", ImVec2(150, 0), true);
			if (ImGui::SmallButton("+ entity"))
				auto _ = entityManager.createEntity();
			for (const auto entity : entityManager.each()) {
				const auto& name = entity.get<ark::TagComponent>().name;
				if (ImGui::Selectable(name.c_str(), selectedEntity == entity))
					selectedEntity = entity;
			}
			ImGui::EndChild();
			ImGui::SameLine();

			// edit selected entity
			ImGui::BeginChild("ark_entity_editor_right_pane", ImVec2(0, 0), false);
			if (selectedEntity.isValid() && ImGui::SmallButton("delete")) {
				entityManager.destroyEntity(selectedEntity);
				selectedEntity.reset();
			}
			else if (selectedEntity.isValid()) {
				int widgetId = 0;
				ImGui::TextUnformatted("Components:");
				// edit component
				for (ark::RuntimeComponent component : selectedEntity.eachComponent()) {
					ImGui::AlignTextToFramePadding();
					const auto* mdata = ark::meta::resolve(component.type);
					auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
					if (ImGui::TreeNodeEx(mdata->name.c_str(), flags)) {
						// can't delete Tag or Transform
						bool deleted = false;
						if (component.type != typeid(TagComponent) && component.type != typeid(ark::Transform)) {
							deleted = AlignButtonToRight("remove component", [&]() {
								selectedEntity.remove(component.type);
							});
						}
						if (!deleted) {
							// maybe use custom function
							if (auto render = mdata->func<bool(int*, void*)>(serviceName))
								render(&widgetId, component.ptr);
							else if (ark::meta::hasProperties(component.type))
								renderPropertiesOfType(component.type, &widgetId, component.ptr);
						}
						ImGui::TreePop();
					}
					ImGui::Separator();
				}

				// list of components that can be added
				const auto& types = entityManager.getTypes();
				auto componentGetter = [](void* data, int index, const char** out_text) -> bool {
					using CompVecT = std::decay_t<decltype(std::declval<ark::EntityManager>().getTypes())>;
					const auto& types = *static_cast<const CompVecT*> (data);
					*out_text = ark::meta::resolve(types[index])->name.c_str();
					return true;
				};
				int componentItemIndex;
				if (ImGui::Combo("add_component", &componentItemIndex, componentGetter, (void*)(&types), types.size())) {
					selectedEntity.add(types[componentItemIndex]);
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void ArkSetFieldName(std::string_view name, EditorOptions* opt)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(name.data());

		if (opt && ImGui::IsItemHovered()) {
			opt->privateOpenPopUp = true;
		}
		if (opt && opt->privateOpenPopUp) {
			if (ImGui::BeginPopupContextWindow(name.data())) {

				auto max = opt->drag_max;
				if (ImGui::InputFloat("drag-max", &max, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					opt->drag_max = max;

				auto min = opt->drag_min;
				if (ImGui::InputFloat("drag-min", &min, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					opt->drag_min = min;
				
				auto speed = opt->drag_speed;
				if (ImGui::InputFloat("drag-speed", &speed, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					opt->drag_speed = speed;

				ImGui::EndPopup();
			}
			else {
				//if(!ImGui::IsPopupOpen(name.data()))
				opt->privateOpenPopUp = false;
			}
		}
		// align
		float widthPercentage = 0.5;
		ImVec2 textSize = ImGui::CalcTextSize(name.data());
		float width = ImGui::GetContentRegionAvailWidth();
		ImGui::SameLine(0, width * widthPercentage - textSize.x);
		ImGui::SetNextItemWidth(-1);
	}

	void ArkFocusHere(int i = -1)
	{
		ImGui::SetKeyboardFocusHere(i);
	}

	std::unordered_map<std::type_index, SceneInspector::RenderPropFunc> SceneInspector::s_renderPropertyTable = {

		{ typeid(int), [](std::string_view name, const void* pField, EditorOptions& opt) {
			int field = *static_cast<const int*>(pField);
			ArkSetFieldName(name, &opt);
			if (ImGui::DragInt("", &field, opt.drag_speed, opt.drag_min, opt.drag_max)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(float), [](std::string_view name, const void* pField, EditorOptions& opt) {
			float field = *static_cast<const float*>(pField);
			ArkSetFieldName(name, &opt);
			if (ImGui::DragFloat("", &field, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(bool), [](std::string_view name, const void* pField, EditorOptions& opt) {
			bool field = *static_cast<const bool*>(pField);
			ArkSetFieldName(name, &opt);
			if (ImGui::Checkbox("", &field)) {
				return std::any{field};
			}
			return std::any{};
		} },

		{ typeid(std::string), [](std::string_view name, const void* pField, EditorOptions& opt) {
			std::string field = *static_cast<const std::string*>(pField);
			char buff[40];
			std::memset(buff, 0, sizeof(buff));
			strcpy_s(buff, field.c_str());
			ArkSetFieldName(name, &opt);
			if (ImGui::InputText("", buff, sizeof(buff), ImGuiInputTextFlags_EnterReturnsTrue)) {
				return std::any{std::string(buff, std::strlen(buff))};
			}
			return std::any{};
		} },

		{ typeid(char), [](std::string_view name, const void* pField, EditorOptions& opt) {
			char field = *static_cast<const char*>(pField);
			char buff[2] = { field, 0};
			ArkSetFieldName(name, &opt);
			if (ImGui::InputText("", buff, 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
				return std::any{buff[0]};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2f), [](std::string_view name, const void* pField, EditorOptions& opt) {
			sf::Vector2f vec = *static_cast<const sf::Vector2f*>(pField);
			ArkSetFieldName(name, &opt);
			if (ImGui::DragFloat2("", &vec.x, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2i), [](std::string_view name, const void* pField, EditorOptions& opt) {
			sf::Vector2i vec = *static_cast<const sf::Vector2i*>(pField);
			ArkSetFieldName(name, &opt);
			if (ImGui::DragInt2("", &vec.x, opt.drag_speed, opt.drag_min, opt.drag_max)) {
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::IntRect), [](std::string_view name, const void* pField, EditorOptions& opt) {
			sf::IntRect rect = *static_cast<const sf::IntRect*>(pField);
			auto extname = std::string(name) + " L/T/W/H";
			ArkSetFieldName({ extname.c_str(), extname.size() }, &opt);
			if (ImGui::DragInt4("", (int*)&rect, opt.drag_speed, opt.drag_min, opt.drag_max)) {
				return std::any{rect};
			}
			return std::any{};
		} },

		{ typeid(sf::Vector2u), [](std::string_view name, const void* pField, EditorOptions& opt) {
			sf::Vector2u vec = *static_cast<const sf::Vector2u*>(pField);
			ArkSetFieldName(name, &opt);
			int v[2] = {(int)vec.x, (int)vec.y};
			if (ImGui::InputInt2("", v, ImGuiInputTextFlags_EnterReturnsTrue)) {
				vec.x = v[0] >= 0? v[0] : 0;
				vec.y = v[1] >= 0? v[1] : 0;
				return std::any{vec};
			}
			return std::any{};
		} },

		{ typeid(sf::Color), [](std::string_view name, const void* pField, EditorOptions& opt) {
			sf::Color color = *static_cast<const sf::Color*>(pField);
			ArkSetFieldName(name, &opt);
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

		{ typeid(sf::Time), [](std::string_view name, const void* pField, EditorOptions& opt) {
			sf::Time time = *static_cast<const sf::Time*>(pField);
			std::string label = std::string(name) + " (as seconds)";
			ArkSetFieldName(label, &opt);
			float sec = time.asSeconds();
			if (ImGui::DragFloat("", &sec, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				return std::any{sf::seconds(sec)};
			}
			return std::any{};
		} }
	};
}
