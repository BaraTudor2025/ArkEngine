#pragma once
#include <vector>
#include <variant>

enum class VectorLayout{ SOA, AOS };

template <VectorLayout L>
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
			typename std::vector<Vector::SOA>::iterator soaIt;
			struct {
				std::vector<PointParticles>::iterator p;
				std::vector<int>::iterator i;
			}aosIt;
		};
		U u;
		Vector* This;

	public:
		Proxy operator*()
		{
			if constexpr (L == VectorLayout::SOA) 
				return Proxy{ u.soaIt->p , u.soaIt->i };
			else 
				return Proxy{ *u.aosIt.p, *u.aosIt.i };
		}

		iterator& operator++()
		{
			if constexpr (L == VectorLayout::SOA) {
				++u.soaIt;
			} else {
				++u.aosIt.p;
				++u.aosIt.i;
			}
			return *this;
		}
	};

public:

	Proxy operator[](int i)
	{
		if constexpr (L == VectorLayout::SOA) {
			auto Struct = std::get<0>(this->var);
			return Proxy{ Struct.p[i], Struct.i[i] };
		} else {
			auto Vec = std::get<1>(this->var);
			return Proxy{ Vec[i].p, Vec[i].i };
		}
	}

	iterator begin() { return {}; }
	iterator end() { return {}; }

};

