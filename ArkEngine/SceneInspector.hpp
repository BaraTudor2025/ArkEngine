#pragma once

#include <typeindex>
#include <any>
#include <string_view>
#include <functional>
#include <unordered_map>

#include <libs/Meta.h>

class EntityManager;
class SystemManager;
class ScriptManager;
class ComponentManager;
class Scene;

class SceneInspector {

public:
	explicit SceneInspector(Scene& scene);
	~SceneInspector() = default;

	void renderSystemInspector();
	void renderEntityInspector();
	void renderEntityEditor();

	void render()
	{
		renderSystemInspector();
		renderEntityInspector();
		renderEntityEditor();
	}

	template <typename T>
	static bool renderFieldsOfType(int* widgetId, void* pValue);

private:
	EntityManager& entityManager;
	SystemManager& systemManager;
	ScriptManager& scriptManager;
	ComponentManager& componentManager;

	using FieldFunc = std::function<std::any(std::string_view, const void*)>;
	static std::unordered_map<std::type_index, FieldFunc> fieldRendererTable;

};


namespace ImGui {
	void PushID(int);
	void PopID();
	bool BeginCombo(const char* label, const char* preview_value, int flags);
	void EndCombo();
	void SetItemDefaultFocus();
	void Indent(float);
	void Unindent(float);
	void Text(char const*, ...);
}

void ArkSetFieldName(std::string_view name);
bool ArkSelectable(const char* label, bool selected); // forward to ImGui::Selectable

template <typename T>
bool SceneInspector::renderFieldsOfType(int* widgetId, void* pValue)
{
	T& valueToRender = *static_cast<T*>(pValue);
	std::any newValue;
	bool modified = false; // used in recursive call to check if the member was modified

	meta::doForAllMembers<T>([widgetId, &modified, &newValue, &valueToRender, &table = fieldRendererTable](auto& member) mutable {
		using MemberT = meta::get_member_type<decltype(member)>;
		ImGui::PushID(*widgetId);

		if constexpr (meta::isRegistered<MemberT>())  {
			// recursively render members that are registered
			auto memberValue = member.getCopy(valueToRender);
			ImGui::Text("%s:", member.getName());
			if (renderFieldsOfType<MemberT>(widgetId, &memberValue)) {
				member.set(valueToRender, memberValue);
				modified = true;
			}
		} else if constexpr (std::is_enum_v<MemberT>) {
			// get name of current enum value
			auto memberValue = member.getCopy(valueToRender);
			const auto& fields = meta::getEnumValues<MemberT>();
			const char* fieldName = nullptr;
			for (const auto& field : fields)
				if (field.value == memberValue)
					fieldName = field.name;

			// render enum values in a list
			ArkSetFieldName(member.getName());
			if (ImGui::BeginCombo("", fieldName, 0)) {
				for (const auto& field : fields) {
					if (ArkSelectable(field.name, field.value == memberValue)) {
						memberValue = field.value;
						modified = true;
						member.set(valueToRender, memberValue);
						break;
					}
				}
				ImGui::EndCombo();
			}
		} else {
			// render field using predefined table
			auto& renderField = table.at(typeid(MemberT));

			if (member.canGetConstRef())
				newValue = renderField(member.getName(), &member.get(valueToRender));
			else /* if (member.hasGetter()) /**/ {
				MemberT local = member.getCopy(valueToRender);
				newValue = renderField(member.getName(), &local);
			}

			if (newValue.has_value()) {
				if (member.hasSetter()) {
					member.set(valueToRender, std::any_cast<MemberT>(newValue));
					modified = true;
				} else if (member.canGetRef()) {
					member.getRef(valueToRender) = std::any_cast<MemberT>(newValue);
					modified = true;
				}
			}
			newValue.reset();
		}
		ImGui::PopID();
		*widgetId += 1;
	}); // doForAllMembers
	return modified;
}
