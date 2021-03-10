#pragma once
#include <vector>
#include <variant>
#include <ranges>

enum class VectorLayout{ SOA, AOS };

template <VectorLayout TLayout>
class Vector {

public:
	struct SOA {
		std::vector<PointParticles> p;
		std::vector<int> i;
	};

	struct AOS {
		PointParticles p;
		int i;
	};

	std::variant<SOA, std::vector<AOS>> var;
	friend class iterator;

public:
	struct Proxy {
		PointParticles& p;
		int& i;
	};

private:
	class iterator {
		union U{
			typename std::vector<Vector::AOS>::iterator aosIt;
			struct {
				typename std::vector<PointParticles>::iterator p;
				typename std::vector<int>::iterator i;
			}soaIt;
		};
		U u;

	public:

		iterator(typename std::vector<Vector::SOA>::iterator aosIt)
		{
			u.aosIt = aosIt;
		}

		iterator(typename std::vector<PointParticles>::iterator pIt, typename std::vector<int>::iterator iIt)
		{
			u.soaIt.p = pIt;
			u.soaIt.i = iIt;
		}

		Proxy operator*()
		{
			if constexpr (TLayout == VectorLayout::SOA) 
				return Proxy{ *u.soaIt.p , *u.soaIt.i };
			else 
				return Proxy{ u.aosIt->p, u.aosIt->i };
		}

		iterator& operator++()
		{
			if constexpr (TLayout == VectorLayout::SOA) {
				++u.soaIt.p;
				++u.soaIt.i;
			} else {
				++u.aosIt;
			}
			return *this;
		}

		friend bool operator==(const iterator& a, const iterator& b) {
			if constexpr (TLayout == VectorLayout::SOA)
				return a.u.soaIt.p == b.u.soaIt.p; 
			else
				return a.u.aosIt == b.u.aosIt;
		}
		friend bool operator!=(const iterator& a, const iterator& b) {
			return !(a == b);
		}
	};

public:

	Proxy operator[](int i)
	{
		if constexpr (TLayout == VectorLayout::SOA) {
			auto& soa = std::get<0>(this->var);
			return Proxy{ soa.p[i], soa.i[i] };
		} else {
			auto& aos = std::get<1>(this->var);
			return Proxy{ aos[i].p, aos[i].i };
		}
	}

	iterator begin() { 
		if constexpr (TLayout == VectorLayout::SOA) {
			auto& soa = std::get<0>(this->var);
			return {soa.p.begin(), soa.i.begin() };
		}
		else {
			auto& aos = std::get<1>(this->var);
			return {aos.begin() };
		}
	}

	iterator end() { 
		if constexpr (TLayout == VectorLayout::SOA) {
			auto& soa = std::get<0>(this->var);
			return {soa.p.end(), soa.i.end() };
		}
		else {
			auto& aos = std::get<1>(this->var);
			return {aos.end() };
		}
	}

};

template <VectorLayout, typename...> class LVector;

template <typename... Types>
class LVector<VectorLayout::SOA, Types...> {
	std::tuple<std::vector<Types>...> m_vector;
	std::vector<int> nush;

	template <std::size_t... Idx>
	void internal_push_back(std::tuple<Types...> args, std::index_sequence<Idx...>) {
		(std::get<Idx>(m_vector).push_back(std::move(std::get<Idx>(args))), ...);
	}

public:
	LVector() = default;

	void push_back(Types... aargs) {
		auto args = std::tuple<Types...>(aargs...);
		internal_push_back(args, std::make_index_sequence<sizeof...(Types)>{});
	}

	auto each() {
		auto iterators = std::apply([](auto&... vectors) {
			return std::tuple<typename std::vector<Types>::iterator...>(std::prev(vectors.begin())...);
		}, m_vector);
		//return std::views::iota(0, m_vector.size())
		nush.resize(std::get<0>(m_vector).size());
		return nush
			| std::views::transform([this, iterators](int) mutable {
				return std::apply([&](auto&... iters) { 
					((++iters), ...); 
					return std::tuple<Types&...>(*iters...);
				}, iterators);
			});
		//return std::views::iota(0, m_vector.size())
		//	| std::views::transform([this](int i) {
		//		return std::apply([&](auto&... vectors) {
		//			return std::tuple<types&...>(vectors[i]...);
		//		}, m_vector);
		//	});
	}
};

template <typename... Types>
class LVector<VectorLayout::AOS, Types...> {
	std::vector<std::tuple<Types...>> m_vector;

public:
	LVector() = default;

	auto& each() {
		return m_vector;
	}
};
