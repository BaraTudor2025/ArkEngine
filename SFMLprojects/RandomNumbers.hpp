#pragma once
#include <random>

enum class DistributionType { normal, uniform };

template<typename T>
struct Distribution {
	std::pair<T, T> values;
	DistributionType type = DistributionType::uniform;
};

//std::mt19937 __Private_Rng__{std::random_device()()};
//std::once_flag __Random_Engine_Flag__;

#define __RNG_ENGINE__ std::ranlux48

struct CustomRNEG : public __RNG_ENGINE__ {
	
	//using std::mt19937::mersenne_twister_engine;

	CustomRNEG(uint64_t seed) : __RNG_ENGINE__(seed) { this->generate(50'000); }

	void generate(int count)
	{
		nums.resize(count);
		for (auto& num : nums)
			num = __RNG_ENGINE__::operator()();
	}

	uint64_t operator()()
	{
		if (i == nums.size())
			i = 0;
		// basically hijack the RNG
		return nums[i++];
	}

private:
	int i = 0;
	std::vector<uint64_t> nums;
};

static inline std::mt19937 __Random_Number_Generator__{ std::random_device()() };
//static CustomRNEG __Random_Number_Generator__{ std::random_device()() };

template <typename T>
static T RandomNumber(Distribution<T> dist) 
{
	static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);


	//static std::function<int()> nums = []() {
	//	std::mt19937 rng{ std::random_device()() };
	//	std::vector<uint32_t> vec(10'000);
	//	for (auto& e : vec)
	//		e = rng();
	//	return[vec = std::move(vec)](){
	//		static int i = 0;
	//		if (i == vec.size())
	//			i = 0;
	//		return vec[i++];
	//	};
	//}();

	auto[a, b] = dist.values;

	if constexpr (std::is_floating_point_v<T>)
		if (dist.type == DistributionType::normal) {
			std::normal_distribution<T> dist(a, b);
			return dist(__Random_Number_Generator__);
		} else {
			std::uniform_real_distribution<T> dist(a, b);
			return dist(__Random_Number_Generator__);
		}

	if constexpr (std::is_integral_v<T>) {
		if (dist.type == DistributionType::uniform) {
			std::uniform_int_distribution<T> dist(a, b);
			return dist(__Random_Number_Generator__);
		}
	}
	return 0;
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
