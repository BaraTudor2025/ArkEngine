#pragma once

#include <random>
#include "ark/ecs/Meta.hpp"
#include "ark/ecs/DefaultServices.hpp"

enum class DistributionType { normal, uniform };

template<typename T>
struct Distribution {
	T a, b;
	DistributionType type = DistributionType::uniform;
};

template<typename T> Distribution(T, T)->Distribution<T>;
template<typename T> Distribution(T, T, DistributionType)->Distribution<T>;

ARK_REGISTER_COMPONENT_WITH_NAME_TAG(Distribution<float>, "Distribution", Distribution_float, registerServiceDefault<Distribution<float>>())
{
	return members(
		member_property("lower", &Distribution<float>::a),
		member_property("upper", &Distribution<float>::b),
		member_property("type", &Distribution<float>::type)
	);
}

ARK_REGISTER_ENUM(DistributionType)
{
	return enumValues(
		enumValue("uniform", DistributionType::uniform),
		enumValue("normal", DistributionType::normal)
	);
}

//namespace ark::meta {
//
//	//template <> inline auto registerMembers<Distribution<float>>()
//	//{
//	//}
//
//	template <> inline auto registerEnum<DistributionType>()
//	{
//	}
//}


namespace detail {

	/* from author Leandros, gist: https://gist.github.com/Leandros/6dc334c22db135b033b57e9ee0311553 */

	/* Copyright (c) 2018 Arvid Gerstmann. */
	/* This code is licensed under MIT license. */
	class splitmix
	{
	public:
		using result_type = uint32_t;
		static constexpr result_type(min)() { return 0; }
		static constexpr result_type(max)() { return UINT32_MAX; }
		friend bool operator==(splitmix const&, splitmix const&);
		friend bool operator!=(splitmix const&, splitmix const&);

		splitmix() : m_seed(1) {}
		explicit splitmix(std::random_device& rd)
		{
			seed(rd);
		}

		void seed(std::random_device& rd) noexcept
		{
			m_seed = uint64_t(rd()) << 31 | uint64_t(rd());
		}

		result_type operator()() noexcept
		{
			uint64_t z = (m_seed += UINT64_C(0x9E3779B97F4A7C15));
			z = (z ^ (z >> 30))* UINT64_C(0xBF58476D1CE4E5B9);
			z = (z ^ (z >> 27))* UINT64_C(0x94D049BB133111EB);
			return result_type((z ^ (z >> 31)) >> 31);
		}

		void discard(unsigned long long n) noexcept
		{
			for (unsigned long long i = 0; i < n; ++i)
				operator()();
		}

	private:
		uint64_t m_seed;
	};

	static bool operator==(splitmix const& lhs, splitmix const& rhs)
	{
		return lhs.m_seed == rhs.m_seed;
	}
	static bool operator!=(splitmix const& lhs, splitmix const& rhs)
	{
		return lhs.m_seed != rhs.m_seed;
	}

	class xorshift
	{
	public:
		using result_type = uint32_t;
		static constexpr result_type(min)() { return 0; }
		static constexpr result_type(max)() { return UINT32_MAX; }
		friend bool operator==(xorshift const&, xorshift const&);
		friend bool operator!=(xorshift const&, xorshift const&);

		xorshift() : m_seed(0xc1f651c67c62c6e0ull) {}
		explicit xorshift(std::random_device& rd) noexcept
		{
			seed(rd);
		}

		void seed(std::random_device& rd) noexcept
		{
			m_seed = uint64_t(rd()) << 31 | uint64_t(rd());
		}

		result_type operator()() noexcept
		{
			uint64_t result = m_seed * 0xd989bcacc137dcd5ull;
			m_seed ^= m_seed >> 11;
			m_seed ^= m_seed << 31;
			m_seed ^= m_seed >> 18;
			return uint32_t(result >> 32ull);
		}

		void discard(unsigned long long n) noexcept
		{
			for (unsigned long long i = 0; i < n; ++i)
				operator()();
		}

	private:
		uint64_t m_seed;
	};

	static bool operator==(xorshift const& lhs, xorshift const& rhs)
	{
		return lhs.m_seed == rhs.m_seed;
	}
	static bool operator!=(xorshift const& lhs, xorshift const& rhs)
	{
		return lhs.m_seed != rhs.m_seed;
	}

	class pcg
	{
	public:
		using result_type = uint32_t;
		static constexpr result_type(min)() { return 0; }
		static constexpr result_type(max)() { return UINT32_MAX; }
		friend bool operator==(pcg const&, pcg const&);
		friend bool operator!=(pcg const&, pcg const&);

		pcg() noexcept
			: m_state(0x853c49e6748fea9bULL)
			, m_inc(0xda3e39cb94b95bdbULL)
		{}
		explicit pcg(std::random_device& rd) noexcept
		{
			seed(rd);
		}

		void seed(std::random_device& rd) noexcept
		{
			uint64_t s0 = uint64_t(rd()) << 31 | uint64_t(rd());
			uint64_t s1 = uint64_t(rd()) << 31 | uint64_t(rd());

			m_state = 0;
			m_inc = (s1 << 1) | 1;
			(void)operator()();
			m_state += s0;
			(void)operator()();
		}

		result_type operator()() noexcept
		{
			uint64_t oldstate = m_state;
			m_state = oldstate * 6364136223846793005ULL + m_inc;
			uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
			int rot = oldstate >> 59u;
			return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
		}

		void discard(unsigned long long n) noexcept
		{
			for (unsigned long long i = 0; i < n; ++i)
				operator()();
		}

	private:
		uint64_t m_state;
		uint64_t m_inc;
	};

	static bool operator==(pcg const& lhs, pcg const& rhs)
	{
		return lhs.m_state == rhs.m_state
			&& lhs.m_inc == rhs.m_inc;
	}
	static bool operator!=(pcg const& lhs, pcg const& rhs)
	{
		return lhs.m_state != rhs.m_state
			|| lhs.m_inc != rhs.m_inc;
	}

}

//static inline std::mt19937 __Random_Number_Generator__{ std::random_device()() };
static inline std::random_device _ark_rng_device;
static inline detail::splitmix __Random_Number_Generator__{_ark_rng_device};

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
inline T RandomNumber(T a, T b, DistributionType dist = DistributionType::uniform) noexcept
{
	return RandomNumber<T>({ a, b , dist });
}
