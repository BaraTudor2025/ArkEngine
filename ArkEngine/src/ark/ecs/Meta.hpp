/* initial code taken from Elias Daler https://github.com/eliasdaler/MetaStuff/tree/cpp17 licensed under the MIT licence*/
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
#include <typeindex>

/* for example usage see the registration of ark::Transform for members and for enum see RandomNumbers.hpp */

#define CONCAT_IMPL( x, y ) x##y
#define MACRO_CONCAT( x, y ) CONCAT_IMPL( x, y )
#define UNIQUE_NAME(x) MACRO_CONCAT(x, __COUNTER__)
#define RUN_CODE(...) \
	inline auto UNIQUE_NAME(_ark_gfunc) = []() {__VA_ARGS__ return 0;} ()

#define REGISTER_SERVICES(TYPE, NAME, ...) \
	namespace ark::meta { \
		RUN_CODE(using Type = TYPE; constructMetadataFrom<TYPE>(NAME); services<TYPE>(__VA_ARGS__);); \
	}

#define ARK_REGISTER_TYPE(TYPE, NAME, ...) \
	REGISTER_SERVICES(TYPE, NAME, __VA_ARGS__) \
	template <> inline auto ::ark::meta::registerMembers<TYPE>()

#define ARK_REGISTER_ENUM(TYPE) \
	template <> inline auto ::ark::meta::registerEnum<TYPE>()

namespace ark::meta
{
	template <typename Class>
	inline auto registerMembers();

	struct Metadata {
	private:
		Metadata(std::type_index type, std::size_t size, std::size_t align, std::string_view name)
			: type(type), size(size), align(align), name(name) { }
	public:
		std::type_index type;
		std::size_t size;
		std::size_t align;
		std::string_view name;

		void(*default_constructor)(void*) = nullptr;
		void(*destructor)(void*) = nullptr;

		// add trivial and noexcept ctor??
		void(*copy_constructor)(void*, const void*) = nullptr;
		void(*copy_assing)(void*, const void*) = nullptr;

		void(*move_constructor)(void*, void*) = nullptr;
		void(*move_assign)(void*, void*) = nullptr;

		// exp: services["EDITOR"] = SceneInspector::renderFieldsOfType<T>;
		// exp: services["SERIALIZE-JSON"] = serialize_value<T>;
		// exp: services["NEW-UNIQUE"] = lambdaToPtr([]()->unique_ptr<TBase> { return make_unique<T>();});
		std::unordered_map<std::string_view, void*> services;

		template <typename T>
		friend void constructMetadataFrom(std::string_view name);
	};


	namespace detail
	{
		/* sMembers holds all Member objects constructed via meta::registerMembers<T> call.
		 * If the class is not registered, sMembers is std::tuple<> */

		template <typename T>
		static inline const auto sMembers = registerMembers<T>();

		// bagamiaspulanmicrosoft ca aparent initializeaza mai intai variabilele statice inline din clasa si dupa pe cele globale statice
		// asa ca tre sa pun sTypeTable intr un struct ca sa ma asigur ca e construit inainte de varialbilele anonime din macroul REGISTER_SERVICES
		//struct bagamiaspula {
		struct guard {
			static inline std::unordered_map<std::type_index, Metadata> sTypeTable;
		};

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


		template <typename T>
		constexpr bool areTheSameType() { return true; }

		template <typename T, typename U, typename... Ts>
		constexpr bool areTheSameType() { return std::is_same_v<T, U> && areTheSameType<U, Ts...>(); }


		template <typename F>
		struct make_func_ptr : make_func_ptr<decltype(&F::operator())> {};

		template <typename Ret, typename... Args>
		struct make_func_ptr<Ret(*)(Args...)> {
			using type = Ret(*)(Args...);
		};

		template <typename Ret, typename... Args>
		struct make_func_ptr<Ret(Args...)> {
			using type = Ret(*)(Args...);
		};

		template <typename Cls, typename Ret, typename... Args>
		struct make_func_ptr<Ret(Cls::*)(Args...) const> {
			using type = Ret(*)(Args...);
		};

		template <typename T>
		using make_func_ptr_t = typename make_func_ptr<T>::type;

		template <typename L>
		void* lambdaToPtr(L&& lambda)
		{
			return static_cast<detail::make_func_ptr_t<L>>(lambda);
		}


		template <typename T, typename...> 
		using getFirstArg = T;

		template <typename... Args>
		struct type_list
		{
			template <std::size_t N>
			using type = std::tuple_element_t<N, std::tuple<Args...>>;
			using indices = std::index_sequence_for<Args...>;
			static const size_t size = sizeof...(Args);
		};

	} // end of namespace detail

	template <typename T>
	static inline auto dummy = 0;

	template <typename T>
	void constructMetadataFrom(std::string_view name)
	{
		Metadata metadata{ typeid(T), sizeof(T), alignof(T), name };

		if constexpr (std::is_default_constructible_v<T>)
			metadata.default_constructor = [](void* This) { new(This)T{}; };

		if constexpr (std::is_copy_constructible_v<T>)
			metadata.copy_constructor = [](void* This, const void* That) { new (This)T(*static_cast<const T*>(That)); };

		if constexpr (std::is_copy_assignable_v<T>)
			metadata.copy_assing = [](void* This, const void* That) { *static_cast<T*>(This) = *static_cast<const T*>(That);/* *(T*)This = *(const T*)That; */ };

		if constexpr (std::is_move_constructible_v<T>)
			metadata.move_constructor = [](void* This, void* That) { new(This)T(std::move(*static_cast<T*>(That))); };

		if constexpr (std::is_move_assignable_v<T>)
			metadata.move_assign = [](void* This, void* That) { *static_cast<T*>(This) = std::move(*static_cast<T*>(That)); };

		if constexpr (not std::is_trivially_destructible_v<T> && std::is_destructible_v<T>)
			metadata.destructor = [](void* This) { static_cast<T*>(This)->~T(); };
		
		detail::guard::sTypeTable.emplace(typeid(T), std::move(metadata));
		//std::cout << "ctor: " << typeid(T).name() << '\n' ;
	}

	inline const Metadata* getMetadata(std::type_index type)
	{
		if (detail::guard::sTypeTable.count(type))
			return &detail::guard::sTypeTable.at(type);
		else
			return nullptr;
	}

	template <typename T, typename... Args>
	inline void services(Args&&... args)
	{
		if constexpr (sizeof...(Args) > 0) {
			Metadata& m = detail::guard::sTypeTable.at(typeid(T));
			((m.services[args.first] = args.second), ...);
		}
	}

	template <typename F>
	auto service(std::string_view name, F fun) -> std::pair<std::string_view, void*>
	{
		static_assert(std::is_function_v<std::remove_pointer_t<F>> || std::is_class_v<F>, "fun must be function or non-capturing lambda");

		if constexpr (std::is_object_v<F>) // is lambda
			return { name, static_cast<detail::make_func_ptr_t<F>>(fun) };
		else
			return { name, fun };
	}

	template <typename F>
	auto getService(const Metadata& m, std::string_view serviceName) -> detail::make_func_ptr_t<F>
	{
		using FP = detail::make_func_ptr_t<F>;
		return reinterpret_cast<FP>(m.services.at(serviceName));
	}

	template <typename T, typename F>
	auto getService(std::string_view serviceName) -> detail::make_func_ptr_t<F>
	{
		using FP = detail::make_func_ptr_t<F>;
		if(auto m = getMetadata(typeid(T)); m)
			return reinterpret_cast<FP>(m->services(serviceName));
		else
			return nullptr;
	}


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
		static_assert(std::is_enum_v<T>, "tre sa fie valoare de enum");
		return EnumValueHolder<T>{name, value};
	}

	template <typename... Ts>
	auto enumValues(Ts&&... args)
	{
		static_assert(detail::areTheSameType<Ts...>(), "de ce plm sunt doua enum-uri diferite???");
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

	// returns the number of registered members of the class
	template <typename Class>
	constexpr std::size_t getMemberCount()
	{
		return std::tuple_size<decltype(registerMembers<Class>())>::value;
	}


	// Check if class has registerMembers<T> specialization (has been registered)
	template <typename Class>
	constexpr bool isRegistered()
	{
		return !std::is_same<std::tuple<>, decltype(registerMembers<Class>())>::value;
	}

	template <typename Class, typename F, typename = std::enable_if_t<isRegistered<Class>()>>
	void doForAllMembers(F&& f) noexcept
	{
		detail::for_tuple(std::forward<F>(f), detail::sMembers<Class>);
	}

	// version for non-registered classes (to generate less template stuff)
	template <typename Class, typename F,
		typename = std::enable_if_t<!isRegistered<Class>()>,
		typename = void>
	void doForAllMembers(F&& /*f*/) noexcept
	{
		//static_assert(false, "nu i type-ul inregistrat!");
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

	// Check if class T has member named 'name'
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

		const T& get(const Class& obj) const noexcept
		{
			if (refGetterPtr) {
				return (obj.*refGetterPtr)();
			} else  {
				return obj.*ptr;
			}
		}

		T getCopy(const Class& obj) const noexcept
		{
			if (refGetterPtr) {
				return (obj.*refGetterPtr)();
			} else if (valGetterPtr) {
				return (obj.*valGetterPtr)();
			} else {
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

		//union {
		//	member_ptr_t<Class, T> ptr;
		//	struct {
		//		ref_getter_func_ptr_t<Class, T> refGetterPtr;
		//		ref_setter_func_ptr_t<Class, T> refSetterPtr;
		//	};
		//	struct {
		//		val_getter_func_ptr_t<Class, T> valGetterPtr;
		//		val_setter_func_ptr_t<Class, T> valSetterPtr;
		//	};
		//	nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr;
		//} members;
		//enum class Tag : std::uint8_t { ptr, ref, val, nonConstRef};
		//Tag tag = Tag::ptr;
		//std::uint8_t tag;


		// T& getRef(Class& obj) const;
		//Member& addNonConstGetter(nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr);
		//bool canGetRef() const { return ptr || nonConstRefGetterPtr; }
		//nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr = nullptr;
	};

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
