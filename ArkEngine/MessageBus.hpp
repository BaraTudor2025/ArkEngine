#pragma once

#include "Core.hpp"

struct Message final {

	int id = -1;
	
	template <typename T>
	const T& data()
	{
		// TODO (message): assert(sizeof(T) == m_size)
		return *static_cast<*T>(m_data);
	}

private:
	int m_size = 0;
	void* m_data = nullptr;

	friend class MessageBus;
};

class MessageBus {

public:

	MessageBus() 
		: inBuffer(128 * 64),
		outBuffer(128 * 64), 
		inPointer(inBuffer.data()),
		outPointer(outBuffer.data()),
		currentCount(0),
		pendingCount(0) 
	{ }
	
	template <typename T, typename...Args>
	T* post(int id)
	{
		// TODO (message bus): assert ca nu am depasit nr max de bytes

		//Message* msg = new(inPointer)Message();
		Message* m = construct_in_place<Message>(inPointer);
		inPointer += sizeof(Message);
		m->id = id;
		m->m_size = sizeof(T);
		m->m_data = construct_in_place<T>(inPointer);
		inPointer += sizeof(T);

		pendingCount++;

		return static_cast<T*>(m->m_data);

	}

	bool pool(Message& message)
	{
		if (currentCount == 0) {
			outBuffer.swap(inBuffer);
			inPointer = inBuffer.data();
			outPointer = outBuffer.data();
			currentCount = pendingCount;
			pendingCount = 0;
			return false;
		}
		message = *reinterpret_cast<Message*>(outPointer);
		outPointer += (sizeof(Message) + message.m_size);
		currentCount--;
		return true;
	}

private:

	template <typename T, typename P>
	inline T* construct_in_place(P p)
	{
		return new(p)T();
	}

private:
	std::vector<uint8_t> inBuffer;
	std::vector<uint8_t> outBuffer;
	uint8_t* inPointer;
	uint8_t* outPointer;
	int currentCount;
	int pendingCount;
};
