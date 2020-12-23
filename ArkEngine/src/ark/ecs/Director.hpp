#pragma once

#include <typeindex>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include "ark/core/Message.hpp"
#include "ark/core/MessageBus.hpp"

#define ROPROPERTY(GET) __declspec(property(get=GET))

namespace ark
{
	class Registry;
	class SystemManager;

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

		ROPROPERTY(getRegistry) Registry registry;
		ROPROPERTY(getType) std::type_index type;
		ROPROPERTY(getSystemManager) SystemManager systemManager;

		Registry& getRegistry() const { return *mRegistry; }
		SystemManager& getSystemManager() const { return *mSystemManager; }
		std::type_index getType() const { return mType; }

	private:
		MessageBus* mMessageBus;
		Registry* mRegistry;
		SystemManager* mSystemManager;
		std::type_index mType = typeid(Director);

		friend class SystemManager;
	};
}