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
	// TODO(editor): serialize options
	struct EditorOptions {
		std::string_view property_name = "";

		/* cu cat se schimba valoarea(cat adaug/scad la valoare) pe pixel miscat cu mouse-ul 
		 * daca e '0'(zero) foloseste widgetu-ul InputInt/Float in loc de DragInt/Float
		*/
		float drag_speed = 0.5;

		/* 0 pentru ambele inseanma ca nu are limita */
		float drag_min = 0;
		float drag_max = 0;

		/* merge doar pentru float-uri */
		float drag_power = 1;

		/* float precision */
		int precision = 2;

		const char* format = "%.2f";

		/* flags passed down to ImGui widget if this is a primitive *and* the widget supports the flags */
		int flags = 0;

		/* sub-options for sub-properties that override the properties' type's own options */
		std::vector<EditorOptions> options;
	};

	/*
	 * Legend: parent type = the type which has this type as a property
	 * instance: pointer to the value to edit
	 * widgetID: id used by ImGui to make widgets unique in case they have the same name
	 * label: the string to display, usually the property name of the parent type
	 * parentOptions: options given by the parent type to this property, if it's null then you may use the types own options
	 * type: type of instance, used when the function is a runtime template and it doesn't know the static type(see renderProperties from SceneInscpetor)
	 * returns: true if the value has chaged, false otherwise
	*/
	using RenderTypeEditor = bool(std::type_index info, void* instance, int* widgetID, std::string_view label, EditorOptions* parentOptions);

	static inline constexpr std::string_view RENDER_EDITOR = "editor";
	static inline constexpr std::string_view RENDER_EDITOR_OPTIONS = "editor_options";


	class SceneInspector : public SystemT<SceneInspector>, public Renderer {
	public:
		SceneInspector() = default;
		~SceneInspector() = default;

		void init() override;
		void update() override {}
		void renderSystemInspector();
		void renderEntityEditor();

		void render(sf::RenderTarget&) override
		{
			//renderSystemInspector();
			renderEntityEditor();
		}
	};

	// returns the 'value' if present, 
	// otherwise return the type's options, declare it to default if the type doesn't have them
	inline EditorOptions* assureEditorOptions(std::type_index type, EditorOptions* value = nullptr) {
		if (value) {
			return value;
		}
		else {
			if (auto opt = ark::meta::resolve(type)->data<EditorOptions>(RENDER_EDITOR_OPTIONS)) {
				return opt;
			}
			else {
				return ark::meta::resolve(type)->data(RENDER_EDITOR_OPTIONS, EditorOptions{});
			}
		}
	}
}

namespace ark
{
	void ArkSetFieldName(std::string_view name, EditorOptions* opt = nullptr);
	bool AlignButtonToRight(const char* str, std::function<void()> callback);
}
