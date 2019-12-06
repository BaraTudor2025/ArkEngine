#pragma once

#include <unordered_map>
#include <typeindex>
#include <string>
#include <exception>
#include <functional>
#include <any>

namespace ark {

	// TODO (resource manager): make loader return an optional default resource, if the optional is null then abort program
	struct Resources {

		static inline const std::string resourceFolder = "./assets/";

		struct Handler {
			std::string folder;
			std::function<std::any(std::string)> load; // takes file path as parameter
		};

		template <typename T, typename F>
		static void addHandler(std::string folder, F f)
		{
			handlers[typeid(T)] = Handler{folder, f};
		}

		template <typename T>
		static void addHandler(Handler handler)
		{
			handlers[typeid(T)] = std::move(handler);
		}

		template <typename T>
		static T* load(const std::string& file)
		{
			static std::unordered_map<std::string, T> cache;

			auto cachedValueIt = cache.find(file);
			if (cachedValueIt != cache.end()) {
				return &(cachedValueIt->second);
			} else {
				auto handlerIt = handlers.find(typeid(T));
				if (handlerIt == handlers.end()) {
					EngineLog(LogSource::ResourceM, LogLevel::Error, "aborting... handler for (%s) was not added", Util::getNameOfType<T>());
					std::abort();
				} else {
					auto& handler = handlerIt->second;
					std::any resource = handler.load(resourceFolder + handler.folder + "/" + file);
					if (!resource.has_value()) {
						EngineLog(LogSource::ResourceM, LogLevel::Error, "aborting... handler for (%s) didn't return a value", Util::getNameOfType<T>());
						std::abort();
					}
					cache[file] = std::any_cast<T&&>(std::move(resource));
					return &cache[file];
				}
			}
		}

		template <typename T>
		static std::any load_SFML_resource(std::string fileName)
		{
			T resource;
			resource.loadFromFile(fileName);
			return resource;
		}

	private:
		static std::unordered_map<std::type_index, Handler> handlers;
	};
}
