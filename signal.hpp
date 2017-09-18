#pragma once

#include <functional>  
#include <map>  
#include <memory>
#include <vector>

namespace detail
{
	template <typename slot_type>
	class signal_t
	{
	public:
		signal_t() : current_id_(0) {}

		int connect(const slot_type& slot)  {
			slots_.insert(std::make_pair(++current_id_, slot));
			return current_id_;
		}

		void disconnect(int id) {
			slots_.erase(id);
		}

		void disconnect_all() {
			slots_.clear();
		}

		template<typename... Args>
		void emit(Args&&... args) const {
			for (auto it : slots_) {
				it.second(std::forward<Args>(args)...);
			}
		}

		template<typename... Args>
		void operator()(Args&&... args) const {
			return emit(std::forward<Args>(args)...);
		}

	private:
		mutable std::map< int, slot_type> slots_;
		mutable int current_id_;

	private:
		signal_t(const signal_t& other);
		void operator =(const signal_t & other);

	};
};


template <typename... Args>
class FuncSignal : public detail::signal_t < std::function<void(Args...)> >
{
};

template <typename R, typename... Args>
class FuncSignal<R(Args...)> : public FuncSignal < Args... >
{};


//////////////////////////////////////////////////////////////////////////
struct Trackable
{
	std::shared_ptr<void> m_trackable_ptr;

	Trackable() : m_trackable_ptr(new int)
	{}
};

namespace detail
{
	template <typename func_type>
	class slot_t
	{
	public:
		template<typename F>
		slot_t(F&& f) : m_slot_func(f)
		{}

		template<typename... Args>
		void operator()(Args&&... a)
		{
			if (!m_slot_func)
			{
				return;
			}
			for (auto& w : m_lookup)
			{
				if (w.expired())
				{
					m_slot_func.swap(std::function<func_type>());
					return;
				}
			}
			m_slot_func(a...);
		}


		template<typename C
			, class = typename std::enable_if<std::is_convertible<C*, Trackable*>::value, void>::type>
			void add_track(C* obj)
		{
			m_lookup.push_back(obj->m_trackable_ptr);
		}
	private:
		std::function<func_type> m_slot_func;
		std::vector< std::weak_ptr<void> > m_lookup;
	};

	///
	template<int>
	struct _slot_autgen_placeholder
	{
	};

};

namespace std
{
	template<int N>
	struct is_placeholder<detail::_slot_autgen_placeholder<N>>
		: integral_constant < int, N + 1 >
	{
	};
};

template <typename... Args>
class ClassSignal : public detail::signal_t < detail::slot_t<void(Args...)> >
{
public:

	template<typename C>
	int connect_member(void(C::*f)(Args...), C* obj)
	{
		static_assert(std::is_convertible<C*, Trackable*>::value, "Not convert to type Trackable");
		auto fn = construct_mem_fn(f, obj, make_int_sequence < sizeof...(Args) > {});
		auto slot = detail::slot_t<void(Args...)>(std::move(fn));
		slot.add_track(obj);
		return connect(std::move(slot));
	}
private:
	template<int... Ns>
	struct int_sequence
	{};

	template<int N, int... Ns>
	struct make_int_sequence
		: make_int_sequence < N - 1, N - 1, Ns... >
	{};

	template<int... Ns>
	struct make_int_sequence<0, Ns...>
		: int_sequence < Ns... >
	{};

	template<typename T, typename U, int... Ns>
	std::function<void(Args...)> construct_mem_fn(const T& fn, U *p, int_sequence<Ns...>) const{
		return std::bind(fn, p, detail::_slot_autgen_placeholder < Ns > {}...);
	}

};

template <typename R, typename... Args>
class ClassSignal<R(Args...)> : public ClassSignal < Args... >
{};


template<typename... Args> struct Signal : ClassSignal < Args ... > {};
