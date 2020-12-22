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
		virtual int getStateId() = 0;

	protected:

		MessageBus& getMessageBus() { return *mMessageBus; }
		void requestStackPush(int stateId);
		void requestStackPop();
		void requestStackClear();

		StateStack* stateStack = nullptr;
	private:
		MessageBus* mMessageBus = nullptr;
		std::type_index mType = typeid(State);
		friend class StateStack;
	};

	class StateStack final : public NonCopyable, public NonMovable {

	public:

		template <typename T>
		void registerState(int id)
		{
			mFactories[id] = [this]() {
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
				if (T* ptr = dynamic_cast<T*>(state.get()))
					return ptr;
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

		void pushStateAndDisablePreviouses(int id)
		{
			mInPendingChanges.push_back([this, id]() {
				auto state = createState(id);
				if (state) {
					state->init();
					mStates.emplace(mStates.begin() + mStateLastIndex, StateData{ std::move(state), mBeginActiveIndex });
					mStateLastIndex += 1;
					mBeginActiveIndex = mStateLastIndex - 1;
				}
			});
		}

		void pushState(int id)
		{
			mInPendingChanges.push_back([this, id]() {
				auto state = createState(id);
				if (state) {
					state->init();
					mStates.emplace(mStates.begin() + mStateLastIndex, StateData{ std::move(state), ArkInvalidIndex });
					mStateLastIndex += 1;
				}
			});
		}

		void pushOverlay(int id)
		{
			mInPendingChanges.push_back([this, id]() {
				auto state = createState(id);
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

		auto createState(int id) -> std::unique_ptr<State>
		{
			auto it = mFactories.find(id);
			if (it == mFactories.end()) {
				EngineLog(LogSource::StateStack, LogLevel::Error, "didn't find state with id %d", id);
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
		std::map<int, std::function<std::unique_ptr<State>()>> mFactories;
		MessageBus* mMessageBus;

		friend class Engine;
	};
}
