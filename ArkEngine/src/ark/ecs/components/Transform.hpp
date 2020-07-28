#pragma once

#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/RenderStates.hpp>

#include "ark/core/Core.hpp"
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/DefaultServices.hpp"
#include "ark/ecs/Entity.hpp"

namespace ark
{


	struct TagComponent final : public Component<TagComponent> {
		TagComponent() = default;
		~TagComponent() = default;

		TagComponent(const TagComponent&) = delete;
		TagComponent& operator=(const TagComponent&) = delete;

		TagComponent(TagComponent&&) = default;
		TagComponent& operator=(TagComponent&&) = default;

		TagComponent(std::string name) : m_name(name) {}

		__declspec(property(get = _getName, put = _setName))
			std::string name;

		const std::string& _getName() const
		{
			return m_name;
		}

		void _setName(const std::string& newName)
		{
			auto constexpr strEntity_ = std::string_view("entity_");
			auto enttName = std::string_view(newName.c_str(), strEntity_.size());
			if (enttName == "entity_" || newName.empty()) {
				m_name.clear();
				m_name.append("entity_");
				m_name.append(std::to_string(m_entity.getID()));
			}
			else {
				m_name = newName;
			}
		}

		void _setEntity(ark::Entity e)
		{
			m_entity = e;
		}

	private:
		ark::Entity m_entity;
		mutable std::string m_name;
	};

	struct ARK_ENGINE_API Transform final : public Component<Transform>, public sf::Transformable{

		using sf::Transformable::Transformable;
		operator const sf::Transform& () const { return this->getTransform(); }
		operator const sf::RenderStates& () const { return this->getTransform(); }

		Transform() = default;

		// don't actually use it, copy_ctor is defined so that the compiler dosen't 
		// scream when we instantiate deque<Transform> in the implementation
		//Transform(const Transform& tx) {}
		//Transform& operator=(const Transform& tx) {}

		Transform(Transform&& tx) noexcept
		{
			this->moveToThis(std::move(tx));
		}

		Transform& operator=(Transform&& tx) noexcept
		{
			if (&tx == this)
				return *this;
			this->removeFromParent();
			this->orphanChildren();
			this->moveToThis(std::move(tx));
			return *this;
		}

		~Transform()
		{
			orphanChildren();
			removeFromParent();
		}

		void addChild(Transform& child)
		{
			if (&child == this)
				return;
			if (child.m_parent == this)
				return;
			child.removeFromParent();
			child.m_parent = this;
			m_children.push_back(&child);
		}

		void removeChild(Transform& child)
		{
			if (&child == this)
				return;
			child.m_parent = nullptr;
			Util::erase(m_children, &child);
		}

		const std::vector<Transform*>& getChildren() const { return m_children; }

		sf::Transform getWorldTransform()
		{
			if (m_parent)
				return this->getTransform() * m_parent->getWorldTransform();
			else
				return this->getTransform();
		}

		//int depth() { return m_depth; }


	private:

		void removeFromParent()
		{
			if (m_parent)
				m_parent->removeChild(*this);
		}

		void orphanChildren()
		{
			for (auto child : m_children)
				child->m_parent = nullptr;
		}

		void moveToThis(Transform&& tx)
		{
			this->m_parent = tx.m_parent;
			tx.removeFromParent();
			this->m_parent->addChild(*this);

			this->m_children = std::move(tx.m_children);
			for (auto child : m_children)
				child->m_parent = this;

			this->setOrigin(tx.getOrigin());
			this->setPosition(tx.getPosition());
			this->setRotation(tx.getRotation());
			this->setScale(tx.getScale());

			tx.setPosition({0.f, 0.f});
			tx.setRotation(0.f);
			tx.setScale({1.f, 1.f});
			tx.setOrigin({0.f, 0.f});
		}

		Transform* m_parent = nullptr;
		std::vector<Transform*> m_children{};
		//int m_depth;
	};
}

ARK_REGISTER_TYPE(ark::Transform, "Transform", ARK_DEFAULT_SERVICES)
{
	return members(
		member_property("position", &ark::Transform::getPosition, &ark::Transform::setPosition),
		member_property("scale", &ark::Transform::getScale, &ark::Transform::setScale),
		member_property("roatation", &ark::Transform::getRotation, &ark::Transform::setRotation),
		member_property("origin", &ark::Transform::getOrigin, &ark::Transform::setOrigin),
		member_function<ark::Transform, void, float, float>("move", &ark::Transform::move),
		member_function<ark::Transform>("getChildren", &ark::Transform::getChildren)
	);
}

ARK_REGISTER_TYPE(ark::TagComponent, "Tag", ARK_DEFAULT_SERVICES)
{
	return members(member_property("name", &ark::TagComponent::_getName, &ark::TagComponent::_setName));
}
