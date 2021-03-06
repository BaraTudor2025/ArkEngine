#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include "ark/util/Util.hpp"

namespace ark
{
	class Renderer {

	public:
		Renderer() = default;
		virtual ~Renderer() = default;

		virtual void preRender(sf::RenderTarget&) {}
		virtual void render(sf::RenderTarget&) = 0;
		virtual void postRender(sf::RenderTarget&) {}
	};
}
