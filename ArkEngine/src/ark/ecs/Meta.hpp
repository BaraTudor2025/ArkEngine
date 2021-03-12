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


   // TODO (Meta): 4. pentru membrii privati

   Modulul de Meta are 4 parti
   1. Groups: un vector de type-uri indexat dupa un string
	   -> grupuri de tipuri
   2. Services: un table de functii indexat printr-un pair de (std::type_index, string)
       -> servesc ca callback-uri sofisticate
       -> observatorul/cel care apeleaza seviciile isi defineste string-ul (acesta fiind un grup de service-uri)
       -> observatorul poate da un template care poate fi folosit de orice type la inregistrare
   3. Metadata: numele, size, alignment, referinte catre constructori
       -> ctorii sunt type-erased pentru a putea fi apelati dinamic
   4. Members: membrii si functiile unui struct/class si-i poti itera
       -> iteratia are loc in compile-time: un loop-rool pe un std::tuple
	   -> membrii au proprietate de nume, get-citire, set-atribuire (operator=), 
	   -> deoarece biblioteca face abstractie prin get/set, un membru poate sa fie si o pereche de get/set al clasei, nu doar o variabila

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
#include <any>
#include <functional>
#include <string_view>
#include <optional>
#include <sstream>
#include <span>

/* for example usage see the registration of ark::Transform for members and for enum see RandomNumbers.hpp */

#define RUN_CODE_NAMESPACE(TYPE, GUARD, ...) \
	namespace ark::meta { \
		template <> inline auto dummy<TYPE, GUARD> = []() {__VA_ARGS__ return 0;} (); \
	} \

#define RUN_CODE_QUALIFIED(TYPE, GUARD, ...) \
	template <> inline auto ::ark::meta::dummy<TYPE, GUARD> = []() {__VA_ARGS__ return 0;} ();

#define RUN_CODE_QSTATIC(TYPE, GUARD, ...) \
	template <> static inline auto ::ark::meta::dummy<TYPE, GUARD> = []() {__VA_ARGS__ return 0;} ();

// de pus intr o clasa, si dat un type unic ca 'GUARD'
#define ARK_REGISTER_SERVICES_IN_CLASS(TYPE, GUARD, ...) \
	RUN_CODE_QSTATIC(TYPE, GUARD, (__VA_ARGS__); )

#define ARK_REGISTER_MEMBERS(TYPE) \
	template <> inline auto ::ark::meta::registerMembers<TYPE>() noexcept

#define ARK_REGISTER_GROUP_WITH_TAG(TYPE, TAG, GROUP)\
	inline auto _ark_reg_group##TAG = [](){ ark::meta::addTypeToGroup(GROUP, typeid(TYPE)); return 0;}();
#define ARK_REGISTER_GROUP(TYPE, GROUP) ARK_REGISTER_GROUP(TYPE, TYPE, GROUP)

#define ARK_REGISTER_SERVICES(TAG, ...)\
	inline auto _ark_reg_serv##TAG = [](){ (__VA_ARGS__); return 0;}();

#define ARK_REGISTER_METADATA_WITH_NAME_TAG(TYPE, NAME, TAG)\
	inline auto _ark_reg_meta##TAG = [](){ ark::meta::type<TYPE>(NAME); return 0;}();
#define ARK_REGISTER_METADATA(TYPE) ARK_REGISTER_METADATA_WITH_TAG_NAME(TYPE, #TYPE, TYPE)
#define ARK_REGISTER_METADATA_WITH_TAG(TYPE, TAG) ARK_REGISTER_METADATA_WITH_TAG_NAME(TYPE, #TYPE, TAG)
#define ARK_REGISTER_METADATA_WITH_NAME(TYPE, NAME) ARK_REGISTER_METADATA_WITH_TAG_NAME(TYPE, NAME, TAG)

#define ARK_REGISTER_TYPE_WITH_NAME_TAG(GROUP, TYPE, NAME, TAG, /*services*/...) \
	ARK_REGISTER_GROUP_WITH_TAG(TYPE, TAG, GROUP) \
	ARK_REGISTER_METADATA_WITH_NAME_TAG(TYPE, NAME, TAG) \
	ARK_REGISTER_SERVICES(TAG, __VA_ARGS__) \
	ARK_REGISTER_MEMBERS(TYPE)

#define ARK_REGISTER_TYPE(GROUP, TYPE, /*services*/...) ARK_REGISTER_TYPE_WITH_NAME_TAG(GROUP, TYPE, #TYPE, TYPE, __VA_ARGS__)
#define ARK_REGISTER_TYPE_WITH_NAME(GROUP, TYPE, NAME, /*services*/...) ARK_REGISTER_TYPE_WITH_NAME_TAG(GROUP, TYPE, NAME, TYPE, __VA_ARGS__)
#define ARK_REGISTER_TYPE_WITH_TAG(GROUP, TYPE, TAG, /*services*/...) ARK_REGISTER_TYPE_WITH_NAME_TAG(GROUP, TYPE, #TYPE, TAG, __VA_ARGS__)

#define ARK_REGISTER_COMPONENT_WITH_NAME_TAG(TYPE, NAME, TAG, /*services*/...) \
	ARK_REGISTER_TYPE_WITH_NAME_TAG(ARK_META_COMPONENT_GROUP, TYPE, NAME, TAG, __VA_ARGS__)

#define ARK_REGISTER_COMPONENT(TYPE, /*services*/...) ARK_REGISTER_COMPONENT_WITH_NAME_TAG(TYPE, #TYPE, TYPE, __VA_ARGS__)
#define ARK_REGISTER_COMPONENT_WITH_NAME(TYPE, NAME, /*services*/...) ARK_REGISTER_COMPONENT_WITH_NAME_TAG(TYPE, NAME, TYPE, __VA_ARGS__)
#define ARK_REGISTER_COMPONENT_WITH_TAG(TYPE, TAG, /*services*/...) ARK_REGISTER_COMPONENT_WITH_NAME_TAG(TYPE, #TYPE, TAG, __VA_ARGS__)

#define ARK_REGISTER_ENUM(TYPE) \
	template <> inline auto ::ark::meta::registerEnum<TYPE>()

// static and runtime reflection
//#define ARK_MEMBERS(TYPE, ...) \
//	ARK_REGISTER_MEMBERS(TYPE) { return ark::meta::members<TYPE>(); }

//#define ARK_MEMBERS(TYPE, ...) \
//	RUN_CODE_NAMESPACE(TYPE, TYPE, ark::meta::type<TYPE>(); ark::meta::addTypeToGroup(ARK_META_COMPONENT_GROUP, typeid(TYPE))) \ 
//	template <> const auto ark::meta::detail::sMembers<TYPE> = ark::meta::members<TYPE>(__VA_ARGS__);

namespace ark::meta
{
	template <typename Class>
	inline auto registerMembers() noexcept;

	inline int getUniqueId()
	{
		static int cID = 0;
		return cID++;
	}

	struct EnumValue {
		using value_type = std::int64_t;

		const std::type_index type;
		const std::string_view name;
		const value_type value;

		template<typename EnumT>
		EnumValue(std::string_view name, EnumT value) : type(typeid(EnumT)), name(name), value(static_cast<EnumValue::value_type>(value)) {}

		template <typename EnumT>
		requires std::is_enum_v<EnumT>
		operator EnumT() const { assert(type == typeid(EnumT)); return static_cast<EnumT>(value); }

		// TODO: operator<=>

		template <typename EnumT>
		friend bool operator==(EnumT val, const EnumValue& This) {
			assert(This.type == typeid(EnumT));
			return val == static_cast<EnumT>(This.value);
		}
		template <typename EnumT>
		friend bool operator==(const EnumValue& This, EnumT val) {
			assert(This.type == typeid(EnumT));
			return val == static_cast<EnumT>(This.value);
		}
		template <typename EnumT>
		friend bool operator!=(const EnumValue& This, EnumT val) {
			return !(val == This);
		}
		template <typename EnumT>
		friend bool operator!=(EnumT val, const EnumValue& This) {
			return !(val == This);
		}
	};

	namespace detail
	{
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
	using nonconst_ref_getter_func_ptr_t = T & (Class::*)();


	template <typename Class, typename Ret, typename... Args>
	using function_ptr_t = Ret(Class::*)(Args...);

	template <typename Class, typename Ret, typename... Args>
	using const_function_ptr_t = Ret(Class::*)(Args...) const;

	template <typename T>
	concept printable = requires(std::stringstream ss, T value) {
		ss << value;
	};

	template <typename, typename = void>
	inline auto dummy = 0;

	class RuntimeProperty {

	public:
		const std::type_index type;
		const std::string_view name;
		const bool isEnum;

		// works on class instances
		void get(const void* instance, void* out_value) const {
			m_getter(instance, out_value);
		}
		void get(const void* instance, std::any& out_value) const {
			m_getter_any(instance, out_value);
		}
		std::any get(const void* instance) const {
			auto value = std::any{};
			get(instance, value);
			return value;
		}

		// works on class instances
		void set(void* instance, void* in_value) const {
			m_setter(instance, in_value);
		}
		void set(void* instance, std::any in_value) const {
			if (isEnum && in_value.type() == typeid(EnumValue::value_type))
				this->toEnumFromInt(in_value);
			m_setter_any(instance, std::move(in_value));
		}
		void set(void* instance, std::any&& in_value) const {
			if (isEnum && in_value.type() == typeid(EnumValue::value_type))
				this->toEnumFromInt(in_value);
			m_setter_any(instance, std::move(in_value));
		}

		std::string toString(const void* instance) const {
			return m_toString(instance);
		}

		/* helpers for the property itself*/

		// get underlying pointer from any
		void* fromAny(std::any& propertyValue) const {
			return m_fromAny(propertyValue);
		}

		/* conversions */
		void toIntFromEnum(std::any& propertyValue) const {
			m_toIntFromEnum(propertyValue);
		}
		void toEnumFromInt(std::any& propertyValue) const {
			m_toEnumFromInt(propertyValue);
		}

		bool isEqual(const std::any& any, const void* data) const {
			return m_isEq(any, data);
		}

		template <typename Class, typename Property>
		RuntimeProperty(std::string_view name, ark::meta::member_ptr_t<Class, Property> ptr)
			: RuntimeProperty(name, std::type_identity<Class>{}, std::type_identity<Property>{}, ptr, ptr)
		{ }

		template <typename Class, typename Property>
		RuntimeProperty(
			std::string_view name,
			ark::meta::ref_getter_func_ptr_t<Class, Property> ptrGet,
			ark::meta::ref_setter_func_ptr_t<Class, Property> ptrSet)
			: RuntimeProperty(name, std::type_identity<Class>{}, std::type_identity<Property>{}, ptrGet, ptrSet)
		{ }

		template <typename Class, typename Property>
		RuntimeProperty(
			std::string_view name,
			ark::meta::val_getter_func_ptr_t<Class, Property> ptrGet,
			ark::meta::val_setter_func_ptr_t<Class, Property> ptrSet)
			: RuntimeProperty(name, std::type_identity<Class>{}, std::type_identity<Property>{}, ptrGet, ptrSet)
		{ }

	private:
		template <typename Class, typename Property, typename GetT, typename SetT>
		RuntimeProperty(std::string_view name, std::type_identity<Class>, std::type_identity<Property>, GetT ptrGet, SetT ptrSet) 
			: name(name), type(typeid(Property)), isEnum(std::is_enum_v<Property>),
			m_fromAny([](std::any& data) { return static_cast<void*>(&std::any_cast<Property&>(data)); }),
			m_toIntFromEnum([](std::any& any) {
			if constexpr (std::is_enum_v<Property>)
				any = static_cast<EnumValue::value_type>(std::any_cast<Property>(any));
		}),
			m_toEnumFromInt([](std::any& any) {
			if constexpr (std::is_enum_v<Property>)
				any = static_cast<Property>(std::any_cast<EnumValue::value_type>(any));
		}),
			m_isEq([](const std::any& any, const void* p) {
			if constexpr (std::equality_comparable<Property>)
				return *static_cast<const Property*>(p) == std::any_cast<const Property&>(any);
			else
				return false;
		}),
			m_getter_any([ptrGet](const void* instance, std::any& out_value) {
				out_value = std::invoke(ptrGet, static_cast<const Class*>(instance));
		}),
			m_getter([ptrGet](const void* instance, void* out_value) {
				*static_cast<Property*>(out_value) = std::invoke(ptrGet, static_cast<const Class*>(instance));
		}),
			m_setter([ptrSet](void* instance, void* in_value) {
				if constexpr (std::is_member_object_pointer_v<SetT>)
					std::invoke(ptrSet, static_cast<Class*>(instance)) = *static_cast<Property*>(in_value);
				else
					std::invoke(ptrSet, static_cast<Class*>(instance), *static_cast<Property*>(in_value));
		}),
			m_setter_any([ptrSet](void* instance, std::any&& in_value) {
				if constexpr (std::is_member_object_pointer_v<SetT>)
					std::invoke(ptrSet, static_cast<Class*>(instance)) = std::any_cast<Property>(in_value);
				else
					std::invoke(ptrSet, static_cast<Class*>(instance), std::any_cast<Property>(in_value));
		}),
			m_toString([this](const void* instance) {
				std::any any = this->get(instance);
				const Property& prop = std::any_cast<const Property&>(any);
				if constexpr (printable<Property>) {
					std::stringstream ss;
					ss << prop;
					return ss.str();
				}
				return std::string();
				//else if (ark::meta::hasProperties(typeid(Property))) {
				//	std::string output;
				//	auto* mdata = ark::meta::resolve(typeid(Property));
				//	for (const auto& subProp : mdata->prop()) {
				//		output += subProp.toString(&prop);
				//	}
				//}
		})
		{}

		std::function<std::string(const void*)> m_toString;
		void (* const m_toIntFromEnum)(std::any&);
		void (* const m_toEnumFromInt)(std::any&);
		void* (* const m_fromAny)(std::any&);
		bool (* const m_isEq)(const std::any&, const void*);
		std::function<void(const void*, void*)> m_getter;
		std::function<void(void*, void*)>  m_setter;
		std::function<void(const void*, std::any&)> m_getter_any;
		std::function<void(void*, std::any&&)> m_setter_any;
	};

	template <typename F>
	class meta_function;

	template <typename Ret, typename... Args>
	class meta_function<Ret(Args...)> {
		using FunctionType = std::function<Ret(Args...)>;
		const FunctionType* m_fun;
	public:

		meta_function(const FunctionType* fun) noexcept : m_fun(fun) {}
		meta_function(std::nullptr_t) noexcept : m_fun(nullptr) {}

		//template <typename... Args>
		//decltype(auto) operator()(Args&&... args) const noexcept(noexcept((*m_fun)(std::forward<Args>(args)...))) {
		//	return (*m_fun)(std::forward<Args>(args)...);
		//}

		Ret operator()(Args... args) const noexcept {
			return (*m_fun)(args...);
		}

		operator bool() const noexcept {
			return m_fun != nullptr;
		}

		auto get() const noexcept -> const FunctionType* {
			return m_fun;
		}

		auto operator*() const noexcept -> const FunctionType& {
			return *m_fun;
		}
	};

	static inline auto s_conversionTable 
		= std::unordered_map<std::type_index, std::unordered_map<std::type_index, void(*)(void* to, const void* from)>>{};

	static void convert(std::type_index to, std::type_index from) {
		if (s_conversionTable.contains(to)) {
			if (s_conversionTable.at(to).contains(from)) {
			}
			else { // find intermediary
				if (s_conversionTable.contains(from)) {
					for (auto [toType, toFun /*,prio*/] : s_conversionTable.at(to)) {
						for (auto [fromType, fromFun] : s_conversionTable.at(from)) {
							if (toType == fromType) {
								//std::any inter;// = make();
								//fromFun(inter, fromVal);
								//toFun(toVal, inter);
							}
						}
					}
				}
			}
		}
	}

	class Metadata {
		std::string m_name;
		void setName(const std::string& name) { m_name = name; }
	public:

		const std::type_index type;
		const std::size_t size;
		const std::size_t align;
		int flags;

		const std::string& getName() const { return m_name; }
		__declspec(property(get=getName, put=setName))
		std::string name;

		void(*default_constructor)(void*) = nullptr;
		void(*destructor)(void*) = nullptr;

		void(*copy_constructor)(void*, const void*) = nullptr;
		void(*copy_assing)(void*, const void*) = nullptr;

		void(*move_constructor)(void*, void*) = nullptr;
		void(*move_assign)(void*, void*) = nullptr;

		template <typename T>
		friend Metadata* type(std::string name) noexcept;

		void prop(RuntimeProperty&& runProp) {
			m_props.emplace_back(std::move(runProp));
		}

		// takes a member pointer or pair of getter and setter
		// see RuntimeProperty constructors
		template <typename... Args>
		requires std::constructible_from<RuntimeProperty, std::string_view, Args...>
		void prop(std::string_view name, Args... args) {
			m_props.emplace_back(RuntimeProperty(name, args...));
		}

		auto prop(std::string_view name) const -> const RuntimeProperty* {
			if (auto it = std::find_if(m_props.begin(), m_props.end(), [name](const auto& p) {return p.name == name; });
				it != m_props.end())
				return &*it;
			else
				return nullptr;
		}
		auto prop() const -> const std::vector<RuntimeProperty>& {
			return m_props;
		}

		// inserts only if not present
		template <typename T>
		requires std::is_object_v<T>
		T* data(std::string_view name, T&& value) {
			auto [it, success] = m_data.emplace(name, std::forward<T>(value));
			std::any& val = it->second;
			return &std::any_cast<T&>(val);
		}

		template <typename T>
		requires std::is_object_v<T>
		T* data(std::string_view name) const {
			if (auto it = m_data.find(name); it != m_data.end())
				return &std::any_cast<T&>(const_cast<std::any&>(it->second));
			else
				return nullptr;
		}

		void func(std::string_view name, auto fun) {
			m_funcs.emplace(name, std::any{ std::function<std::remove_pointer_t<detail::make_func_ptr_t<decltype(fun)>>>{ fun } });
		}

		template <typename F>
		requires std::is_function_v<F>
		auto func(std::string_view name) const -> meta_function<F>
		{
			if (auto it = m_funcs.find(name); it != m_funcs.end())
				return &std::any_cast<const std::function<F>&>(it->second);
			else
				return nullptr;
		}

	private:
		std::vector<RuntimeProperty> m_props;
		std::unordered_map<std::string_view, std::any> m_data;
		std::unordered_map<std::string_view, std::any> m_funcs;

		template <typename T>
		Metadata(std::type_identity<T>, std::string name)
			: type(typeid(T)), size(sizeof(T)), align(alignof(T)), m_name(name) 
		{}
	};

	namespace detail
	{
		template <typename... Args>
		struct type_list
		{
			template <std::size_t N>
			using arg = std::tuple_element_t<N, std::tuple<Args...>>;

			using type = type_list;

			using indices = std::index_sequence_for<Args...>;

			static constexpr auto size = sizeof...(Args);
		};

		inline std::string prettifyTypeName(std::string_view name) noexcept
		{
			// remove 'class' or 'struct'
			if(auto pos = name.find_first_of(' '); pos != name.npos)
				name.remove_prefix(pos + 1);

			auto templateArgPos = name.find_first_of('<');
			// remove namespace
			if (templateArgPos == name.npos) {
				auto pos = name.find_first_of("::");
				while (pos != name.npos) {
					name.remove_prefix(pos + 2);
					pos = name.find_first_of("::");
				}
			}
			else { // remove namespace until template argument
				auto pos = name.find_first_of("::");
				while (pos != name.npos && pos < templateArgPos) {
					name.remove_prefix(pos + 2);
					pos = name.find_first_of("::");
					templateArgPos = name.find_first_of('<');
				}
			}
			// make template argument pretty (if any)
			if (templateArgPos != name.npos) {
				auto begPos = name.find_first_of('<') + 1;
				auto templateArg = prettifyTypeName(name.substr(begPos, name.size() - begPos - 1));
				// remove template arg so the prettier version can be added;
				name.remove_suffix(name.size() - name.find_first_of('<'));
				return std::string(name) + '<' + templateArg + '>';
			}
			else
				return std::string(name);
		}

		/* sMembers holds all Member objects constructed via meta::registerMembers<T> call.
		 * If the class is not registered, sMembers is std::tuple<> */
		template <typename T>
		inline const auto sMembers = registerMembers<T>();

		// bagamiaspulanmicrosoft ca aparent initializeaza mai intai variabilele statice inline din clasa si dupa pe cele globale statice
		// asa ca tre sa pun sTypeTable intr un struct ca sa ma asigur ca e construit inainte de varialbilele anonime din macroul REGISTER_SERVICES
		//struct/namespace bagamiaspula {

		inline auto fsTypeTable() -> std::unordered_map<std::type_index, Metadata>& {
			static std::unordered_map<std::type_index, Metadata> sTypeTable;
			return sTypeTable;
		}
		struct guard {
			static inline std::unordered_map<std::string_view, std::vector<std::type_index>> sTypeGroups;
		};

		/* helper functions */

		// c++20 ?
		//template <std::size_t Idx = 0, const auto& tupleT, typename F>
		//constexpr int template_for_tuple(F f)
		//{
		//	constexpr auto tupMem = std::get<Idx>(tupleT);
		//	constexpr decltype(auto) _ = f(tupMem);
		//	if constexpr (Idx + 1 != std::tuple_size_v<std::decay_t<decltype(tupleT)>>)
		//		template_for_tuple<Idx + 1, tupleT>(f);
		//	return 0;
		//}

		template <typename F, typename... Args, std::size_t... Idx>
		constexpr void for_tuple_impl(F f, std::tuple<Args...> tuple, std::index_sequence<Idx...>)
		{
			(f(std::get<Idx>(tuple)), ...);
		}

		template <typename F, typename... Args>
		constexpr void for_tuple(F f, std::tuple<Args...> tuple)
		{
			for_tuple_impl(f, tuple, std::index_sequence_for<Args...>{});
		}

		template <typename F>
		constexpr void for_tuple(F&& /* f */, const std::tuple<>& /* tuple */)
		{ /* do nothing */
		}

		template <class F, class... Ts>
		constexpr void for_each_argument(F&& f, Ts&&... a)
		{
			(f(std::forward<Ts>(a)), ...);
		}

		template <typename F, typename... Args>
		constexpr void property_for_tuple(F&& f, const std::tuple<Args...>& tuple)
		{
			std::apply([&](const Args&... members) {
				for_each_argument([&](const auto& member) {
					if constexpr (member.isProperty)
						f(member);
				}, members...);
			}, tuple);
		}

		template <typename F, typename... Args>
		constexpr void function_for_tuple(F&& f, const std::tuple<Args...>& tuple)
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

		template <typename L>
		constexpr void* lambdaToPtr(L&& lambda)
		{
			return static_cast<make_func_ptr_t<L>>(lambda);
		}


		template <typename T, typename...>
		using getFirstArg = T;

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

			using arg_list = type_list<Arguments...>;

			template <std::size_t N>
			using arg = typename arg_list::template arg<N>;

			static constexpr std::size_t arity = sizeof...(Arguments);
		};

	} // detail

	template <std::invocable<ark::meta::Metadata&> F>
	void forTypes(F&& f) {
		for (auto& type : detail::fsTypeTable()) {
			f(type.second);
		}
	}

	// register type T if not registered and return its Metadata
	// if no name is passed then prettifyTypeName will generate a name from typeid(T).name()
	// a name can be provided and overriten at any time when calling this function, even if the type is registered
	template <typename T>
	Metadata* type(std::string name = "") noexcept
	{
		if (auto it = detail::fsTypeTable().find(typeid(T)); it != detail::fsTypeTable().end()) {
			if (!name.empty())
				it->second.name = name;
			return &it->second;
		}
		if (name.empty())
			name = detail::prettifyTypeName(typeid(T).name());
		auto metadata = Metadata{ std::type_identity<T>{}, std::move(name) };

		if constexpr (std::is_default_constructible_v<T>)
			metadata.default_constructor = [](void* This) { new(This)T{}; };

		if constexpr (std::is_copy_constructible_v<T>)
			metadata.copy_constructor = [](void* This, const void* That) { new (This)T(*static_cast<const T*>(That)); };

		if constexpr (std::is_copy_assignable_v<T>)
			metadata.copy_assing = [](void* This, const void* That) { *static_cast<T*>(This) = *static_cast<const T*>(That);};

		if constexpr (std::is_move_constructible_v<T>)
			metadata.move_constructor = [](void* This, void* That) { new(This)T(std::move(*static_cast<T*>(That))); };

		if constexpr (std::is_move_assignable_v<T>)
			metadata.move_assign = [](void* This, void* That) { *static_cast<T*>(This) = std::move(*static_cast<T*>(That)); };

		if constexpr (std::is_destructible_v<T>)
			metadata.destructor = [](void* This) { static_cast<T*>(This)->~T(); };

		return &(detail::fsTypeTable().emplace(typeid(T), std::move(metadata)).first->second);
	}

	inline auto resolve(std::type_index type) noexcept -> Metadata*
	{
		if (detail::fsTypeTable().count(type))
			return &detail::fsTypeTable().at(type);
		else
			return nullptr;
	}

	template <typename T>
	inline auto resolve() noexcept -> Metadata* {
		return resolve(typeid(T));
	}

	inline auto resolve(std::string_view name) noexcept -> Metadata*
	{
		for (auto& [type, mdata] : detail::fsTypeTable()) {
			if (mdata.name == name)
				return &mdata;
		}
		return nullptr;
	}

	inline bool hasProperties(std::type_index type) {
		auto mdata = resolve(type); 
		return mdata && !mdata->prop().empty();
	}

	inline void addTypeToGroup(std::string_view groupName, std::type_index type) noexcept
	{
		auto& vec = detail::guard::sTypeGroups[groupName];
		if (auto it = std::find(vec.begin(), vec.end(), type); it == vec.end())
			vec.push_back(type);
	}

	inline auto getTypeGroup(std::string_view groupName) noexcept -> std::span<std::type_index>
	{
		auto it = detail::guard::sTypeGroups.find(groupName);
		if (it != detail::guard::sTypeGroups.end())
			return it->second;
		else
			return {};
	}

	/* Enum stuff */

	struct detail_enum_s {
		static inline auto s_EnumValuesTable = std::unordered_map<std::type_index, std::vector<EnumValue>>();
	};

	inline void registerEnum(std::type_index type, std::vector<EnumValue> enumValues) {
		detail_enum_s::s_EnumValuesTable[type] = std::move(enumValues);
	}
	template <typename T>
	inline void registerEnum(std::vector<EnumValue> enumValues) {
		registerEnum(typeid(T), std::move(enumValues));
	}
	template <typename... Args>
	inline void registerEnum(std::type_index type, Args&&... enumValues) {
		registerEnum(type, std::vector<EnumValue>{std::forward<Args>(enumValues)...});
	}
	template <typename T, typename... Args>
	inline void registerEnum(Args&&... enumValues) {
		registerEnum(typeid(T), std::forward<Args>(enumValues)...);
	}

	inline auto getEnumValues(std::type_index type) -> const std::vector<EnumValue>* {
		auto it = detail_enum_s::s_EnumValuesTable.find(type);
		if (it != detail_enum_s::s_EnumValuesTable.end())
			return &it->second;
		else
			return nullptr;
	}
	template <typename T>
	inline auto getEnumValues() -> const std::vector<EnumValue>* {
		return getEnumValues(typeid(T));
	}

	inline auto getNameOfEnumValue(std::type_index type, EnumValue::value_type value) -> std::string_view
	{
		const auto& fields = getEnumValues(type);
		auto fieldName = std::string_view{};
		for (const auto& field : *fields)
			if (field.value == value)
				fieldName = field.name;
		return fieldName;
	}

	template <typename EnumT>
	inline auto getNameOfEnumValue(EnumT value) -> std::string_view {
		return getNameOfEnumValue(typeid(EnumT), static_cast<EnumValue::value_type>(value));
	}

	inline auto getValueOfEnumName(std::type_index type, std::string_view name) -> EnumValue::value_type
	{
		const auto& fields = getEnumValues(type);
		for (const auto& field : *fields)
			if (field.name == name)
				return field.value;
		assert(false);
		return {};
	}

	template <typename EnumT>
	inline auto getValueOfEnumName(std::string_view name) -> EnumT
	{
		return static_cast<EnumT>(getValueOfEnumName(typeid(EnumT), name));
	}


	/* class members stuff */

	template <typename T, typename... Args>
	constexpr auto members(Args&&... args) noexcept
	{
		auto tup = std::make_tuple(std::forward<Args>(args)...);
		auto* mdata = type<T>();
		detail::property_for_tuple([&mdata](auto& member) {
			if (member.hasPtr())
				mdata->prop(RuntimeProperty(member.getName(), member.getPtr()));
			else if (member.hasRefFuncPtrs()) {
				auto [get, set] = member.getRefFuncPtrs();
				mdata->prop(RuntimeProperty(member.getName(), get, set));
			}
			else if (member.hasValFuncPtrs()) {
				auto [get, set] = member.getValFuncPtrs();
				mdata->prop(RuntimeProperty(member.getName(), get, set));
			}
		}, tup);
		return tup;
	}

	// function used for registration of classes by user
	template <typename Class>
	inline auto registerMembers() noexcept
	{
		return std::make_tuple();
	}

	// returns the number of registered members of the class
	template <typename Class>
	constexpr std::size_t getMemberCount() noexcept
	{
		return std::tuple_size_v<decltype(registerMembers<Class>())>;
	}

	//template <typename T>
	//concept _isRegisterd = requires {
	//	detail::sMembers<T>;
	//};

	// Check if class has registerMembers<T> specialization (has been registered)
	template <typename Class>
	constexpr bool isRegistered() noexcept
	{
		return !std::is_same_v<std::tuple<>, decltype(registerMembers<Class>())>;
		//return _isRegisterd<Class>;
	}

	template <typename Class, typename F, typename = std::enable_if_t<isRegistered<Class>()>>
	constexpr void doForAllMembers(F f) noexcept
	{
		// c++20 ?
		//static constexpr auto mems = registerMembers<Class>();
		//detail::template_for_tuple<0, mems>(f);
		detail::for_tuple(f, detail::sMembers<Class>);
	}

	template <typename Class, typename F, typename = std::enable_if_t<!isRegistered<Class>()>, typename=void>
	constexpr void doForAllMembers(F&& f) noexcept { }

	template <typename Class, typename F, typename = std::enable_if_t<isRegistered<Class>()>>
	constexpr void doForAllFunctions(F&& f) noexcept
	{
		detail::function_for_tuple(std::forward<F>(f), detail::sMembers<Class>);
	}

	template <typename Class, typename F,
		typename = std::enable_if_t<!isRegistered<Class>()>,
		typename = void>
	constexpr void doForAllFunctions(F&& /*f*/) noexcept
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

	template <typename MemberType>
	using get_member_type = typename std::decay_t<MemberType>::member_type;

	template <typename MemberType>
	using get_class_type = typename std::decay_t<MemberType>::class_type;

	// Do F for member named 'name' with type T. It's important to pass correct type of the member
	template <typename Class, typename T, typename F>
	void doForProperty(const char* name, F&& f)
	{
		doForAllProperties<Class>(
			[&](const auto& member)
		{
			if (!strcmp(name, member.getName())) {
				using MemberT = meta::get_member_type<decltype(member)>;
				static_assert((std::is_same_v<MemberT, T>) && "Property doesn't have type T");
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

	template <typename Class, typename Ret, typename... Args>
	class MemberFunction {
	public:
		using class_type = Class;
		using member_type = Ret(Args...);
		using member_type_ptr = Ret(*)(Args...);

		constexpr MemberFunction(const char* name, function_ptr_t<Class, Ret, Args...> fun) noexcept
			: isConst(false), m_name(name), m_functionPtr(fun)
		{
		}

		constexpr MemberFunction(const char* name, const_function_ptr_t<Class, Ret, Args...> fun) noexcept
			: isConst(true), m_name(name), m_constFunctionPtr(fun)
		{
		}

		static constexpr bool isProperty = false;
		static constexpr bool isFunction = true;

		const bool isConst;
		constexpr const char* getName() const noexcept { return this->m_name; }
		constexpr auto getConstFunPtr() const noexcept { return m_constFunctionPtr; }
		constexpr auto getFunPtr() const noexcept { return m_functionPtr; }

		Ret call(Class& obj, Args&&... args) const noexcept
		{
			if (!isConst) {
				return (obj.*m_functionPtr)(std::forward<Args>(args));
			}
			else {
				return (obj.*m_constFunctionPtr)(std::forward<Args>(args));
			}
		}

	private:
		const char* const m_name;

		union {
			function_ptr_t<Class, Ret, Args...> m_functionPtr;
			const_function_ptr_t<Class, Ret, Args...> m_constFunctionPtr;
		};

	};


	template <typename Class, typename T>
	class Property {
	public:
		using class_type = Class;
		using member_type = T;

		constexpr Property(const char* name, member_ptr_t<Class, T> ptr) noexcept
			: m_tag(Tag::ptr), m_name(name), m_ptr(ptr)
		{
		}

		constexpr Property(const char* name, ref_getter_func_ptr_t<Class, T> getterPtr, ref_setter_func_ptr_t<Class, T> setterPtr) noexcept
			: m_tag(Tag::ref), m_name(name), m_refGetter(getterPtr), m_refSetter(setterPtr)
		{
		}

		constexpr Property(const char* name, val_getter_func_ptr_t<Class, T> getterPtr, val_setter_func_ptr_t<Class, T> setterPtr) noexcept
			: m_tag(Tag::val), m_name(name), m_valGetter(getterPtr), m_valSetter(setterPtr)
		{
		}

		static constexpr bool isProperty = true;
		static constexpr bool isFunction = false;

		bool hasPtr() const noexcept { return Tag::ptr == m_tag; }
		bool hasRefFuncPtrs() const noexcept { return Tag::ref == m_tag; }
		bool hasValFuncPtrs() const noexcept { return Tag::val == m_tag; }
		bool canGetConstRef() const noexcept { return Tag::ptr == m_tag || Tag::ref == m_tag; }

		const char* getName() const noexcept { return m_name; }
		auto getPtr() const noexcept { return m_ptr; }
		auto getRefFuncPtrs() const noexcept { return std::pair{ m_refGetter, m_refSetter }; }
		auto getValFuncPtrs() const noexcept { return std::pair{ m_valGetter, m_valSetter }; }

		const T& get(const Class& obj) const noexcept
		{
			if (Tag::ref == m_tag) {
				return (obj.*m_refGetter)();
			}
			else {
				return obj.*m_ptr;
			}
		}

		T getCopy(const Class& obj) const noexcept
		{
			if (Tag::ref == m_tag) {
				return (obj.*m_refGetter)();
			}
			else if (Tag::val == m_tag) {
				return (obj.*m_valGetter)();
			}
			else {
				return obj.*m_ptr;
			}
		}

		template <typename U>
		void set(Class& obj, U&& value) const noexcept
		{
			static_assert(std::is_constructible_v<T, U>);
			if (Tag::ref == m_tag) {
				(obj.*m_refSetter)(value);
			}
			else if (Tag::val == m_tag) {
				(obj.*m_valSetter)(std::forward<U>(value));
			}
			else {
				obj.*m_ptr = std::forward<U>(value);
			}
		}

	private:
		const char* const m_name;

		enum class Tag : std::uint8_t { ptr, ref, val, nonConstRef };
		const Tag m_tag;

		union {
			member_ptr_t<Class, T> m_ptr;
			struct {

				ref_getter_func_ptr_t<Class, T> m_refGetter;
				ref_setter_func_ptr_t<Class, T> m_refSetter;
			};
			struct {
				val_getter_func_ptr_t<Class, T> m_valGetter;
				val_setter_func_ptr_t<Class, T> m_valSetter;
			};
			nonconst_ref_getter_func_ptr_t<Class, T> m_nonConstRefGetterPtr;
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
	constexpr Property<Class, T> member_property(const char* name, T Class::* ptr) noexcept
	{
		return { name, ptr };
	}

	template <typename Class, typename T>
	constexpr Property<Class, T> member_property(const char* name, ref_getter_func_ptr_t<Class, T> getterPtr, ref_setter_func_ptr_t<Class, T> setterPtr) noexcept
	{
		return Property<Class, T>(name, getterPtr, setterPtr);
	}

	template <typename Class, typename T>
	constexpr Property<Class, T> member_property(const char* name, val_getter_func_ptr_t<Class, T> getterPtr, val_setter_func_ptr_t<Class, T> setterPtr) noexcept
	{
		return Property<Class, T>(name, getterPtr, setterPtr);
	}

} // namespace meta
