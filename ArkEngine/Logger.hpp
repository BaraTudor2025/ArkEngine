#pragma once

#include <string>
#include <libs/tinyformat.hpp>

// used internally
enum class LogSource {
	EntityM,
	SystemM,
	ScriptM,
	ComponentM,
	ResourceM,
	StateStack,
	MessageBus,
	Message,
	Scene,
	Engine
};

enum class LogLevel {
	Info,
	Warning,
	Error,
	Critical,
	Debug
};

struct EngineLogData {
	LogSource source;
	LogLevel level;
	std::string text;
};

void InternalEngineLog(EngineLogData);
void InternalGameLog(std::string);

template <typename... Args>
void EngineLog(LogSource source, LogLevel level, std::string fmt, Args&&... args)
{
	if constexpr (sizeof...(Args) != 0) {
		std::string text = tfm::format(fmt.c_str(), std::forward<Args>(args)...);
		InternalEngineLog({source, level, text});
	} else {
		InternalEngineLog({source, level, fmt});
	}
}

template <typename... Args>
void GameLog(std::string fmt, Args&& ... args)
{
	if constexpr (sizeof...(Args) != 0)	{
		std::string text = tfm::format(fmt.c_str(), std::forward<Args>(args)...);
		InternalGameLog(text);
	} else {
		InternalGameLog(fmt);
	}
}
