#pragma once

#include "Core.hpp"
#include "Message.hpp"

#include <vector>
#include <functional>
#include <type_traits>

namespace ark {

	class MessageBus final {

	public:

		MessageBus()
			: inBuffer(128 * 64),
			outBuffer(128 * 64),
			inPointer(inBuffer.data()),
			outPointer(outBuffer.data()),
			currentCount(0),
			pendingCount(0),
			inDestructors(0),
			outDestructors(0),
			inDtorCount(0),
			outDtorCount(0)
		{}

		template <typename T>
		T* post(int id)
		{
			// TODO (message bus): assert ca nu am depasit nr max de bytes

			Message* m = Util::construct_in_place<Message>(inPointer);
			inPointer += sizeof(Message);
			const_cast<int&>(m->id) = id; // so sue me
			m->m_size = sizeof(T);
			m->m_data = Util::construct_in_place<T>(inPointer);
			inPointer += sizeof(T);
			pendingCount++;

			T* data = static_cast<T*>(m->m_data);

			if constexpr (!std::is_trivially_destructible_v<T>) {
				// keep count so that we dont always allocate a new function
				if (inDtorCount == inDestructors.size())
					inDestructors.push_back([data]() { data->~T(); });
				else
					inDestructors.at(inDtorCount) = [data]() { data->~T(); };

				inDtorCount++;
			}

			return data;
		}

		bool pool(Message*& message)
		{
			if (currentCount == 0) {
				for (int i = 0; i < outDtorCount; i++)
					outDestructors[i]();

				outDestructors.swap(inDestructors);
				outDtorCount = inDtorCount;
				inDtorCount = 0;

				outBuffer.swap(inBuffer);
				inPointer = inBuffer.data();
				outPointer = outBuffer.data();
				currentCount = pendingCount;
				pendingCount = 0;

				return false;
			}
			message = reinterpret_cast<Message*>(outPointer);
			outPointer += (sizeof(Message) + message->m_size);
			currentCount--;
			return true;
		}

	private:
		std::vector<uint8_t> inBuffer;
		std::vector<uint8_t> outBuffer;
		uint8_t* inPointer;
		uint8_t* outPointer;
		int currentCount;
		int pendingCount;
		std::vector<std::function<void()>> inDestructors;
		std::vector<std::function<void()>> outDestructors;
		int inDtorCount;
		int outDtorCount;
	};
}
