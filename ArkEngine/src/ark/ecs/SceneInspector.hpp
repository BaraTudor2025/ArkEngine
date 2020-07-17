#pragma once

#include <typeindex>
#include <any>
#include <string_view>
#include <functional>
#include <unordered_map>

#include "ark/ecs/Meta.hpp"

#define ARK_SERVICE_INSPECTOR service(ark::SceneInspector::serviceName, &ark::SceneInspector::renderFieldsOfType<Type>)

namespace ark {

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

		static inline constexpr std::string_view serviceName = "INSPECTOR";

	private:
		EntityManager& entityManager;
		SystemManager& systemManager;
		ScriptManager& scriptManager;
		ComponentManager& componentManager;

		using FieldFunc = std::function<std::any(std::string_view, const void*)>;
		static std::unordered_map<std::type_index, FieldFunc> fieldRendererTable;

	};
}


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

namespace ark {

	void ArkSetFieldName(std::string_view name);
	bool ArkSelectable(const char* label, bool selected); // forward to ImGui::Selectable

	template <typename T>
	bool SceneInspector::renderFieldsOfType(int* widgetId, void* pValue)
	{
		T& valueToRender = *static_cast<T*>(pValue);
		std::any newValue;
		bool modified = false; // used in recursive call to check if the member was modified

		meta::doForAllMembers<T>([widgetId, &modified, &newValue, &valueToRender, &table = fieldRendererTable](auto& member) mutable {
			using MemberType = meta::get_member_type<decltype(member)>;
			ImGui::PushID(*widgetId);

			if constexpr (meta::isRegistered<MemberType>()) {
				// recursively render members that are registered
				auto memberValue = member.getCopy(valueToRender);
				ImGui::Text("%s:", member.getName());
				if (renderFieldsOfType<MemberType>(widgetId, &memberValue)) {
					member.set(valueToRender, memberValue);
					modified = true;
				}
			} else if constexpr (std::is_enum_v<MemberType> /* && is registered*/) {
				auto memberValue = member.getCopy(valueToRender);
				const auto& fields = meta::getEnumValues<MemberType>();
				const char* fieldName = meta::getNameOfEnumValue<MemberType>(memberValue);

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
				auto& renderField = table.at(typeid(MemberType));

				if (member.canGetConstRef())
					newValue = renderField(member.getName(), &member.get(valueToRender));
				else {
					MemberType local = member.getCopy(valueToRender);
					newValue = renderField(member.getName(), &local);
				}

				if (newValue.has_value()) {
					member.set(valueToRender, std::any_cast<MemberType>(newValue));
					modified = true;
				}
				newValue.reset();
			}
			ImGui::PopID();
			*widgetId += 1;
		}); // doForAllMembers
		return modified;
	}
}
