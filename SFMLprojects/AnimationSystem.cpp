#include "AnimationSystem.hpp"
#include "ResourceManager.hpp"
#include "Util.hpp"

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

void AnimationSystem::init()
{
	forEach<Animation>([](auto& a) {

		auto visitor = [](const std::pair<uint32_t, std::vector<uint32_t>>& frameCounts) {
			return sf::Vector2u{ frameCounts.first, (uint32_t)frameCounts.second.size() }; // size means the number of columns
		}; 
		auto[fcX, fcY] = std::visit(overloaded{
			visitor,
			[](sf::Vector2u frameCount) { return frameCount; }
		}, a.frameCount);

		a.texture = load<sf::Texture>(a.fileName);
		a.uvRect.width = a.texture->getSize().x / (float)fcX;
		a.uvRect.height = a.texture->getSize().y / (float)fcY;
		a.elapsedTime = sf::seconds(0);
		a.currentFrame.x = 0;
	});
	forEach<Mesh>([](auto& mesh) {
		mesh.texture = load<sf::Texture>(mesh.fileName);
		mesh.entity()->getComponent<Transform>()->setOrigin(static_cast<sf::Vector2f>(mesh.texture->getSize()) / 2.f);
		auto[a, b, c, d] = mesh.uvRect;
		if (a == 0 && b == 0 && c == 0 && d == 0) { // undefined uvRect
			mesh.uvRect.width = mesh.texture->getSize().x;
			mesh.uvRect.height = mesh.texture->getSize().y;
			mesh.uvRect.left = 0;
			mesh.uvRect.top = 0;
		}
		mesh.vertices.updatePosTex(mesh.uvRect);
		//sf::IntRect uvRect;
		//if (mesh->flipX) {
		//	uvRect.left = uvRect.width;
		//	uvRect.width = -uvRect.width;
		//} else {
		//	uvRect.left = 0;
		//} 
		//if (mesh->flipY) {
		//	uvRect.top = uvRect.height;
		//	uvRect.height = -uvRect.height;
		//} else {
		//	uvRect.top = 0;
		//}
	});
}

void AnimationSystem::update()
{
	forEach<Animation>([](auto& animation) {
		auto&[row, frameCount, frameTime, currentFrame, elapsedTime, uvRect] = animation;

		auto visitor = [row = row](std::pair<uint32_t, std::vector<uint32_t>>& frameCounts) { return frameCounts.second[row]; };
		uint32_t frameCountX = std::visit(overloaded{
			visitor,
			[](sf::Vector2u frameCount) { return frameCount.x; }
		}, frameCount);

		currentFrame.y = row;
		elapsedTime += VectorEngine::deltaTime();
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
	});
}

void AnimationSystem::render(sf::RenderTarget & target)
{
	sf::RenderStates rs;
	forEach<Mesh>([&](auto& mesh){
		mesh.texture->setRepeated(mesh.repeatTexture);
		mesh.texture->setSmooth(mesh.smoothTexture);
		rs.texture = mesh.texture;
		rs.transform = *mesh.entity()->getComponent<Transform>();
		target.draw(mesh.vertices.data(), 4, sf::TriangleStrip, rs);
	});
	forEach<Animation>([&](auto& animation){
		animation.texture->setSmooth(animation.smoothTexture);
		rs.texture = animation.texture;
		rs.transform = *animation.entity()->getComponent<Transform>();
		target.draw(animation.vertices.data(), 4, sf::TriangleStrip, rs);
	});
}
