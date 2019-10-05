#pragma once

#include "Core.hpp"

#include <iostream>

struct Message final {

	const int id = -1;
	
	template <typename T>
	const T& data() const
	{
		// TODO (message): assert(sizeof(T) == m_size)
		if (sizeof(T) != m_size)
			std::cerr << " Message: sizeof(T) with T [" << typeid(T).name() << "] is not equal with message size\n\n";
		return *static_cast<T*>(m_data);
	}

private:
	int m_size = 0;
	void* m_data = nullptr;

	friend class MessageBus;
};
