#include "AnimationSystem.hpp"
#include "ResourceManager.hpp"

template <typename Range>
inline void updateVerticesWithRect(Range& vertices, const sf::IntRect& uvRect)
{
	auto h = std::abs(uvRect.height);
	auto w = std::abs(uvRect.width);
	vertices[0].position = sf::Vector2f(0, 0);
	vertices[1].position = sf::Vector2f(0, h);
	vertices[2].position = sf::Vector2f(w, 0);
	vertices[3].position = sf::Vector2f(w, h);

	float left = uvRect.left;//static_cast<float>(uvRect.left);
	float right = left + uvRect.width;
	float top = uvRect.top; //static_cast<float>(uvRect.top);
	float bottom = top + uvRect.height;

	vertices[0].texCoords = sf::Vector2f(left, top);
	vertices[1].texCoords = sf::Vector2f(left, bottom);
	vertices[2].texCoords = sf::Vector2f(right, top);
	vertices[3].texCoords = sf::Vector2f(right, bottom);
}

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
	else if constexpr (N == 3) return (this->currentFrame);
	else if constexpr (N == 4) return (this->elapsedTime);
	else if constexpr (N == 5) return (this->uvRect);
}

void AnimationSystem::add(Component * c)
{
	if (auto a = static_cast<Animation*>(c); a) {
		a->pTexture = load<sf::Texture>(a->fileName);
		a->pTexture->setSmooth(true);
		a->uvRect.width = a->pTexture->getSize().x / (float)a->frameCount.x;
		a->uvRect.height = a->pTexture->getSize().y / (float)a->frameCount.y;
		a->elapsedTime = sf::seconds(0);
		a->currentFrame.x = 0;
		a->vertices.resize(4);
		a->vertices.setPrimitiveType(sf::TriangleStrip);
		a->entity()->getComponent<Transform>()->setOrigin(a->frameSize() / 2.f);
	}
}

void AnimationSystem::update()
{
	for (auto animation : this->getComponents<Animation>()) {
		auto&[row, frameCount, frameTime, currentFrame, elapsedTime, uvRect] = *animation;

		currentFrame.y = row;
		elapsedTime += VectorEngine::deltaTime();
		if (elapsedTime >= frameTime) {
			elapsedTime -= frameTime;
			currentFrame.x += 1;
			if (currentFrame.x >= frameCount.x)
				currentFrame.x = 0;
		}
		uvRect.top = currentFrame.y * uvRect.height;
		if (animation->flipped) {
			uvRect.left = (currentFrame.x + 1) * std::abs(uvRect.width);
			uvRect.width = -std::abs(uvRect.width);
		} else {
			uvRect.left = currentFrame.x * uvRect.width;
			uvRect.width = std::abs(uvRect.width);
		}

		updateVerticesWithRect(animation->vertices, uvRect);
	}
}

void AnimationSystem::render(sf::RenderTarget & target)
{
	sf::RenderStates rs;
	for (auto animation : this->getComponents<Animation>()) {
		rs.texture = animation->pTexture;
		if(auto t = animation->entity()->getComponent<Transform>(); t)
			rs.transform = *t;
		target.draw(animation->vertices, rs);
	}
}
