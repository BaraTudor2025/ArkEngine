#pragma once

#include "Core.hpp"
#include "Message.hpp"
#include "ark/util/Util.hpp"

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

			Message* m = new(inPointer)Message();
			inPointer += sizeof(Message);
			m->id = id;
			m->m_size = sizeof(T);
			m->m_data = new(inPointer)T();
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
			//if(auto funcs = findDelegate(message->type); funcs)
			//	for (auto& fun : *funcs)
			//		fun(*message);
			return true;
		}

		//void addObserver(std::type_index type, std::function<void(const Message&)> func) {
		//	if (auto funcs = findDelegate(type)) {
		//		funcs->emplace_back(std::move(func));
		//	}
		//	else {
		//		callbacks.push_back({ type, {std::move(func)} });
		//	}
		//}

	private:

		//auto findDelegate(std::type_index type) -> std::vector<std::function<void(const Message&)>>* {
		//	if (auto it = std::find_if(callbacks.begin(), callbacks.end(), [&](const auto& val) { return type == val.first; });
		//		it != callbacks.end())
		//		return &it->second;
		//	else
		//		return nullptr;
		//}

		//std::vector<std::pair<std::type_index, std::vector<std::function<void(const Message&)>>>> callbacks;
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
