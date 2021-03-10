#pragma once

#include <variant>

#include <ark/ecs/Component.hpp>
#include <ark/ecs/System.hpp>
#include <ark/ecs/components/Transform.hpp>
#include <ark/util/ResourceManager.hpp>
#include <ark/util/Util.hpp>
#include <ark/ecs/DefaultServices.hpp>
#include <ark/ecs/Renderer.hpp>

#include <queue>

#include "Quad.hpp"

struct MeshComponent {

	MeshComponent() = default;

	MeshComponent(std::string fileName, bool smoothTexture = true)
		:fileName(fileName), smoothTexture(smoothTexture), uvRect(0,0,0,0), repeatTexture(false) 
	{
		setTexture(fileName);
	}

	MeshComponent(std::string fileName, bool smoothTexture, sf::IntRect rect, bool repeatTexture)
		:fileName(fileName),  smoothTexture(smoothTexture), uvRect(uvRect), repeatTexture(repeatTexture) 
	{
		setTexture(fileName);
	}

	sf::Vector2f getMeshSize() const { 
	    return static_cast<sf::Vector2f>(
		    sf::Vector2i{ std::abs(uvRect.width), std::abs(uvRect.height) }); 
	}

	void setMeshSize(sf::Vector2f vec) {
		uvRect.width = vec.x;
		uvRect.height = vec.y;
		vertices.updatePosTex(uvRect);
	}

	sf::IntRect getTextureRect() const {
		return uvRect;
	}

	void setTextureRect(sf::IntRect rect) {
		uvRect = rect;
		vertices.updatePosTex(uvRect);
	}

	void setTexture(const std::string& name)
	{
		fileName = name;
		texture = ark::Resources::load<sf::Texture>(name);
		texture->setRepeated(repeatTexture);
		texture->setSmooth(smoothTexture);
		//auto[a, b, c, d] = uvRect;
		//if (a == 0 && b == 0 && c == 0 && d == 0) { // undefined uvRect
		uvRect.width = texture->getSize().x;
		uvRect.height = texture->getSize().y;
		uvRect.left = 0;
		uvRect.top = 0;
		//}
		vertices.updatePosTex(uvRect);
	}

	const std::string& getTexture() const {
		return fileName;
	}

	sf::IntRect uvRect;
	Quad vertices;
	bool flipX = false;
	bool flipY = false;

	const sf::Texture* getTextureHandle() const {
		return texture;
	}

private:
	bool repeatTexture = true;
	bool smoothTexture = true;
	std::string fileName = "";
	sf::Texture* texture = nullptr;
	friend class MeshSystem;
};

ARK_REGISTER_COMPONENT(MeshComponent, registerServiceDefault<MeshComponent>()) {
	return members<MeshComponent>(
		member_property("texture", &MeshComponent::getTexture, &MeshComponent::setTexture),
		member_property("mesh_size", &MeshComponent::getMeshSize, &MeshComponent::setMeshSize),
		member_property("flip_x", &MeshComponent::flipX),
		member_property("flip_y", &MeshComponent::flipY)
	);
}

struct AnimationController {

	AnimationController() = default;
	AnimationController(const AnimationController& a) = default;
	AnimationController& operator=(const AnimationController&) = default;

	AnimationController(int animationsCount, int maxFrames)
		: numAnimations(animationsCount), maxFrames(maxFrames) {}

	static void onAdd(ark::EntityManager& manager, ark::EntityId entity)
	{
		auto& mesh = manager.get<MeshComponent>(entity);
		auto& anim = manager.get<AnimationController>(entity);

		sf::Vector2u textureSize = mesh.getTextureHandle()->getSize();
		sf::Vector2u frameSize = textureSize / sf::Vector2u(anim.maxFrames, anim.numAnimations);
		mesh.uvRect.width = frameSize.x;
		mesh.uvRect.height = frameSize.y;
	}

	struct Animation {
		std::vector<sf::IntRect> frames;
		sf::Time framerate = sf::milliseconds(120);
		int loopStart = 0;
		bool looped = false;
		bool cancelable = true;
	};

	// TODO: enum based animations? asign special index to animation at declaration
	std::vector<Animation> animations;
	sf::Vector2f frameSize; // set in onAdd
	int maxFrames;
	int numAnimations;

	// TODO: kinda broken
	// wait for the current animation to finish, then play this one
	void playInQueue(int id){
		if(m_queue.empty() || m_queue.back() != id)
			m_queue.push(id);
	}

	// cancels the current animation to play another one
	void play(int id, bool rewind = false) {
		if (rewind || (m_id != id))
			m_frameID = 0;
		m_id = id;
		m_playing = true;
	}

	void pause() {
		m_playing = false;
	}
	void resume() {
		m_playing = true;
	}

	void stop() {
		m_playing = false;
		m_frameID = 0;
		m_id = 0;
		m_elapsedTime = sf::Time::Zero;
	}

	bool stopped() {
		return !m_playing;
	}

private:
	//std::variant<sf::Vector2u, std::pair<uint32_t, std::vector<uint32_t>>> frameCount;
	//sf::Vector2u currentFrame;
	//sf::Time elapsedTime;

	std::queue<int> m_queue; // animation queue;
	int m_id = 0; // animation index
	int m_frameID = 0;
	bool m_playing = false;
	sf::Time m_elapsedTime;

	friend class AnimationSystem;
};

ARK_REGISTER_COMPONENT(AnimationController, registerServiceDefault<AnimationController>())
{
	return members<AnimationController>(
		member_property("maxFrameNumber", &AnimationController::maxFrames),
		member_property("numAnimations", &AnimationController::numAnimations)
		//member_property("frame_count", &AnimationController::getFrameCount, &AnimationController::setFrameCount),
		//member_property("frame_count", &AnimationController::getFrameCount, &AnimationController::setFrameCount),
		//member_property("texture", &AnimationController::getTexture, &AnimationController::setTexture),
		//member_property("frame_time", &AnimationController::frameTime),
		//member_property("row", &AnimationController::row)
		//member("frame_size", &AnimationController::frameSize)
	);
}

/* 
 * animationRow: row of animation in sprite, animation id, index in animations vector
 * frameCount: frame number for this animation
 * pre-conditions: 
 *  - animations and frames are equally spaced-out in sprite
 *  - entity has a MeshComponent with a texture
*/
inline auto makeAnimation(ark::Entity entity, int animationRow, int frameCount) -> AnimationController::Animation& {
	auto& controller = entity.get<AnimationController>();
	sf::Vector2u textureSize = entity.get<MeshComponent>().getTextureHandle()->getSize();
	sf::Vector2u frameSize = textureSize / sf::Vector2u(controller.maxFrames, controller.numAnimations);
	auto anim = AnimationController::Animation{};
	for (int i = 0; i < frameCount; i++) {
		auto& area = anim.frames.emplace_back();
		area.left = i * frameSize.x;
		area.top = animationRow * frameSize.y;
		area.width = frameSize.x;
		area.height = frameSize.y;
	}
	if (controller.animations.size() <= animationRow)
		controller.animations.resize(animationRow + 1);
	return controller.animations[animationRow] = anim;
}

// update for MeshComponent, AnimationController
class AnimationSystem : public ark::SystemT<AnimationSystem> {
	ark::View<MeshComponent, AnimationController> view;
public:

	void init() override
	{
		view = entityManager;
	}

	void update() override;
};

// for ark::Transform, MeshComponent
class MeshSystem : public ark::SystemT<MeshSystem>, public ark::Renderer {
public:

	void init() override {}

	void update() override {}

	void render(sf::RenderTarget& target) override;
};
