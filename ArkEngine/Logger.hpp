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

template <typename... Args>
void EngineLog(LogSource source, LogLevel level, std::string fmt, Args&&... args)
{
	if constexpr (sizeof...(Args) != 0) {
		std::string fmt2 = tfm::format(fmt.c_str(), std::forward<Args>(args)...);
		InternalEngineLog({source, level, fmt2});
	} else {
		InternalEngineLog({source, level, fmt});
	}
}

template <typename... Args>
void GameLog(LogLevel, std::string fmt, Args&& ... args)
{

}
