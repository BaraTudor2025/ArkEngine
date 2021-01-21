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
}
