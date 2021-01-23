#pragma once

#include "ark/core/Core.hpp"
#include "ark/core/Logger.hpp"

#include <iostream>
#include <typeindex>

namespace ark {

	struct Message final {

		std::type_index type = typeid(void);

		template <typename T>
		bool is() const {
			return typeid(T) == this->type;
		}

		template <typename T>
		const T& data() const
		{
			assert(typeid(T) == type);
			if (typeid(T) != type)
				EngineLog(LogSource::Message, LogLevel::Error, "Type (%s) with size %d doesn't have the message size of %d", typeid(T).name(), sizeof(T), m_size);
			return *static_cast<T*>(m_data);
		}

		template <typename T>
		const T* tryData() const {
			return this->is<T>() ? &this->data<T>() : nullptr;
		}

	private:
		int m_size = 0;
		void* m_data = nullptr;

		friend class MessageBus;
	};
}
