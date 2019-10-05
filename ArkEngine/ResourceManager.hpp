#pragma once

#include <unordered_map>
#include <typeindex>
#include <string>
#include <exception>
#include <functional>

// TODO (resource manager): make loader return an optional default resource, if the optional is null then abort program
struct Resources {

	// f must take void* as the first argument and cast it to T*, and the filename as the second arg
	// f must also return true if loading was a success, false otherwise
	// e.g.: bool f(void*, std::string fileName)

	template <typename T, typename F>
	static void addLoader(F f)
	{
		loaders[typeid(T)] = f;
	}

	template <typename T>
	static T* load(const std::string& fileName)
	{
		static std::unordered_map<std::string, T> cache;

		auto itRes = cache.find(fileName);
		if (itRes == cache.end()) {
			T temp;
			auto itLoader = loaders.find(typeid(T));
			if (itLoader == loaders.end()) {
				std::cout << "\n\n loader for (" << typeid(T).name() << ") was not added\n";
				std::abort();
			} else if (!itLoader->second(&temp, "./res/" + fileName)) {
				std::string msg = "couldn't load file: " + fileName;
				std::cout << "\n\n" << msg << "\n\n";
				throw std::runtime_error(msg);
			}
			return &(cache[fileName] = std::move(temp));
		} else {
			return &(itRes->second);
		}
	}

	template <typename T>
	static bool sfmlResourceLoader(void* res, std::string fileName)
	{
		return static_cast<T*>(res)->loadFromFile(fileName);
	}

private:
	static std::unordered_map<std::type_index, std::function<bool(void*, std::string)>> loaders;
};
