#pragma once

// https://github.com/RelicOfTesla/signals
#include <functional>  
#include <map>  
#include <memory>
#include <vector>
#include <type_traits>

namespace detail
{
	template <typename slot_type>
	class signal_t
	{
	public:
		signal_t() : current_id_(0) {}

		template<typename T>
		int connect(T&& slot)  {
			slots_.insert(std::pair<int, slot_type>(++current_id_, std::forward<T>(slot)));
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


template <typename... ARGS>
class FuncSignal : public detail::signal_t < std::function<void(ARGS...)> >
{
};

template <typename R, typename... ARGS>
class FuncSignal<R(ARGS...)> : public FuncSignal < ARGS... >
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
	template<typename T>
	struct remove_all_extend : std::remove_pointer < typename std::remove_reference< typename std::decay<T>::type >::type >
	{};
	template<typename T>
	struct remove_all_extend< std::reference_wrapper<T> > : remove_all_extend < T >
	{};

	template<typename T>
	struct is_conv_trackable : std::is_convertible < typename std::add_pointer< typename remove_all_extend<T>::type  >::type, Trackable* >
	{};

	template<typename T>
	struct enable_if_trackable : std::enable_if < is_conv_trackable<T>::value, void >
	{};


	template <typename func_type>
	class slot_t
	{
	public:
		slot_t() {}
		slot_t(const slot_t& r) : m_slot_func(r.m_slot_func), m_lookup(r.m_lookup)
		{}

		template<typename F>
		slot_t(F&& f) : m_slot_func(f)
		{}

		template<typename... Args>
		void operator()(Args&&... args)
		{
			if (!m_slot_func)
			{
				return;
			}
			for (auto& w : m_lookup)
			{
				if (w.expired())
				{
					m_slot_func = func_type();
					return;
				}
			}
			m_slot_func(args...);
		}

		template<typename T>
		static typename detail::remove_all_extend<T>::type* get_pointer(T&& r){
			return &r;
		};
		template<typename T>
		static typename detail::remove_all_extend<T>::type* get_pointer(T* r){
			return r;
		};
		template<typename T>
		static typename detail::remove_all_extend<T>::type* get_pointer(std::reference_wrapper<T>& r){
			return &(r.get());
		};

		template<typename T
			, class = typename enable_if_trackable<T>::type
		>
		void add_track(T&& obj)
		{
			m_lookup.push_back(get_pointer(obj)->m_trackable_ptr);
		}

	protected:
		func_type m_slot_func;
		std::vector< std::weak_ptr<void> > m_lookup;
	};


	template<int y> struct visit_add_track_t{
		template<typename slot_type, typename T>
		static void run(slot_type& s, T&& a){
		}
	};
	template<> struct visit_add_track_t < 1 > {
		template<typename slot_type, typename T>
		static void run(slot_type& s, T&& a){
			s.add_track(std::forward<T>(a));
		}
	};

	template<typename slot_type, typename T, typename... Args>
	void visit_add_track(slot_type& s, T&& a1, Args&&... args){
		visit_add_track_t< is_conv_trackable<T>::value >::run(s, std::forward<T>(a1));
		visit_add_track(s, std::forward<Args>(args)...);
	}
	template<typename slot_type>
	void visit_add_track(slot_type& s){
	}

	///
	template<int>
	struct _slot_autgen_placeholder
	{
	};

};

namespace std
{
	template<int N>
	struct is_placeholder< detail::_slot_autgen_placeholder<N> >
		: integral_constant < int, N + 1 >
	{
	};
};

template <typename... ARGS>
class ClassSignal : public detail::signal_t < detail::slot_t< std::function<void(ARGS...)> > >
{
public:
	typedef detail::slot_t< std::function<void(ARGS...)> > slot_type;
	typedef detail::signal_t<slot_type> base_type;

	template<typename F, typename... Args>
	int connect(F&& f, Args&&... args)
	{
		slot_type slot(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
		detail::visit_add_track(slot, std::forward<Args>(args)...);
		return base_type::connect(slot);
	}
	template<typename F>
	int connect(F&& f)
	{
		return base_type::connect(std::forward<F>(f));;
	}

	template<typename T>
	int connect_member(void(T::*f)(ARGS...), T* obj)
	{
		static_assert(detail::is_conv_trackable<T>::value, "Not convert to type Trackable");
		slot_type slot(gen_mem_fn(f, obj, make_int_sequence < sizeof...(ARGS) > {}));
		detail::visit_add_track(slot, obj);
		return base_type::connect(slot);
	}
protected:


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

	template<typename F, typename T, int... Ns>
	std::function<void(ARGS...)> gen_mem_fn(const F& fn, T *p, int_sequence<Ns...>) const{
		return std::bind(fn, p, detail::_slot_autgen_placeholder < Ns > {}...);
	}
};

template <typename R, typename... ARGS>
class ClassSignal<R(ARGS...)> : public ClassSignal < ARGS... >
{};


template<typename... T> struct Signal : ClassSignal < T ... > {};
