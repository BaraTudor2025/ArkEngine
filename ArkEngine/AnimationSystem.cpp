#include "AnimationSystem.hpp"
#include <ark/core/Engine.hpp>

namespace std{

	template<> struct tuple_size<Animation> : std::integral_constant<std::size_t, 6> {};

	template<std::size_t N> 
	struct tuple_element<N, Animation> {
		using type = decltype(std::declval<Animation>().get<N>());
	};
}

template<std::size_t N>
inline decltype(auto) Animation::get()
{
	if constexpr (N == 0) return this->row;
	else if constexpr (N == 1) return this->frameCount;
	else if constexpr (N == 2) return this->frameTime;
	else if constexpr (N == 3) return (this->currentFrame); // parens means return by ref
	else if constexpr (N == 4) return (this->elapsedTime);
	else if constexpr (N == 5) return (this->uvRect);
}

void AnimationSystem::update()
{
	for (auto& animation : view.each<Animation>()) {

		auto&[row, frameCount, frameTime, currentFrame, elapsedTime, uvRect] = animation;

		auto visitor = [row = row](std::pair<uint32_t, std::vector<uint32_t>>& frameCounts) { return frameCounts.second[row]; };
		uint32_t frameCountX = std::visit(Util::overloaded{
			visitor,
			[](sf::Vector2u frameCount) { return frameCount.x; }
		}, frameCount);

		currentFrame.y = row;
		elapsedTime += ark::Engine::deltaTime();
		if (elapsedTime >= frameTime) {
			elapsedTime -= frameTime;
			currentFrame.x += 1;
			if (currentFrame.x >= frameCountX)
				currentFrame.x = 0;
		}

		if (animation.flipX) {
			uvRect.left = (currentFrame.x + 1) * std::abs(uvRect.width);
			uvRect.width = -std::abs(uvRect.width);
		} else {
			uvRect.left = currentFrame.x * uvRect.width;
			uvRect.width = std::abs(uvRect.width);
		}
		if (animation.flipY) {
			uvRect.top = (currentFrame.y + 1) * std::abs(uvRect.height);
			uvRect.height = -std::abs(uvRect.height);
		} else {
			uvRect.top = currentFrame.y * uvRect.height;
			uvRect.height = std::abs(uvRect.height);
		}

		animation.vertices.updatePosTex(uvRect);
	}
}

void AnimationSystem::render(sf::RenderTarget& target)
{
	sf::RenderStates rs;
	for (auto [transform, animation] : view) {
		animation.texture->setSmooth(animation.smoothTexture);
		rs.texture = animation.texture;
		rs.transform = transform.getTransform();
		target.draw(animation.vertices.data(), 4, sf::TriangleStrip, rs);
	}
}

void MeshSystem::render(sf::RenderTarget& target)
{
	sf::RenderStates rs;
	entityManager.view<ark::Transform, MeshComponent>().each([&](const auto& transform, auto& mesh) {
		rs.texture = mesh.texture;
		rs.transform = transform.getTransform();
		target.draw(mesh.vertices.data(), 4, sf::TriangleStrip, rs);
	});
}
