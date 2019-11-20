#pragma once

#include <random>

enum class DistributionType { normal, uniform };

template<typename T>
struct Distribution {
	T a, b;
	DistributionType type = DistributionType::uniform;
};

template<typename T> Distribution(T, T)->Distribution<T>;
template<typename T> Distribution(T, T, DistributionType)->Distribution<T>;

namespace meta {

	template <> inline auto registerMembers<Distribution<float>>()
	{
		return members(
			member("lower", &Distribution<float>::a),
			member("upper", &Distribution<float>::b),
			member("type", &Distribution<float>::type)
		);
	}

	template <> inline auto registerEnum<DistributionType>()
	{
		return enumValues<DistributionType>(
			enumValue("uniform", DistributionType::uniform),
			enumValue("normal", DistributionType::normal)
		);
	}
}

static inline std::mt19937 __Random_Number_Generator__{ std::random_device()() };

template <typename T>
static T RandomNumber(Distribution<T> arg) noexcept
{
	static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

	if constexpr (std::is_floating_point_v<T>)
		if (arg.type == DistributionType::normal) {
			std::normal_distribution<T> dist(arg.a, arg.b);
			return dist(__Random_Number_Generator__);
		} else {
			std::uniform_real_distribution<T> dist(arg.a, arg.b);
			return dist(__Random_Number_Generator__);
		}
	else if constexpr (std::is_integral_v<T>) {
		if (arg.type == DistributionType::uniform) {
			std::uniform_int_distribution<T> dist(arg.a, arg.b);
			return dist(__Random_Number_Generator__);
		}
	}
	// daca am ajuns aici ceva nu e in regula
	std::abort();
}

template <typename T>
inline T RandomNumber(T a, T b, DistributionType dist = DistributionType::uniform)
{
	return RandomNumber<T>({ a, b , dist });
}
