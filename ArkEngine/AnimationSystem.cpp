#include "AnimationSystem.hpp"
#include <ark/core/Engine.hpp>

void AnimationSystem::update()
{
	for (auto [mesh, cont] : view) {
		if (cont.stopped())
			continue;
		auto& anim = cont.animations[cont.m_id];
		cont.m_elapsedTime += ark::Engine::deltaTime();
		// next frame
		if (cont.m_elapsedTime >= anim.framerate) {
			cont.m_elapsedTime = sf::Time::Zero;
			cont.m_frameID += 1;
			// loop or stop when we reach the end
			if (cont.m_frameID >= anim.frames.size()) {
				if (!cont.m_queue.empty()) {
					cont.play(cont.m_queue.front(), true);
					cont.m_queue.pop();
				}
				else if (anim.looped) {
					cont.m_frameID = anim.loopStart;
				}
				else {
					cont.stop();
					continue;
				}
			}
		}
		mesh.uvRect = cont.animations[cont.m_id].frames[cont.m_frameID];
		mesh.vertices.updatePosTex(mesh.uvRect);
	}
}

void MeshSystem::render(sf::RenderTarget& target)
{
	sf::RenderStates rs;
	for (auto [transform, mesh] : entityManager.view<const ark::Transform, MeshComponent>()) {
		auto rect = mesh.uvRect; // used for flippping
		if (mesh.flipX) {
			rect.width = -rect.width;
			rect.left += std::abs(rect.width);
			mesh.vertices.updateTexCoords(rect);
		}
		if (mesh.flipY) {
			rect.height = -rect.height;
			rect.top += std::abs(rect.height);
			mesh.vertices.updateTexCoords(rect);
		}
		
		rs.texture = mesh.texture;
		rs.transform = transform.getTransform();
		target.draw(mesh.vertices.data(), 4, sf::TriangleStrip, rs);

		// reset vertices to original
		if(mesh.flipX || mesh.flipY)
			mesh.vertices.updateTexCoords(mesh.uvRect);
	}
}
