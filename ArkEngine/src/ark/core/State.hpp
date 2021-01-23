#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <map>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Window/Event.hpp>

#include "ark/core/Message.hpp"
#include "ark/core/Logger.hpp"
#include "ark/util/Util.hpp"

namespace ark
{

	class StateStack;
	class MessageBus;
	class Registry;

	class State : public NonCopyable, public NonMovable {

	public:
		State(MessageBus& mb) : mMessageBus(&mb) {}
		virtual ~State() = default;

		virtual void init() {}
		virtual void handleMessage(const Message&) = 0;
		virtual void handleEvent(const sf::Event&) = 0;
		virtual void update() = 0;
		virtual void preRender(sf::RenderTarget&) {}
		virtual void render(sf::RenderTarget&) = 0;
		virtual void postRender(sf::RenderTarget&) {}

	protected:

		template <typename T>
		T* getState();

		MessageBus& getMessageBus() { return *mMessageBus; }
		void requestStackPush(std::type_index type);
		void requestStackPop();
		void requestStackClear();
		std::type_index getType() const { return mType; }

		StateStack* stateStack = nullptr;
	private:
		MessageBus* mMessageBus = nullptr;
		std::type_index mType = typeid(State);
		friend class StateStack;
	};

	class StateStack final : public NonCopyable, public NonMovable {

	public:

		template <typename T>
		void registerState()
		{
			mFactories[typeid(T)] = [this]() {
				auto state = std::make_unique<T>(*this->mMessageBus);
				state->stateStack = this;
				state->mType = typeid(T);
				return std::move(state);
			};
		}

		template <typename T>
		T* getState()
		{
			for (auto& [state, _] : mStates)
				if (state->mType == typeid(T))
					return static_cast<T*>(state.get());
			return nullptr;
		}

		void handleMessage(const Message& message)
		{
			forEachState([&](auto& state) {
				state->handleMessage(message);
			});
		}

		void handleEvent(const sf::Event& event)
		{
			forEachState([&](auto& state) {
				state->handleEvent(event);
			});
		}

		void update()
		{
			forEachState([](auto& state) {
				state->update();
			});
		}

		void preRender(sf::RenderTarget& target)
		{
			forEachState([&](auto& state) {
				state->preRender(target);
			});
		}

		void render(sf::RenderTarget& target)
		{
			forEachState([&](auto& state) {
				state->render(target);
			});
		}

		void postRender(sf::RenderTarget& target)
		{
			forEachState([&](auto& state) {
				state->postRender(target);
			});
		}

		template <typename T>
		void pushStateAndDisablePreviouses() {
			pushStateAndDisablePreviouses(typeid(T));
		}

		void pushStateAndDisablePreviouses(std::type_index type)
		{
			mInPendingChanges.push_back([this, type]() {
				auto state = createState(type);
				if (state) {
					state->init();
					mStates.emplace(mStates.begin() + mStateLastIndex, StateData{ std::move(state), mBeginActiveIndex });
					mStateLastIndex += 1;
					mBeginActiveIndex = mStateLastIndex - 1;
				}
			});
		}

		template <typename T>
		void pushState() {
			pushState(typeid(T));
		}

		void pushState(std::type_index type)
		{
			mInPendingChanges.push_back([this, type]() {
				auto state = createState(type);
				if (state) {
					state->init();
					mStates.emplace(mStates.begin() + mStateLastIndex, StateData{ std::move(state), ArkInvalidIndex });
					mStateLastIndex += 1;
				}
			});
		}

		template <typename T>
		void pushOverlay() {
			pushOverlay(typeid(T));
		}

		void pushOverlay(std::type_index type)
		{
			mInPendingChanges.push_back([this, type]() {
				auto state = createState(type);
				if (state) {
					state->init();
					mStates.emplace_back(StateData{ std::move(state), ArkInvalidIndex });
				}
			});
		}

		void popState() // int id
		{
			mInPendingChanges.push_back([this]() {
				auto& state = mStates.at(std::size_t(mStateLastIndex) - 1);
				if (state.previousStateThatDisablesIndex != ArkInvalidIndex) {
					mBeginActiveIndex = state.previousStateThatDisablesIndex;
				}
				mStates.erase(mStates.begin() + mStateLastIndex - 1);
				mStateLastIndex--;
			});
		}

		void popOverlay()
		{
			mInPendingChanges.push_back([this]() {
				if(mStateLastIndex < mStates.size())
					mStates.pop_back();
			});
		}

		void clearStack()
		{
			mInPendingChanges.push_back([this]() {
				mStates.clear();
				mStateLastIndex = 0;
				mBeginActiveIndex = 0;
			});
		}

		void processPendingChanges()
		{
			mInPendingChanges.swap(mOutPendingChanges);
			for (auto change : mOutPendingChanges)
				change();
			mOutPendingChanges.clear();
		}

	private:

		template <typename F>
		void forEachState(F&& f)
		{
			for (auto it = mStates.begin() + mBeginActiveIndex; it != mStates.end(); it++) {
				f(it->state);
			}
		}

		auto createState(std::type_index type) -> std::unique_ptr<State>
		{
			auto it = mFactories.find(type);
			if (it == mFactories.end()) {
				EngineLog(LogSource::StateStack, LogLevel::Error, "didn't find state %s\n", type.name());
				return nullptr;
			}
			else
				return it->second();
		}

		struct StateData {
			std::unique_ptr<State> state;
			int previousStateThatDisablesIndex = -1;
		};

		std::vector<StateData> mStates;
		std::vector<std::function<void()>> mInPendingChanges;
		std::vector<std::function<void()>> mOutPendingChanges;
		int mStateLastIndex = 0;
		int mBeginActiveIndex = 0;
		std::map<std::type_index, std::function<std::unique_ptr<State>()>> mFactories;
		MessageBus* mMessageBus;

		friend class Engine;
	};

	template <typename T>
	//requires std::derived_from<T, State>
	T* State::getState() {
		return this->stateStack->getState<T>();
	}
}
