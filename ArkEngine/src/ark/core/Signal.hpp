#pragma once

#include <functional>
#include <vector>
#include <typeindex>

namespace ark
{
	template <typename>
	class Signal;

	template <typename>
	class Sink;

	class Connection {
		template <typename>
		friend class Sink;

		std::function<void()> m_func;

		Connection(std::function<void()>&& func) : m_func(std::move(func)) {}
	public:

		Connection() = default;

		void release() {
			if (m_func) {
				m_func();
				m_func = nullptr;
			}
		}
	};

	class ScopedConnection {
		Connection m_con;

	public:
		ScopedConnection() = default;
		ScopedConnection(const Connection& con) : m_con(con) {}
		ScopedConnection(Connection&& con) : m_con(std::move(con)) {}

		ScopedConnection(ScopedConnection&& con) noexcept : m_con(std::move(con.m_con)) {}
		ScopedConnection& operator=(ScopedConnection&& con) noexcept { m_con = std::move(con.m_con); return *this; }

		ScopedConnection(const ScopedConnection&) = delete;
		ScopedConnection& operator=(const ScopedConnection&) = delete;

		ScopedConnection& operator=(Connection con) {
			m_con = std::move(con);
			return *this;
		}

		~ScopedConnection() { m_con.release(); }
		void release() {
			m_con.release();
		}
	};

	template <typename F, typename...Args>
	auto bind_front(F&& fun, Args&&... capt) {
		return[fun = std::forward<F>(fun), ...capt = std::forward<Args>(capt)](auto&&... args) mutable -> decltype(auto) {
			return std::invoke(fun, capt..., std::forward<decltype(args)>(args)...);
		};
	}

	template <typename Ret, typename ...Args>
	class Signal<Ret(Args...)> {
		friend class Sink<Ret(Args...)>;

		std::vector<std::function<Ret(Args...)>> m_funcs;

	public:
		using sink_type = Sink<Ret(Args...)>;

		int size() const { return m_funcs.size(); }

		void publish(Args... args) const {
			for (auto&& fun : std::as_const(m_funcs))
				fun(args...);
		}
	};

	template <typename Ret, typename ...Args>
	class Sink<Ret(Args...)> {
		Signal<Ret(Args...)>* m_signal;

	public:
		using signal_type = Signal<Ret(Args...)>;
	private:

		static void disconnect(signal_type* signal, std::type_index info) {
			std::erase_if(signal->m_funcs,
				[&](const auto& fun) { return fun.target_type() == info; });
		}

	public:

		Sink(signal_type& signal) noexcept : m_signal(&signal) {}

		template <auto Func>
		Connection connect() {
			return connect(Func);
		}

		template <typename F, typename... Payload>
		requires std::invocable<F, Payload..., Args...>
			Connection connect(F&& fn, Payload&&... pay) {
			std::type_index type = typeid(void);
			if constexpr (sizeof...(Payload) == 0) {
				type = m_signal->m_funcs.emplace_back(std::forward<F>(fn)).target_type();
			}
			else {
				auto fun = ark::bind_front(fn, std::forward<Payload>(pay)...);
				type = m_signal->m_funcs.emplace_back(std::move(fun)).target_type();
			}
			return Connection([type, sig = this->m_signal]{
				Sink::disconnect(sig, type);
				});
		}

		template <auto Func>
		void disconnect() {
			disconnect(m_signal, typeid(Func));
		}
	};

	template <typename Ret, typename...Args>
	Sink(Signal<Ret(Args...)>&)->Sink<Ret(Args...)>;

	class Dispatcher {

	public:

		void trigger() {}

		void push() {}

		template <typename Event>
		void update() {}

		void update() const {}
	};
}
