#pragma once

#include <unordered_map>

// TODO (resource manager): remove specialized folders(constexpr) and let the user choose what folder he wants to load from; keep ./res
template <typename T>
inline T* load(const std::string& fileName)
{
	static std::unordered_map<std::string, T> cache;

	std::string folder;
	if constexpr (std::is_same_v<T, sf::Texture>)
		folder = "textures/";
	if constexpr (std::is_same_v<T, sf::Font>)
		folder = "fonts/";
	if constexpr (std::is_same_v<T, sf::Image>)
		folder = "imags/";

	auto it = cache.find(fileName);
	if (it == cache.end()) {
		T temp;
		if (!temp.loadFromFile("./res/" + folder + fileName))
			throw std::runtime_error("nu am gasit fisierul: " + folder + fileName);
		return &(cache[fileName] = temp);
	}
	return &(it->second);
}
