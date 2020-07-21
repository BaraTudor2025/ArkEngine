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
#pragma warning (disable : 26495) // silly VS warning about inline friend stuff...
#endif

#include <cstring>
#include <type_traits>
#include <tuple>
#include <utility>
#include <array>
#include <typeindex>
#include <unordered_map>
#include <map>


/* for example usage see the registration of ark::Transform for members and for enum see RandomNumbers.hpp */

#define RUN_CODE_NAMESPACE(TYPE, GUARD, ...) \
	namespace ark::meta { \
		template <> inline auto dummy<TYPE, GUARD> = []() {__VA_ARGS__ return 0;} (); \
	}

#define RUN_CODE_QUALIFIED(TYPE, GUARD, ...) \
	template <> inline auto ::ark::meta::dummy<TYPE, GUARD> = []() {__VA_ARGS__ return 0;} ();

#define RUN_CODE_QSTATIC(TYPE, GUARD, ...) \
	template <> static inline auto ::ark::meta::dummy<TYPE, GUARD> = []() {__VA_ARGS__ return 0;} ();

#define _ARK_META_COUT(TYPE) std::cout << "ctor:" << typeid(TYPE).name() << '\n'
//#define _ARK_META_COUT(TYPE)


// scope global, tre sa apara o singura data ca sa mearga
// ori dai lista de servicii la REGISTER_TYPE ori la REGISTER_SERVICES dupa ce type-ul a fost inregistrat
#define ARK_REGISTER_SERVICES(TYPE, GUARD, ...) \
	RUN_CODE_NAMESPACE(TYPE, GUARD, _ARK_META_COUT(TYPE); using Type = TYPE; services<TYPE>(__VA_ARGS__); )

// de pus intr o clasa, si dat un type unic ca 'GUARD'
#define ARK_REGISTER_SERVICES_IN_CLASS(TYPE, GUARD, ...) \
	RUN_CODE_QSTATIC(TYPE, GUARD, _ARK_META_COUT(TYPE); using Type = TYPE; services<TYPE>(__VA_ARGS__); )

#define ARK_REGISTER_MEMBERS(TYPE) \
	template <> constexpr inline auto ::ark::meta::registerMembers<TYPE>() noexcept

// standard
#define ARK_REGISTER_TYPE(TYPE, NAME, ...) \
	RUN_CODE_NAMESPACE(TYPE, void, constructMetadataFrom<TYPE>(NAME);) \
	ARK_REGISTER_SERVICES(TYPE, decltype(NAME), __VA_ARGS__) \
	ARK_REGISTER_MEMBERS(TYPE)

#define ARK_REGISTER_ENUM(TYPE) \
	template <> inline auto ::ark::meta::registerEnum<TYPE>()

namespace ark::meta
{
	template <typename Class>
	constexpr inline auto registerMembers() noexcept;

	inline int getUniqueId()
	{
		static int cID = 0;
		return cID++;
	}

	template <typename, typename=void>
	inline auto dummy = 0;

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
		inline const auto sMembers = registerMembers<T>();

		// bagamiaspulanmicrosoft ca aparent initializeaza mai intai variabilele statice inline din clasa si dupa pe cele globale statice
		// asa ca tre sa pun sTypeTable intr un struct ca sa ma asigur ca e construit inainte de varialbilele anonime din macroul REGISTER_SERVICES
		//struct/namespace bagamiaspula {
		struct guard {
			static inline std::unordered_map<std::type_index, Metadata> sTypeTable;
			//static inline std::unordered_map<std::type_index, std::unordered_map<std::string_view, void*>> sServiceTable;
			//static inline std::map<std::pair<std::type_index, std::string_view>, void*> sServiceTable;
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

		template <class F, class... Ts>
		void for_each_argument(F f, Ts&&... a)
		{
			(void)std::initializer_list<int>{(f(std::forward<Ts>(a)), 0)...};
		}

		template <typename F, typename... Args>
		void property_for_tuple(F&& f, const std::tuple<Args...>& tuple)
		{
			std::apply([&](const Args&... members) {
				for_each_argument([&](const auto& member) {
					if constexpr (member.isProperty)
						f(member);
				}, members...);
			}, tuple);
		}

		template <typename F, typename... Args>
		void function_for_tuple(F&& f, const std::tuple<Args...>& tuple)
		{
			std::apply([&](const Args&... members) {
				for_each_argument([&](const auto& member) {
					if constexpr (member.isFunction)
						f(member);
				}, members...);
			}, tuple);
		}

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

		template<typename> struct function_traits;

		template <typename Function>
		struct function_traits : function_traits<
			decltype(&Function::operator())
		> { };

		template <typename ClassType, typename ReturnType, typename... Arguments>
		struct function_traits<
			ReturnType(ClassType::*)(Arguments...) const
		> : function_traits<ReturnType(*)(Arguments...)> { };

		/* support the non-const operator ()
		 * this will work with user defined functors */
		template <typename ClassType, typename ReturnType, typename... Arguments>
		struct function_traits<
			ReturnType(ClassType::*)(Arguments...)
		> : function_traits<ReturnType(*)(Arguments...)> { };

		template <typename ReturnType, typename... Arguments>
		struct function_traits<ReturnType(*)(Arguments...)>
			: function_traits<ReturnType(Arguments...)> { };

		template <typename ReturnType, typename... Arguments>
		struct function_traits<ReturnType(Arguments...)> 
		{
			using return_type = ReturnType;

			template <std::size_t Index>
			using arg = typename std::tuple_element<
				Index,
				std::tuple<Arguments...>
			>::type;

			static constexpr std::size_t arity = sizeof...(Arguments);
		};

	} // end of namespace detail


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

		//if constexpr (not std::is_trivially_destructible_v<T> && std::is_destructible_v<T>)
		if constexpr (std::is_destructible_v<T>)
			metadata.destructor = [](void* This) { static_cast<T*>(This)->~T(); };
		
		if(not detail::guard::sTypeTable.count(typeid(T)))
			detail::guard::sTypeTable.emplace(typeid(T), std::move(metadata));
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
	constexpr auto members(Args&&... args) noexcept
	{
		return std::make_tuple(std::forward<Args>(args)...);
	}

	// function used for registration of classes by user
	template <typename Class>
	constexpr inline auto registerMembers() noexcept
	{
		return std::make_tuple();
	}

	// returns the number of registered members of the class
	template <typename Class>
	constexpr std::size_t getMemberCount() noexcept
	{
		return std::tuple_size<decltype(registerMembers<Class>())>::value;
	}


	// Check if class has registerMembers<T> specialization (has been registered)
	template <typename Class>
	constexpr bool isRegistered() noexcept
	{
		return !std::is_same<std::tuple<>, decltype(registerMembers<Class>())>::value;
	}

	template <typename Class, typename F, typename = std::enable_if_t<isRegistered<Class>()>>
	void doForAllFunctions(F&& f) noexcept
	{
		detail::function_for_tuple(std::forward<F>(f), detail::sMembers<Class>);
	}

	template <typename Class, typename F,
		typename = std::enable_if_t<!isRegistered<Class>()>,
		typename = void>
	void doForAllFunctions(F&& /*f*/) noexcept
	{
		//static_assert(false, "nu i type-ul inregistrat!");
		// do nothing! Nothing gets generated
	}

	template <typename Class, typename F, typename = std::enable_if_t<isRegistered<Class>()>>
	void doForAllProperties(F&& f) noexcept
	{
		//detail::for_tuple(std::forward<F>(f), detail::sMembers<Class>);
		detail::property_for_tuple(std::forward<F>(f), detail::sMembers<Class>);
	}

	// version for non-registered classes (to generate less template stuff)
	template <typename Class, typename F,
		typename = std::enable_if_t<!isRegistered<Class>()>,
		typename = void>
	void doForAllProperties(F&& /*f*/) noexcept
	{
		//static_assert(false, "nu i type-ul inregistrat!");
		// do nothing! Nothing gets generated
	}

	// MemberType is Property<T, Class>
	template <typename MemberType>
	using get_member_type = typename std::decay_t<MemberType>::member_type;

	// Do F for member named 'name' with type T. It's important to pass correct type of the member
	template <typename Class, typename T, typename F>
	void doForProperty(const char* name, F&& f)
	{
		doForAllProperties<Class>(
			[&](const auto& member)
			{
				if (!strcmp(name, member.getName())) {
					using MemberT = meta::get_member_type<decltype(member)>;
					static_assert((std::is_same<MemberT, T>::value) && "Property doesn't have type T");
					if constexpr (std::is_same_v<MemberT, T>)
						f(member);
				}
			}
		);
	}

	// Check if class T has member named 'name'
	template <typename Class>
	bool hasProperty(const char* name)
	{
		bool found = false;
		doForAllProperties<Class>(
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
	T getPropertyValue(Class& obj, const char* name)
	{
		T value;
		doForProperty<Class, T>(name,
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
	void setPropertyValue(Class& obj, const char* name, V&& value)
	{
		doForProperty<Class, T>(name,
			[&obj, value = std::forward<V>(value)](const auto& member)
			{
				member.set(obj, value);
			}
		);
	}


	template <typename Class, typename T>
	using member_ptr_t = T Class::*;

	template <typename Class, typename T>
	using ref_getter_func_ptr_t = const T& (Class::*)() const;

	template <typename Class, typename T>
	using ref_setter_func_ptr_t = void (Class::*)(const T&);

	template <typename Class, typename T>
	using val_getter_func_ptr_t = T(Class::*)() const;

	template <typename Class, typename T>
	using val_setter_func_ptr_t = void (Class::*)(T);

	template <typename Class, typename T>
	using nonconst_ref_getter_func_ptr_t = T& (Class::*)();


	template <typename Class, typename Ret, typename... Args>
	using function_ptr_t = Ret(Class::*)(Args...);

	template <typename Class, typename Ret, typename... Args>
	using const_function_ptr_t = Ret(Class::*)(Args...) const;



	template <typename Class, typename Ret, typename... Args>
	class MemberFunction {
	public:
		using class_type = Class;
		using member_type = Ret(Args...);
		using member_type_ptr = Ret(*)(Args...);

		constexpr MemberFunction(const char* name, function_ptr_t<Class, Ret, Args...> fun) noexcept
			: mTag(Tag::fun), mName(name), mFunctionPtr(fun) { }

		constexpr MemberFunction(const char* name, const_function_ptr_t<Class, Ret, Args...> fun) noexcept
			: mTag(Tag::constFun), mName(name), mConstFunctionPtr(fun) { }

		static constexpr bool isProperty = false;
		static constexpr bool isFunction = true;

		const char* getName() const noexcept { return mName; }
		bool isConst() const noexcept { return Tag::constFun == mTag; }
		auto getConstFunPtr() const noexcept { return mConstFunctionPtr; }
		auto getFunPtr() const noexcept { return mFunctionPtr; }

		Ret call(Class& obj, Args&&... args) const noexcept { 
			if (Tag::fun == mTag) {
				return (obj.*mFunctionPtr)(std::forward<Args>(args));
			}
			else {
				return (obj.*mConstFunctionPtr)(std::forward<Args>(args));
			}
		}

	private:
		const char* const mName;

		union {
			function_ptr_t<Class, Ret, Args...> mFunctionPtr;
			const_function_ptr_t<Class, Ret, Args...> mConstFunctionPtr;
		};

		enum class Tag : std::uint8_t { fun, constFun};
		const Tag mTag;
	};


	template <typename Class, typename T>
	class Property {
	public:
		using class_type = Class;
		using member_type = T;

		constexpr Property(const char* name, member_ptr_t<Class, T> ptr) noexcept
			: mTag(Tag::ptr), mName(name), mPtr(ptr) {}

		constexpr Property(const char* name, ref_getter_func_ptr_t<Class, T> getterPtr, ref_setter_func_ptr_t<Class, T> setterPtr) noexcept
			: mTag(Tag::ref), mName(name), mRefGetter(getterPtr), mRefSetter(setterPtr) {}

		constexpr Property(const char* name, val_getter_func_ptr_t<Class, T> getterPtr, val_setter_func_ptr_t<Class, T> setterPtr) noexcept
			: mTag(Tag::val), mName(name), mValGetter(getterPtr), mValSetter(setterPtr) {}

		static constexpr bool isProperty = true;
		static constexpr bool isFunction = false;

		bool hasPtr() const noexcept { return Tag::ptr == mTag; }
		bool hasRefFuncPtrs() const noexcept { return Tag::ref == mTag; }
		bool hasValFuncPtrs() const noexcept { return Tag::val == mTag; }
		bool canGetConstRef() const noexcept { return Tag::ptr == mTag || Tag::ref == mTag; }

		const char* getName() const noexcept { return mName; }
		auto getPtr() const noexcept { return mPtr; }
		auto getRefFuncPtrs() const noexcept { return std::pair{mRefGetter, mRefSetter}; }
		auto getValFuncPtrs() const noexcept { return std::pair{mValGetter, mValSetter}; }

		const T& get(const Class& obj) const noexcept
		{
			if (Tag::ref == mTag) {
				return (obj.*mRefGetter)();
			} else {
				return obj.*mPtr;
			}
		}

		T getCopy(const Class& obj) const noexcept
		{
			if (Tag::ref == mTag) {
				return (obj.*mRefGetter)();
			} else if (Tag::val == mTag) {
				return (obj.*mValGetter)();
			} else {
				return obj.*mPtr;
			}
		}

		template <typename U>
		void set(Class& obj, U&& value) const noexcept
		{
			static_assert(std::is_constructible_v<T, U>);
			if (Tag::ref == mTag) {
				(obj.*mRefSetter)(value);
			} else if (Tag::val == mTag) {
				(obj.*mValSetter)(std::forward<U>(value));
			} else {
				obj.*mPtr = std::forward<U>(value);
			} 
		}

	private:
		const char* const mName;

		enum class Tag : std::uint8_t { ptr, ref, val, nonConstRef};
		const Tag mTag;

		union {
			member_ptr_t<Class, T> mPtr;
			struct {
				ref_getter_func_ptr_t<Class, T> mRefGetter;
				ref_setter_func_ptr_t<Class, T> mRefSetter;
			};
			struct {
				val_getter_func_ptr_t<Class, T> mValGetter;
				val_setter_func_ptr_t<Class, T> mValSetter;
			};
			nonconst_ref_getter_func_ptr_t<Class, T> mNonConstRefGetterPtr;
		};

		// T& getRef(Class& obj) const;
		//Property& addNonConstGetter(nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr);
		//bool canGetRef() const { return ptr || nonConstRefGetterPtr; }
		//nonconst_ref_getter_func_ptr_t<Class, T> nonConstRefGetterPtr = nullptr;
	};

	template <typename Class, typename Ret, typename... Args>
	constexpr MemberFunction<Class, Ret, Args...> member_function(const char* name, function_ptr_t<Class, Ret, Args...> fun) noexcept
	{
		return MemberFunction<Class, Ret, Args...>(name, fun);
	}

	template <typename Class, typename Ret, typename... Args>
	constexpr MemberFunction<Class, Ret, Args...> member_function(const char* name, const_function_ptr_t<Class, Ret, Args...> fun) noexcept
	{
		return MemberFunction<Class, Ret, Args...>(name, fun);
	}

	template <typename Class, typename T>
	constexpr Property<Class, T> member(const char* name, T Class::* ptr) noexcept
	{
		return { name, ptr };
	}

	template <typename Class, typename T>
	constexpr Property<Class, T> member(const char* name, ref_getter_func_ptr_t<Class, T> getterPtr, ref_setter_func_ptr_t<Class, T> setterPtr) noexcept
	{
		return Property<Class, T>(name, getterPtr, setterPtr);
	}

	template <typename Class, typename T>
	constexpr Property<Class, T> member(const char* name, val_getter_func_ptr_t<Class, T> getterPtr, val_setter_func_ptr_t<Class, T> setterPtr) noexcept
	{
		return Property<Class, T>(name, getterPtr, setterPtr);
	}

} // end of namespace meta
