#pragma once

#include <typeindex>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include "ark/core/Message.hpp"
#include "ark/core/MessageBus.hpp"

#define ROPROPERTY(GET) __declspec(property(get=GET))

namespace ark
{
	class Scene;
	class EntityManager;

	class Director {

	public:
		Director() = default;
		virtual ~Director() = default;

		virtual void init() = 0;
		virtual void update() {}
		virtual void handleMessage(const Message&) {}
		virtual void handleEvent(const sf::Event&) {}

	protected:
		template <typename T>
		T* postMessage(int id)
		{
			return mMessageBus->post<T>(id);
		}

		ROPROPERTY(getScene) Scene scene;
		ROPROPERTY(getType) std::type_index type;

		Scene& getScene() const { return *mScene; }
		std::type_index getType() const { return mType; }

	private:
		MessageBus* mMessageBus;
		Scene* mScene;
		std::type_index mType = typeid(Director);

		friend class Scene;
	};
}