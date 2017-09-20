#pragma once

// https://github.com/RelicOfTesla/signals
#include <functional>  
#include <map>  
#include <memory>
#include <vector>
#include <type_traits>

namespace SignalDetail
{
	struct connection_base
	{
		virtual void disconnect() = 0;
	};
	typedef std::shared_ptr<connection_base> connect_type;

	template <typename slot_type>
	class signal_t
	{
	public:
		signal_t() : current_id_(0), m_lookup(this, _lookup_release)
		{}

		template<typename T>
		connect_type connect(T&& slot)  {
			auto id = ++current_id_;
			slots_.insert(std::pair<int, slot_type>(id, std::forward<T>(slot)));
			return std::make_shared<connection_t>(m_lookup, id);
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
	protected:
		struct connection_t : connection_base
		{
			connection_t(std::weak_ptr<signal_t> wp, int id) : m_wp(wp), m_id(id)
			{}

			void disconnect() override
			{
				if (m_id) {
					if (auto sp = m_wp.lock()) {
						sp->disconnect_id(m_id);
					}
					m_id = 0;
				}
			}
		private:
			std::weak_ptr< signal_t > m_wp;
			int m_id;
		};
		void disconnect_id(int id) {
			slots_.erase(id);
		}

	private:
		mutable std::map< int, slot_type> slots_;
		int current_id_;
		std::shared_ptr< signal_t > m_lookup;

		static void _lookup_release(signal_t*){}
	private:
		signal_t(const signal_t& other);
		void operator =(const signal_t & other);

	};
};


template <typename... ARGS>
class FuncSignal : public SignalDetail::signal_t < std::function<void(ARGS...)> >
{
};

template <typename R, typename... ARGS>
class FuncSignal<R(ARGS...)> : public FuncSignal < ARGS... >
{};

//////////////////////////////////////////////////////////////////////////
struct Trackable
{
	std::shared_ptr<void> m_trackable_ptr;

	Trackable() : m_trackable_ptr((void*)&_Trackable_destory, _Trackable_destory)
	{}
	static void _Trackable_destory(void*){}
};

namespace SignalDetail
{
	template<typename T>
	struct remove_all_extend : std::remove_pointer < typename std::remove_reference< typename std::decay<T>::type >::type >
	{};
	template<typename T> struct remove_all_extend <T*> : remove_all_extend < T > {};
	template<typename T> struct remove_all_extend <T&> : remove_all_extend < T > {};

	template<typename T>
	static T* get_pointer(T& r){
		return &r;
	};
	template<typename T>
	static T* get_pointer(T* r){
		return r;
	};
	template<typename T>
	static T* get_pointer(std::reference_wrapper<T>&& r){
		return &(r.get());
	};
	//////////////////////////////////////////////////////////////////////////
	template<typename T>
	struct is_conv_trackable : std::is_convertible < typename remove_all_extend<T>::type*, Trackable* >
	{};
	template<typename T> struct is_conv_trackable< std::reference_wrapper<T> > : is_conv_trackable < T > {};

	template <typename func_impl>
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
					m_slot_func = func_impl();
					return;
				}
			}
			m_slot_func(args...);
		}


		template<typename T
			, class = typename std::enable_if <is_conv_trackable<T>::value>::type
		>
		void add_track(T&& obj)
		{
			m_lookup.push_back(get_pointer(std::forward<T>(obj))->m_trackable_ptr);
		}

	protected:
		func_impl m_slot_func;
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
	template <typename func_impl>
	class slot_signal_t : public signal_t < func_impl >
	{
	private:
		typedef signal_t<func_impl> base_type;
	public:
		typedef slot_t<func_impl> slot_type;

		template<typename F, typename... Args>
		connect_type connect(F&& f, Args&&... args)
		{
			slot_type slot(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
			visit_add_track(slot, std::forward<Args>(args)...);
			return base_type::connect(slot);
		}
		template<typename F>
		connect_type connect(F&& f)
		{
			return base_type::connect(std::forward<F>(f));
		}
	};
}

template <typename... ARGS>
class WeakSignal : public SignalDetail::slot_signal_t < std::function<void(ARGS...)> >
{};

template <typename R, typename... ARGS>
class WeakSignal<R(ARGS...)> : public WeakSignal < ARGS... >
{};
//////////////////////////////////////////////////////////////////////////
namespace SignalDetail
{
	template<int>
	struct _slot_autgen_placeholder
	{
	};

	template <typename... ARGS>
	class MemberSignal : public WeakSignal < ARGS... >
	{
	private:
		typedef WeakSignal < ARGS... > base_type;
		typedef typename base_type::slot_type slot_type;
	public:

		template<typename T>
		connect_type connect_member(void(T::*f)(ARGS...), T* obj)
		{
			static_assert(is_conv_trackable<T>::value, "Not convert to type Trackable");
			slot_type slot(gen_mem_fn(f, obj, make_int_sequence < sizeof...(ARGS) > {}));
			visit_add_track(slot, obj);
			return base_type::connect(slot);
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

		template<typename F, typename T, int... Ns>
		std::function<void(ARGS...)> gen_mem_fn(const F& fn, T *p, int_sequence<Ns...>) const{
			return std::bind(fn, p, _slot_autgen_placeholder < Ns > {}...);
		}
	};
}
namespace std
{
	template<int N>
	struct is_placeholder< SignalDetail::_slot_autgen_placeholder<N> >
		: integral_constant < int, N + 1 >
	{
	};
};

template <typename ... ARGS>
class MemberSignal : public SignalDetail::MemberSignal < ARGS... >
{};
template <typename R, typename... ARGS>
class MemberSignal<R(ARGS...)> : public MemberSignal < ARGS... >
{};
//////////////////////////////////////////////////////////////////////////

//template<typename... T> struct Signal : FuncSignal < T ... > {};
//template<typename... T> struct Signal : WeakSignal < T ... > {};
template<typename... T> struct Signal : MemberSignal < T ... > {};

typedef SignalDetail::connect_type SignalConnect;
