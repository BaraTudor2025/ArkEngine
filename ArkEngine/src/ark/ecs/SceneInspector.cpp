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

	// returns new value if any

	bool renderTypeEnumEditor(std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions*) {
		//auto type = ark::meta::resolve(info);

		// //std::any enumValue = property.get(pInstance);
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


		//std::any propValue = property.get(pInstance);
		const auto* enumFields = meta::getEnumValues(info);
		auto previewValue = *static_cast<ark::meta::EnumValue::value_type*>(instance);
		const char* preview = meta::getNameOfEnumValue(info, previewValue).data();

		// render enum values in a list
		ArkSetFieldName(label, nullptr);
		if (ImGui::BeginCombo("", preview, 0)) {
			for (const auto& field : *enumFields) {
				//if (ImGui::Selectable(field.name.data(), property.isEqual(propValue, &field.value))) {
				//	propValue = field.value;
				//	property.set(pInstance, propValue);
				//	modified = true;
				//	break;
				//}
			}
			ImGui::EndCombo();
		}
		return false;
	}

	bool renderTypePropertiesEditor(std::type_index info, void* instance, int* widgetID, std::string_view, EditorOptions*) 
	{
		auto* type = ark::meta::resolve(info);
		auto* typeOptions = type->data<EditorOptions>(RENDER_EDITOR_OPTIONS);

		bool modified = false;
		for (const auto& property : type->prop()) {

			if (property.isEnum) {
				std::any propValue = property.get(instance);
				const auto& values = meta::getEnumValues(property.type);
				auto previewValue = *static_cast<ark::meta::EnumValue::value_type*>(property.fromAny(propValue));
				const char* preview = meta::getNameOfEnumValue(property.type, previewValue).data();

				// render enum values in a list
				ArkSetFieldName(property.name, nullptr);
				ImGui::PushID(*widgetID);
				if (ImGui::BeginCombo("", preview, 0)) {
					for (const auto& field : *values) {
						if (ImGui::Selectable(field.name.data(), property.isEqual(propValue, &field.value))) {
							propValue = field.value;
							property.set(instance, propValue);
							modified = true;
							break;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopID();

			}
			else {
				std::any value = property.get(instance);
				auto propOptions = [&] {
					if (typeOptions) {
						for (auto& opt : typeOptions->options) {
							if (opt.property_name == property.name)
								return &opt;
						}
					}
					return (EditorOptions*)nullptr;
				}();

				auto mdataProp = ark::meta::resolve(property.type);
				if (auto edit = mdataProp->func<RenderTypeEditor>(RENDER_EDITOR)) {

					if(!mdataProp->prop().empty())
						ImGui::Text("--%s:", property.name.data());
					std::any value = property.get(instance);
					ImGui::PushID(*widgetID);
					if (edit(property.type, property.fromAny(value), widgetID, property.name, propOptions)) {
						property.set(instance, value);
						modified = true;
					}
					ImGui::PopID();
				}
			}
			*widgetID += 1;
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
			for (const ark::Entity entity : entityManager.each()) {
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
							if (auto edit = mdata->func<RenderTypeEditor>(RENDER_EDITOR)) {
								edit(component.type, component.ptr, &widgetId, "", nullptr);
							}
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
		// selected options
		static EditorOptions* c_options = nullptr;
		if (opt && ImGui::IsItemHovered())
			c_options = opt;
		if (c_options) {
			if (ImGui::BeginPopupContextWindow()) {

				auto* opt = c_options;
				auto min = opt->drag_min;
				if (ImGui::InputFloat("drag-min", &min, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					opt->drag_min = min;

				auto max = opt->drag_max;
				if (ImGui::InputFloat("drag-max", &max, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					opt->drag_max = max;
				
				auto speed = opt->drag_speed;
				if (ImGui::InputFloat("drag-speed", &speed, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					opt->drag_speed = speed;

				ImGui::EndPopup();
			}
			else {
				// un-select if Popup is closed
				if(!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
					c_options = nullptr;
			}
		}
		// align
		float widthPercentage = 0.5;
		ImVec2 textSize = ImGui::CalcTextSize(name.data());
		float width = ImGui::GetContentRegionAvailWidth();
		ImGui::SameLine(0, width * widthPercentage - textSize.x);
		ImGui::SetNextItemWidth(-1);
	}

	void SceneInspector::init() {

		// if the type doesn't have a renderer and has properties, then the default one calls the renderer for the properties
		ark::meta::forTypes([&](ark::meta::Metadata& type) {
			if (auto fun = type.func<RenderTypeEditor>(RENDER_EDITOR); !fun) {
				if (!type.prop().empty())
					type.func<RenderTypeEditor>(RENDER_EDITOR, renderTypePropertiesEditor);
			}
		});

		ark::meta::type<int>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			int& value = *static_cast<int*>(instance);
			auto& opt = *assureEditorOptions(typeid(value), parentOptions);
			ArkSetFieldName(label, &opt);
			if (opt.drag_speed == 0) {
				int copy = value;
				if (ImGui::InputInt("", &copy, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue | opt.flags)) {
					value = copy;
					return true;
				}
			}
			else {
				return ImGui::DragInt("", &value, opt.drag_speed, opt.drag_min, opt.drag_max);
			}
			return false;
		});

		ark::meta::type<float>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			float& value = *static_cast<float*>(instance);
			auto& opt = *assureEditorOptions(typeid(value), parentOptions);
			ArkSetFieldName(label, &opt);
			if (opt.drag_speed == 0) {
				float copy = value;
				if (ImGui::InputFloat("", &copy, 0, 0, opt.precision, ImGuiInputTextFlags_EnterReturnsTrue | opt.flags)) {
					value = copy;
					return true;
				}
			}
			else {
				return ImGui::DragFloat("", &value, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power);
			}
			return false;
		});

		ark::meta::type<bool>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			bool& value = *(bool*)instance;
			ArkSetFieldName(label, nullptr);
			return ImGui::Checkbox("", &value);
		});

		ark::meta::type<char>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			char& value = *static_cast<char*>(instance);
			char buff[2] = { value, 0 };
			ArkSetFieldName(label, nullptr);
			if (ImGui::InputText("", buff, sizeof(buff), ImGuiInputTextFlags_EnterReturnsTrue)) {
				value = buff[0];
				return true;
			}
			return false;
		});

		ark::meta::type<std::string>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			std::string& value = *static_cast<std::string*>(instance);
			char buff[50];
			std::memset(buff, 0, sizeof(buff));
			strcpy_s(buff, value.c_str());
			ArkSetFieldName(label, nullptr);
			if (ImGui::InputText("", buff, sizeof(buff), ImGuiInputTextFlags_EnterReturnsTrue)) {
				value.assign(buff);
				return true;
			}
			return false;
		});

		ark::meta::type<sf::Vector2f>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			auto& value = *(sf::Vector2f*)instance;
			auto& opt = *assureEditorOptions(typeid(value), parentOptions);
			ArkSetFieldName(label, &opt);
			//GameLog("name(%s) drag_speed(%d)", opt.property_name, opt.drag_speed);
			return ImGui::DragFloat2("", &value.x, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power);
		});

		ark::meta::type<sf::Vector2i>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			auto& value = *(sf::Vector2i*)instance;
			auto& opt = *assureEditorOptions(info, parentOptions);
			ArkSetFieldName(label, &opt);
			return ImGui::DragInt2("", &value.x, opt.drag_speed, opt.drag_min, opt.drag_max);
		});

		ark::meta::type<sf::Vector2u>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			auto& value = *(sf::Vector2i*)instance;
			auto& opt = *assureEditorOptions(info, parentOptions);
			ArkSetFieldName(label, &opt);
			sf::Vector2i vec = { value.x, value.y };
			if (ImGui::DragInt2("", &vec.x, opt.drag_speed, opt.drag_min < 0 ? 0 : opt.drag_min, opt.drag_max)) {
				value = {
					vec.x < 0 ? 0 : vec.x,
					vec.y < 0 ? 0 : vec.y
				};
				return true;
			}
			return false;
		});

		ark::meta::type<sf::IntRect>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			sf::IntRect& value = *(sf::IntRect*)(instance);
			auto extname = std::string(label) + " L/T/W/H";
			auto& opt = *assureEditorOptions(info, parentOptions);
			ArkSetFieldName({ extname.c_str(), extname.size() }, &opt);
			return ImGui::DragInt4("", (int*)&value, opt.drag_speed, opt.drag_min, opt.drag_max);
		});

		ark::meta::type<sf::Color>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			sf::Color& color = *(sf::Color*)instance;
			auto& opt = *assureEditorOptions(info, parentOptions);
			ArkSetFieldName(label, nullptr);
			float vec[4] = {
				float(color.r) / 255.f,
				float(color.g) / 255.f,
				float(color.b) / 255.f,
				float(color.a) / 255.f
			};

			if (ImGui::ColorEdit4("", vec, opt.flags)) {
				color.r = static_cast<uint8_t>(vec[0] * 255.f);
				color.g = static_cast<uint8_t>(vec[1] * 255.f);
				color.b = static_cast<uint8_t>(vec[2] * 255.f);
				color.a = static_cast<uint8_t>(vec[3] * 255.f);
				return true;
			}
			return false;
		});

		ark::meta::type<sf::Time>()->func<RenderTypeEditor>(RENDER_EDITOR,
			[](std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions)
		{
			sf::Time& value = *(sf::Time*)instance;
			std::string name = std::string(label) + " (as seconds)";
			auto& opt = *assureEditorOptions(info, parentOptions);
			ArkSetFieldName(name, &opt);
			float seconds = value.asSeconds();
			if (ImGui::DragFloat("", &seconds, opt.drag_speed, opt.drag_min, opt.drag_max, opt.format, opt.drag_power)) {
				value = sf::seconds(seconds);
				return true;
			}
			return false;
		});
	}
}
