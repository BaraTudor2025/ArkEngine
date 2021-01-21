#pragma once

#include <typeindex>
#include <any>
#include <string_view>
#include <functional>
#include <unordered_map>

#include "ark/ecs/Meta.hpp"
#include "ark/ecs/Renderer.hpp"
#include "ark/ecs/System.hpp"

namespace ark
{
	struct EditorOptions {
		std::string_view property_name;
		float drag_speed = 0.5;
		float drag_min = 0;
		float drag_max = 0;
		// merg doar pentru float
		float drag_power = 1;
		const char* format = "%.2f";
	};
	class EntityManager;

	class SceneInspector : public SystemT<SceneInspector>, public Renderer {

	public:
		SceneInspector() = default;
		~SceneInspector() = default;

		void update() override {}
		void renderSystemInspector();
		//void renderEntityInspector();
		void renderEntityEditor();

		void render(sf::RenderTarget&) override
		{
			//renderSystemInspector();
			//renderEntityInspector();
			renderEntityEditor();
		}

		template <typename T>
		static bool renderPropertiesOfType(int* widgetId, void* pValue);
		static bool renderPropertiesOfType(std::type_index type, int* widgetId, void* pValue);

		static inline constexpr std::string_view serviceName = "INSPECTOR";
		static inline constexpr std::string_view serviceOptions = "ark_inspector_options";
		using VectorOptions = std::vector<EditorOptions>;

	private:
		using RenderPropFunc = std::function<std::any(std::string_view, const void*, EditorOptions)>;
		static std::unordered_map<std::type_index, RenderPropFunc> sPropertyRendererTable;

	};
}

namespace ImGui
{
	void PushID(int);
	void PopID();
	bool BeginCombo(const char* label, const char* preview_value, int flags);
	void EndCombo();
	void SetItemDefaultFocus();
	void Indent(float);
	void Unindent(float);
	void Text(char const*, ...);
	void TreePop();
}

namespace ark
{
	void ArkSetFieldName(std::string_view name);
	bool ArkSelectable(const char* label, bool selected); // forward to ImGui::Selectable
	bool AlignButtonToRight(const char* str, std::function<void()> callback);
	void ArkAlign(std::string_view str, float widthPercentage);

	template <typename TComp>
	bool SceneInspector::renderPropertiesOfType(int* widgetId, void* pValue)
	{
		const auto* options = ark::meta::getMetadata(typeid(type))->data<VectorOptions>(serviceOptions);
		TComp& valueToRender = *static_cast<TComp*>(pValue);
		std::any newValue;
		bool modified = false; // used in recursive call to check if the property was modified

		meta::doForAllProperties<TComp>([options, widgetId, &modified, &newValue, &valueToRender, &table = sPropertyRendererTable](auto& property) mutable {
			using PropType = meta::get_member_type<decltype(property)>;
			ImGui::PushID(*widgetId);

			if constexpr (meta::isRegistered<PropType>()) {
				// recursively render members that are registered
				auto propValue = property.getCopy(valueToRender);
				ImGui::Text("%s:", property.getName());
				if (renderPropertiesOfType<PropType>(widgetId, &propValue)) {
					property.set(valueToRender, propValue);
					modified = true;
				}
			}
			else if constexpr (std::is_enum_v<PropType> /* && is registered*/) {
				auto propValue = property.getCopy(valueToRender);
				const auto& fields = meta::getEnumValues<PropType>();
				const char* fieldName = meta::getNameOfEnumValue<PropType>(propValue).data();

				// render enum values in a list
				ArkSetFieldName(property.getName());
				if (ImGui::BeginCombo("", fieldName, 0)) {
					for (const auto& field : *fields) {
						if (ArkSelectable(field.name.data(), propValue == field)) {
							propValue = field;
							modified = true;
							property.set(valueToRender, propValue);
							break;
						}
					}
					ImGui::EndCombo();
				}
			}
			else {
				// render field using predefined table
				auto& renderProperty = table.at(typeid(PropType));

				// find custom editor options for field
				auto editopt = EditorOptions{};
				if (options) {
					for (auto& opt : *options)
						if (opt.property_name == property.getName())
							editopt = opt;
				}

				if (property.canGetConstRef())
					newValue = renderProperty(property.getName(), &property.get(valueToRender), editopt);
				else {
					PropType local = property.getCopy(valueToRender);
					newValue = renderProperty(property.getName(), &local, editopt);
				}

				if (newValue.has_value()) {
					property.set(valueToRender, std::any_cast<PropType>(newValue));
					modified = true;
				}
				newValue.reset();
			}
			ImGui::PopID();
			*widgetId += 1;
		}); // doForAllProperties
		return modified;
	}
}
