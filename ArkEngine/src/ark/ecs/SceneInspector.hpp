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
		std::string_view property_name;

		/* cu cat se schimba valoarea(cat adaug/scad la valoare) pe pixel miscat cu mouse-ul */
		float drag_speed = 0.5;

		/* 0 pentru ambele inseanma ca nu are limita */
		float drag_min = 0;
		float drag_max = 0;

		/* merge doar pentru float-uri */
		float drag_power = 1;
		const char* format = "%.2f";

		/* 
		 * -sub-optiuni pentru un membru compus la randul lui din proprietati
		 * evident, optiunile de mai sus sunt ignorate
		 * -daca nu este definit, atunci editorul incearca sa foloseasca optiunule definite la nivelul de type
		*/ 
		std::vector<EditorOptions> options;

		// folosit intern, nu umbla la el
		// declarat public ca struct-ul sa ramana un aggregate
		bool privateOpenPopUp = false;
	};

	class EntityManager;

	class SceneInspector : public SystemT<SceneInspector>, public Renderer {

	public:
		SceneInspector() = default;
		~SceneInspector() = default;

		void update() override {}
		void renderSystemInspector();
		void renderEntityEditor();

		void render(sf::RenderTarget&) override
		{
			//renderSystemInspector();
			renderEntityEditor();
		}

		static bool renderPropertiesOfType(std::type_index type, int* widgetId, void* pValue, 
			std::type_index parentType = typeid(void), std::string_view thisPropertyName = "");

		static inline constexpr std::string_view serviceName = "INSPECTOR";
		static inline constexpr std::string_view serviceOptions = "ark_inspector_options";
		using VectorOptions = std::vector<EditorOptions>;

		using RenderPropFunc = std::function<std::any(std::string_view, const void*, EditorOptions&)>;
		static std::unordered_map<std::type_index, RenderPropFunc> s_renderPropertyTable;
	};
}

namespace ark
{
	void ArkSetFieldName(std::string_view name, EditorOptions* opt = nullptr);
	bool AlignButtonToRight(const char* str, std::function<void()> callback);
}
