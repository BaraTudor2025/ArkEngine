#pragma once

#include "Core.hpp"

#include <iostream>

namespace ark {

	struct Message final {

		const int id = -1;

		template <typename T>
		const T& data() const
		{
			// TODO (message): assert(sizeof(T) == m_size)
			if (sizeof(T) != m_size)
				EngineLog(LogSource::Message, LogLevel::Warning, "Type (%s) with size %d doesn't have the message size of %d", Util::getNameOfType<T>(), sizeof(T), m_size);
			return *static_cast<T*>(m_data);
		}

	private:
		int m_size = 0;
		void* m_data = nullptr;

		friend class MessageBus;
	};
}
