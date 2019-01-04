#pragma once
#include <random>

enum class DistributionType { normal, uniform };

template<typename T>
struct Distribution {
	std::pair<T, T> values;
	DistributionType type = DistributionType::uniform;
};

static std::mt19937 _Private_Rng_{std::random_device()()};

template <typename T>
static T RandomNumber(Distribution<T> dist) {
	static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

	auto[a, b] = dist.values;

	if constexpr (std::is_floating_point_v<T>)
		if (dist.type == DistributionType::normal) {
			std::normal_distribution<T> dist(a, b);
			return dist(_Private_Rng_);
		} else {
			std::uniform_real_distribution<T> dist(a, b);
			return dist(_Private_Rng_);
		}

	if constexpr (std::is_integral_v<T>) {
		if (dist.type == DistributionType::uniform) {
			std::uniform_int_distribution<T> dist(a, b);
			return dist(_Private_Rng_);
		}
	}
}

template <typename T>
inline T RandomNumber(std::pair<T, T> args, DistributionType distribution = DistributionType::uniform)
{
	return RandomNumber<T>({ args, distribution});
}

template <typename T>
inline T RandomNumber(T a, T b, DistributionType dist = DistributionType::uniform)
{
	return RandomNumber<T>({ a, b }, dist);
}
