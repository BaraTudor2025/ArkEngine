#pragma once

#include "Message.hpp"
#include "Util.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <map>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Window/Event.hpp>

class StateStack;
class MessageBus;
class Scene;

class State : public NonCopyable, public NonMovable {

public:
	State(MessageBus& mb) : messageBus(&mb) {}
	virtual ~State();

	virtual void init() { }
	virtual void handleMessage(const Message&) = 0;
	virtual bool handleEvent(const sf::Event&) = 0;
	virtual bool update() = 0;
	virtual bool render(sf::RenderTarget&) = 0;
	virtual int getStateId() = 0;

protected:

	MessageBus& getMessageBus() { return *messageBus; }
	void requestStackPush(int stateId);
	void requestStackPop();
	void requestStackClear();
	void addTab(std::string, std::function<void()>);
	void removeTab(std::string);

private:
	StateStack* stateStack = nullptr;
	MessageBus* messageBus = nullptr;
	friend class StateStack;
};

class StateStack final : public NonCopyable, public NonMovable {

public:

	enum class Action {
		Push,
		Pop,
		Clear
	};

	template <typename T>
	void registerState(int id)
	{
		factories[id] = [this]() {
			auto state = std::make_unique<T>(*this->messageBus);
			state->stateStack = this;
			return std::move(state);
		};
	}

	void handleMessage(const Message& message) 
	{
		for (auto it = stack.rbegin(); it != stack.rend(); it++)
			(*it)->handleMessage(message);
	}

	void handleEvent(const sf::Event& event) 
	{
		for (auto it = stack.rbegin(); it != stack.rend(); it++)
			if (!(*it)->handleEvent(event))
				break;
	}

	void update() 
	{
		for (auto it = stack.rbegin(); it != stack.rend(); it++)
			if (!(*it)->update())
				break;
	}

	void render(sf::RenderTarget& target) 
	{
		for (auto it = stack.rbegin(); it != stack.rend(); it++)
			if (!(*it)->render(target))
				break;
	}

	void pushState(int id) {
		pendingChanges.push_back({Action::Push, id});
	}

	void popState() {
		pendingChanges.push_back({Action::Pop, ArkInvalidID});
	}

	void clearStack() {
		pendingChanges.push_back({Action::Clear, ArkInvalidID});
	}

	void processPendingChanges()
	{
		for (auto changes : pendingChanges) {
			if (changes.action == Action::Push) {
				auto it = factories.find(changes.stateId);
				if (it == factories.end()) {
					EngineLog(LogSource::StateStack, LogLevel::Error, "didn't find state with id %d", changes.stateId);
					return;
				}
				auto factorie = it->second;
				stack.emplace_back(factorie());
				stack.back()->init();
			} else if (changes.action == Action::Pop) {
				stack.pop_back();
			} else if (changes.action == Action::Clear) {
				stack.clear();
			}
		}
		pendingChanges.clear();
	}

private:
	struct PendingChange {
		Action action;
		int stateId = ArkInvalidID;
	};

	std::vector<std::unique_ptr<State>> stack;
	std::map<int, std::function<std::unique_ptr<State>()>> factories;
	std::vector<PendingChange> pendingChanges;
	MessageBus* messageBus;
	friend class ArkEngine;
};
