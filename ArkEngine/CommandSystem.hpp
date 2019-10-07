#pragma once

#include "Component.hpp"
#include "System.hpp"
#include "Entity.hpp"

struct CommandTarget {
	int id;
};

struct Command {
	int flags;
	std::function<void(Entity)> command;
};

class CommandSystem : public System {
public:
	CommandSystem() : System(typeid(CommandSystem)), commands(0), pendingCommands(0), count(0)
	{
		requireComponent<CommandTarget>();
	}

	void sendCommand(Command c)
	{
		if (count >= pendingCommands.size())
			pendingCommands.push_back(std::move(c));
		else
			pendingCommands[count] = std::move(c);
		count++;
	}

	void update() override
	{
		commands.swap(pendingCommands);
		count = 0;
		for (int i = 0; i < commands.size(); i++)
			for (auto entity : getEntities())
				if (entity.getComponent<CommandTarget>().id & commands[i].flags)
					commands[i].command(entity);
	}

private:
	std::vector<Command> commands;
	std::vector<Command> pendingCommands;
	int count = 0;
};
