/* -----------------------------------------------------------------------------------------------

meta::registerMembers<T> is used for class registration and it has the following form when specialized:

namespace meta
{

template <>
auto registerMembers<YourClass>()
{
    return members(
        member(...),
        ...
    );
}

}

! Some important details:
1) This specialization should be defined in header, because compiler needs to deduce the return type.
2) This function is called by MetaHolder during it's static member initialization, so the tuple is created
   only once and then registerMembers function is never called again.
3) registerMembers could easily be a free function, but befriending such function is hard if you want to
   be able to get pointers to private members... Writing "friend class Meta" in your preferred class
   is just much easier. Though this might be somehow fixed in the future.
4) If the class is not registered then doForAllMembers(<your function>) will do nothing,
   because the function will return empty tuple.

-------------------------------------------------------------------------------------------------*/

#pragma once

#ifdef _MSC_VER
#pragma warning (disable : 4396) // silly VS warning about inline friend stuff...
#endif

#include <cstring>
#include <type_traits>
#include <tuple>
#include <utility>
#include <array>

/* for example usage see the registration of ark::Transform for members and for enum see RandomNumbers.hpp */

#define REGISTER_MEMBERS(type) template <> inline auto registerMembers<type>()
#define REGISTER_ENUM(type) template <> inline auto registerEnum<type>()

namespace ark::meta
{
	template <typename Class>
	inline auto registerMembers();

	template <typename Class>
	constexpr auto registerName();

	namespace detail
	{
		/* MetaHolder holds all Member objects constructed via meta::registerMembers<T> call.
		 * If the class is not registered, members is std::tuple<> */

		template <typename T, typename TupleType>
		struct MetaHolder {
			static TupleType members;
			static const char* name() 
			{
				return registerName<T>();
			}
		};

		template <typename T, typename TupleType>
		TupleType MetaHolder<T, TupleType>::members = registerMembers<T>();


		/* helper functions */

		template <typename F, typename... Args, std::size_t... Idx>
		void for_tuple_impl(F&& f, const std::tuple<Args...>& tuple, std::index_sequence<Idx...>)
		{
			(f(std::get<Idx>(tuple)), ...);
		}

		template <typename F, typename... Args>
		void for_tuple(F&& f, const std::tuple<Args...>& tuple)
		{
			for_tuple_impl(f, tuple, std::index_sequence_for<Args...>{});
		}

		template <typename F>
		void for_tuple(F&& /* f */, const std::tuple<>& /* tuple */)
		{ /* do nothing */ }

		template <typename T, typename...> 
		using getFirstArg = T;

	} // end of namespace detail


	// type_list is array of types
	template <typename... Args>
	struct type_list
	{
		template <std::size_t N>
		using type = std::tuple_element_t<N, std::tuple<Args...>>;
		using indices = std::index_sequence_for<Args...>;
		static const size_t size = sizeof...(Args);
	};

	/* Enum stuff */

	template <typename T>
	struct EnumValueHolder {
		using enum_type = T;
		const char* name;
		T value;
	};

	template <typename T>
	auto enumValue(const char* name, T value)
	{
		return EnumValueHolder<T>{name, value};
	}

	template <typename... Ts>
	auto enumValues(Ts&&... args)
	{
		using EnumValueH = detail::getFirstArg<Ts...>;
		using EnumT = typename EnumValueH::enum_type;
		return std::array<EnumValueHolder<EnumT>, sizeof...(Ts)> { std::forward<Ts>(args)... };
	}
	
	template <typename T> inline auto registerEnum()
	{
		static_assert(false, "T wasn't registered");
		return std::array<T, 0>{};
	}

	template <typename T> inline const auto& getEnumValues() /* -> array<T,N> */ { 
		static auto enumVals = registerEnum<T>(); 
		return enumVals;
	}

	template <typename EnumT>
	const char* getNameOfEnumValue(const EnumT& value)
	{
		const auto& fields = getEnumValues<EnumT>();
		const char* fieldName = nullptr;
		for (const auto& field : fields)
			if (field.value == value)
				fieldName = field.name;
		return fieldName;
	}

	template <typename EnumT>
	EnumT getValueOfEnumName(const char* name)
	{
		const auto& fields = getEnumValues<EnumT>();
		for (const auto& field : fields)
			if (std::strcmp(name, field.name) == 0)
				return field.value;
		return {};
	}

	/* class members stuff */

	template <typename... Args>
	auto members(Args&&... args)
	{
		return std::make_tuple(std::forward<Args>(args)...);
	}

	// function used for registration of classes by user
	template <typename Class>
	inline auto registerMembers()
	{
		return std::make_tuple();
	}

	// function used for registration of class name by user
	template <typename Class>
	constexpr auto registerName()
	{
		return "";
	}

	// returns set name for class
	template <typename Class>
	constexpr auto getName()
	{
		return detail::MetaHolder<Class, decltype(registerMembers<Class>())>::name();
	}

	// returns the number of registered members of the class
	template <typename Class>
	constexpr std::size_t getMemberCount()
	{
		return std::tuple_size<decltype(registerMembers<Class>())>::value;
	}

	// returns std::tuple of Members
	template <typename Class>
	const auto& getMembers()
	{
		return detail::MetaHolder<Class, decltype(registerMembers<Class>())>::members;
	}

	// Check if class has registerMembers<T> specialization (has been registered)
	template <typename Class>
	constexpr bool isRegistered()
	{
		return !std::is_same<std::tuple<>, decltype(registerMembers<Class>())>::value;
	}


	template <typename T>
	struct constructor_args {
		using types = type_list<>;
	};

	template <typename T>
	using constructor_arguments = typename constructor_args<T>::types;


	// Check if Class has non-default ctor registered
	template <typename Class>
	constexpr bool ctorRegistered()
	{
		return !std::is_same<type_list<>, constructor_arguments<Class>>::value;
	}

	template <typename Class, typename F, typename = std::enable_if_t<isRegistered<Class>()>>
	void doForAllMembers(F&& f) noexcept
	{
		detail::for_tuple(std::forward<F>(f), getMembers<Class>());
	}

	// version for non-registered classes (to generate less template stuff)
	template <typename Class, typename F,
		typename = std::enable_if_t<!isRegistered<Class>()>,
		typename = void>
	void doForAllMembers(F&& /*f*/) noexcept
	{
		// do nothing! Nothing gets generated
	}

	// MemberType is Member<T, Class>
	template <typename MemberType>
	using get_member_type = typename std::decay_t<MemberType>::member_type;

	// Do F for member named 'name' with type T. It's important to pass correct type of the member
	template <typename Class, typename T, typename F>
	void doForMember(const char* name, F&& f)
	{
		doForAllMembers<Class>(
			[&](const auto& member)
			{
				if (!strcmp(name, member.getName())) {
					using MemberT = meta::get_member_type<decltype(member)>;
					static_assert((std::is_same<MemberT, T>::value) && "Member doesn't have type T");
					if constexpr (std::is_same_v<MemberT, T>)
						f(member);
				}
			}
		);
	}

	// Check if class T has member
	template <typename Class>
	bool hasMember(const char* name)
	{
		bool found = false;
		doForAllMembers<Class>(
			[&found, &name](const auto& member)
			{
				if (!strcmp(name, member.getName())) {
					found = true;
				}
			}
		);
		return found;
	}


	// Get value of the member named 'name'
	template <typename T, typename Class>
	T getMemberValue(Class& obj, const char* name)
	{
		T value;
		doForMember<Class, T>(name,
			[&value, &obj](const auto& member)
			{
				value = member.getCopy(obj);
			}
		);
		return value;
	}

	// Set value of the member named 'name'
	template <typename T, typename Class, typename V,
		typename = std::enable_if_t<std::is_constructible<T, V>::value>
	>
	void setMemberValue(Class& obj, const char* name, V&& value)
	{
		doForMember<Class, T>(name,
			[&obj, value = std::forward<V>(value)](const auto& member)
			{
				member.set(obj, value);
			}
		);
	}


	template <typename Class, typename T>
	using member_ptr_t = T Class::*;

	// reference getter/setter func pointer type
	template <typename Class, typename T>
	using ref_getter_func_ptr_t = const T& (Class::*)() const;

	template <typename Class, typename T>
	using ref_setter_func_ptr_t = void (Class::*)(const T&);

	// value getter/setter func pointer type
	template <typename Class, typename T>
	using val_getter_func_ptr_t = T(Class::*)() const;

	template <typename Class, typename T>
	using val_setter_func_ptr_t = void (Class::*)(T);

	// non const reference getter
	template <typename Class, typename T>
	using nonconst_ref_getter_func_ptr_t = T& (Class::*)();


	template <typename Class, typename T>
	class Member {
	public:
		using class_type = Class;
		using member_type = T;

		Member(const char* name, member_ptr_t<Class, T> ptr) noexcept
			: name(name), ptr(ptr) {}

		Member(const char* name, ref_getter_func_ptr_t<Class, T> getterPtr, ref_setter_func_ptr_t<Class, T> setterPtr) noexcept
			: name(name), refGetterPtr(getterPtr), refSetterPtr(setterPtr) {}

		Member(const char* name, val_getter_func_ptr_t<Class, T> getterPtr, val_setter_func_ptr_t<Class, T> setterPtr) noexcept
			: name(name), valGetterPtr(getterPtr), valSetterPtr(setterPtr) {}


		bool hasPtr() const noexcept { return ptr; }
		bool hasFuncPtrs() const noexcept { return refGetterPtr || valSetterPtr; }
		bool hasRefFuncPtrs() const noexcept { return refGetterPtr && refSetterPtr; }
		bool hasValFuncPtrs() const noexcept { return valGetterPtr && refSetterPtr; }
		bool canGetConstRef() const noexcept { return ptr || refGetterPtr; }

		const char* getName() const noexcept { return name; }
		member_ptr_t<Class, T> getPtr() const noexcept { return ptr; }
		auto getRefFuncPtrs() { return std::pair{refGetterPtr, refSetterPtr}; }
		auto getValFuncPtrs() { return std::pair{valGetterPtr, valSetterPtr}; }

		// get sets methods can be used to add support
		// for getters/setters for members instead of
		// direct access to them
		const T& get(const Class& obj) const noexcept
		{
			if (refGetterPtr) {
				return (obj.*refGetterPtr)();
			} else /* if (ptr) */ {
				return obj.*ptr;
			}
		}

		T getCopy(const Class& obj) const noexcept
		{
			if (refGetterPtr) {
				return (obj.*refGetterPtr)();
			} else if (valGetterPtr) {
				return (obj.*valGetterPtr)();
			} else /* if (ptr) */ {
				return obj.*ptr;
			}
		}

		template <typename U>
		void set(Class& obj, U&& value) const noexcept
		{
			static_assert(std::is_constructible_v<T, U>);
			if (refSetterPtr) {
				(obj.*refSetterPtr)(value);
			} else if (valSetterPtr) {
				(obj.*valSetterPtr)(std::forward<U>(value));
			} else if (ptr) {
				obj.*ptr = std::forward<U>(value);
			} 
		}

	private:
		const char* name;
		member_ptr_t<Class, T> ptr = nullptr;

		ref_getter_func_ptr_t<Class, T> refGetterPtr = nullptr;
		ref_setter_func_ptr_t<Class, T> refSetterPtr = nullptr;

		val_getter_func_ptr_t<Class, T> valGetterPtr = nullptr;
		val_setter_func_ptr_t<Class, T> valSetterPtr = nullptr;

		// T& getRef(Class& obj) const;
		//Member& addNonConstGetter(nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr);
		//bool canGetRef() const { return ptr || nonConstRefGetterPtr; }
		//nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr = nullptr;
	};

	// useful function similar to make_pair which is used so you don't have to write this:
	// Member<SomeClass, int>("someName", &SomeClass::someInt); and can just to this:
	// member("someName", &SomeClass::someInt);

	template <typename Class, typename T>
	Member<Class, T> member(const char* name, T Class::* ptr) noexcept
	{
		return Member<Class, T>(name, ptr);
	}

	template <typename Class, typename T>
	Member<Class, T> member(const char* name, ref_getter_func_ptr_t<Class, T> getterPtr, ref_setter_func_ptr_t<Class, T> setterPtr) noexcept
	{
		return Member<Class, T>(name, getterPtr, setterPtr);
	}

	template <typename Class, typename T>
	Member<Class, T> member(const char* name, val_getter_func_ptr_t<Class, T> getterPtr, val_setter_func_ptr_t<Class, T> setterPtr) noexcept
	{
		return Member<Class, T>(name, getterPtr, setterPtr);
	}

} // end of namespace meta
