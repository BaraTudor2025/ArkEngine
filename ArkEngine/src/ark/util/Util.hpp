#pragma once

#include "SFML/System/Vector2.hpp"
#include "SFML/Graphics/Rect.hpp"

#include <type_traits>
#include <mutex>
#include <vector>
#include <sstream>
#include <optional>
#include <string>
#include <typeindex>


#define CONCAT_IMPL( x, y ) x##y
#define MACRO_CONCAT( x, y ) CONCAT_IMPL( x, y )
#define UNIQUE_NAME(x) MACRO_CONCAT(x, __COUNTER__)


#define log_impl(fmt, ...) printf(__FUNCTION__ ": " fmt "\n", __VA_ARGS__ )
//#define log(fmt, ...)  log_impl(fmt, __VA_ARGS__)

#define log_err_impl(fmt, ...) do { log(fmt, __VA_ARGS__); std::exit(EXIT_FAILURE); } while(false)
#define log_err(fmt, ...) log_err_impl(fmt, __VA_ARGS__)

#define INSTANCE(type) type type::instance;

#define COPYABLE(type) \
	type(const type&) = default; \
	type& operator=(const type&) = default;

#define MOVABLE(type) \
	type(type&&) = default; \
	type& operator=(type&&) = default;

static inline constexpr int ArkInvalidIndex = -1;
static inline constexpr int ArkInvalidID = -1;
//static inline constexpr int ark_invalid_index = -1;

struct NonCopyable {
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

struct NonMovable {
	NonMovable() = default;
	NonMovable(NonMovable&&) = delete;
	NonMovable& operator=(NonMovable&&) = delete;
};

template <typename T, typename U>
sf::Vector2<T> operator*(sf::Vector2<T> v1, sf::Vector2<U> v2)
{
	return { v1.x * v2.x, v1.y * v2.y };
}

template <typename T, typename U>
sf::Vector2<T> operator/(sf::Vector2<T> v1, sf::Vector2<U> v2)
{
	return { v1.x / v2.x, v1.y / v2.y };
}

namespace Util
{

	template <typename...Ts>
	struct overloaded : Ts... {
		using Ts::operator()...;
	};

	template <typename...Ts> overloaded(Ts...)->overloaded<Ts...>;

	template <typename R, typename T>
	inline R reinterpretCast(T in)
	{
		static_assert(sizeof(T) == sizeof(R));
		union {
			T in;
			R ret;
		}u{ in };
		return u.ret;
	}

	template <typename T1, typename T2>
	auto set_difference(const T1& range1, const T2& range2) -> std::vector<typename T1::value_type>
	{
		std::vector<typename T1::value_type> diff;
		std::set_difference(range1.begin(), range1.end(), range2.begin(), range2.end(), std::back_inserter(diff));
		return diff;
	}

	template <typename T>
	void push_back_range(T& range, const T& inserted)
	{
		range.insert(range.end(), inserted.begin(), inserted.end());
	}

	template <typename T>
	sf::Vector2f centerOfRect(sf::Rect<T> rect)
	{
		return { rect.left + rect.width / 2.f, rect.top + rect.height / 2.f };
	}

	inline std::vector<std::string> splitOnSpace(std::string string)
	{
		std::stringstream ss(string);
		std::vector<std::string> v;
		std::string s;
		while (ss >> s)
			v.push_back(s);
		return v;
	}

	template <typename T, typename U>
	inline void erase(T& range, const U& elem)
	{
		range.erase(std::remove(range.begin(), range.end(), elem), range.end());
	}

	template <typename T, typename F>
	inline void erase_if(T& range, F predicate)
	{
		range.erase(std::remove_if(range.begin(), range.end(), predicate), range.end());
	}

	template <typename T>
	inline void erase_at(T& range, int index)
	{
		range.erase(range.begin() + index);
	}

	// return std::optional<U&> or std::optional<U> if U is pointer
	template <typename T, typename U>
	inline auto find(const T& range, const U& elem) -> std::optional<std::conditional_t<std::is_pointer_v<U>, U, U&>>
	{
		static_assert(std::is_same_v<typename T::value_type, U>, "'elem' must have the same type as the range elements");
		auto it = std::find(std::begin(range), std::end(range), elem);

		if (it == std::end(range))
			return std::nullopt;
		else {
			if constexpr (std::is_pointer_v<U>)
				return std::optional<U>(*it);
			else
				return std::optional<U&>(*it);
		}
	}

	template <typename T, typename F, typename U>
	inline std::optional<U&> find_if(const T& range, F f)
	{
		auto it = std::find_if(std::begin(range), std::end(range), f);
		if (it == std::end(range))
			return {};
		else
			return *it;
	}

	// return index of the element, InvalidIndex if not found
	template <typename T, typename U>
	inline int get_index(const T& range, const U& elem)
	{
		auto pos = std::find(std::begin(range), std::end(range), elem);
		if (pos == std::end(range)) {
			return ArkInvalidIndex;
		}
		return pos - std::begin(range);
	}

	template <typename T, typename F>
	inline int get_index_if(const T& range, const F& f)
	{
		auto pos = std::find_if(std::begin(range), std::end(range), f);
		if (pos == std::end(range)) {
			return ArkInvalidIndex;
		}
		return pos - std::begin(range);
	}

	struct PolarVector {
		float magnitude;
		float angle;
	};

	inline sf::Vector2f normalize(sf::Vector2f vec)
	{
		return vec / std::hypot(vec.x, vec.y);
	}

	inline float toRadians(float deg)
	{
		return deg / 180 * 3.14159f;
	}

	inline float toDegrees(float rad)
	{
		return rad / 3.14159f * 180;
	}

	inline PolarVector toPolar(sf::Vector2f vec)
	{
		auto [x, y] = vec;
		auto magnitude = std::sqrt(x * x + y * y);
		auto angle = std::atan2(y, x);
		return { magnitude, angle };
	}

	inline sf::Vector2f toCartesian(PolarVector vec)
	{
		auto [r, fi] = vec;
		auto x = r * std::cos(fi);
		auto y = r * std::sin(fi);
		return { x, y };
	}

#if 0
	template <typename T>
	class Sync {

	public:
		template <typename... Args>
		Sync(Args&& ... args) : var(std::forward<Args>(args)...) {}

		T& var() { return var_; }

		T* operator->()
		{
			std::lock_guard<std::mutex> lk();
			return &var_;
		}

		void lock() { mtx.lock(); }
		void unlock() { mtx.unlock(); }

	private:
		std::mutex mtx;
		T var_;
	};

	/* Params
	 * f : function, if is member func then pass 'this' as the next args
	 * this: if f is member func
	 * v : range with member 'count', represents how many elements in each span
	 * spans...
	*/
	template <typename F, typename...Args>
	inline void forEachSpan(F f, Args&& ...args)
	{
		if constexpr (std::is_member_function_pointer_v<F>)
			forEachSpanImplMemberFunc(f, std::forward<Args>(args)...);
		else
			forEachSpanImplNormalFunc(f, std::forward<Args>(args)...);
	}

	template<typename F, typename V, typename...Spans>
	inline void forEachSpanImplNormalFunc(F f, V& v, Spans& ...spans)
	{
		int pos = 0;
		for (auto p : v) {
			size_t count;
			if constexpr (std::is_pointer_v<V::value_type>)
				count = p->count;
			else
				count = p.count;

			std::invoke(
				f,
				*p,
				gsl::span<Spans::value_type>{ spans.data() + pos, count } ...);

			if constexpr (std::is_pointer_v<V::value_type>)
				pos += p->count;
			else
				pos += p.count;
		}
	}

	template<typename F, class C, typename V, typename...Spans>
	inline void forEachSpanImplMemberFunc(F f, C* pThis, V& v, Spans& ...spans)
	{
		int pos = 0;
		for (auto p : v) {
			size_t count;
			if constexpr (std::is_pointer_v<V::value_type>)
				count = p->count;
			else
				count = p.count;

			std::invoke(
				f,
				pThis,
				*p,
				gsl::span<Spans::value_type>{ spans.data() + pos, count } ...);

			if constexpr (std::is_pointer_v<V::value_type>)
				pos += p->count;
			else
				pos += p.count;
		}
	}
#endif

} //namespace Util

#if false
template <typename R, typename... Args>
std::function<R(Args...)> memo(R(*fn)(Args...))
{
	std::map<std::tuple<Args...>, R> table;
	return [fn, table](Args... args) mutable -> R {
		auto argt = std::make_tuple(args...);
		auto memoized = table.find(argt);
		if (memoized == table.end()) {
			auto result = fn(args...);
			table[argt] = result;
			return result;
}
		else {
			return memoized->second;
		}
	};
}
#endif