#include <typeindex>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Event.hpp>
#include "ark/ecs/Scene.hpp"
#include "ark/core/Message.hpp"
#include "ark/core/MessageBus.hpp"

namespace ark
{
	class Director {

	public:
		Director() = default;
		virtual ~Director() = default;

		virtual void init() = 0;
		virtual void update() = 0;
		virtual void handleMessage(const Message&) {}
		virtual void handleEvent(const sf::Event&) {}

	protected:
		template <typename T>
		T* postMessage(int id)
		{
			return mMessageBus->post<T>(id);
		}

		template <typename F>
		void forEachEntity(F&& f);

		Scene& scene() const { return *mScene; }
		std::type_index type() const { return mType; }

	private:
		MessageBus* mMessageBus;
		Scene* mScene;
		std::type_index mType;

		friend class Scene;
	};
}