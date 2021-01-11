// sigslot.h: Signal/Slot classes
// 
// Written by Sarah Thompson (sarah@telergy.com) 2002.
//
// License: Public domain. You are free to use this code however you like, with the proviso that
//          the author takes on no responsibility or liability for any use.
//
// QUICK DOCUMENTATION 
//		
//				(see also the full documentation at http://sigslot.sourceforge.net/)
//
//		#define switches
//			SIGSLOT_PURE_ISO			- Define this to force ISO C++ compliance. This also disables
//										  all of the thread safety support on platforms where it is 
//										  available.
//
//			SIGSLOT_USE_POSIX_THREADS	- Force use of Posix threads when using a C++ compiler other than
//										  gcc on a platform that supports Posix threads. (When using gcc,
//										  this is the default - use SIGSLOT_PURE_ISO to disable this if 
//										  necessary)
//
//			SIGSLOT_DEFAULT_MT_POLICY	- Where thread support is enabled, this defaults to multi_threaded_global.
//										  Otherwise, the default is single_threaded. #define this yourself to
//										  override the default. In pure ISO mode, anything other than
//										  single_threaded will cause a compiler error.
//
//		PLATFORM NOTES
//
//			Win32						- On Win32, the WIN32 symbol must be #defined. Most mainstream
//										  compilers do this by default, but you may need to define it
//										  yourself if your build environment is less standard. This causes
//										  the Win32 thread support to be compiled in and used automatically.
//
//			Unix/Linux/BSD, etc.		- If you're using gcc, it is assumed that you have Posix threads
//										  available, so they are used automatically. You can override this
//										  (as under Windows) with the SIGSLOT_PURE_ISO switch. If you're using
//										  something other than gcc but still want to use Posix threads, you
//										  need to #define SIGSLOT_USE_POSIX_THREADS.
//
//			ISO C++						- If none of the supported platforms are detected, or if
//										  SIGSLOT_PURE_ISO is defined, all multithreading support is turned off,
//										  along with any code that might cause a pure ISO C++ environment to
//										  complain. Before you ask, gcc -ansi -pedantic won't compile this 
//										  library, but gcc -ansi is fine. Pedantic mode seems to throw a lot of
//										  errors that aren't really there. If you feel like investigating this,
//										  please contact the author.
//
//		
//		THREADING MODES
//
//			single_threaded				- Your program is assumed to be single threaded from the point of view
//										  of signal/slot usage (i.e. all objects using signals and slots are
//										  created and destroyed from a single thread). Behaviour if objects are
//										  destroyed concurrently is undefined (i.e. you'll get the occasional
//										  segmentation fault/memory exception).
//
//			multi_threaded_global		- Your program is assumed to be multi threaded. Objects using signals and
//										  slots can be safely created and destroyed from any thread, even when
//										  connections exist. In multi_threaded_global mode, this is achieved by a
//										  single global mutex (actually a critical section on Windows because they
//										  are faster). This option uses less OS resources, but results in more
//										  opportunities for contention, possibly resulting in more context switches
//										  than are strictly necessary.
//
//			multi_threaded_local		- Behaviour in this mode is essentially the same as multi_threaded_global,
//										  except that each signal, and each object that inherits has_slots, all 
//										  have their own mutex/critical section. In practice, this means that
//										  mutex collisions (and hence context switches) only happen if they are
//										  absolutely essential. However, on some platforms, creating a lot of 
//										  mutexes can slow down the whole OS, so use this option with care.
//
//		USING THE LIBRARY
//
//			See the full documentation at http://sigslot.sourceforge.net/
//
//

// sigslot.hpp : head file
//
// Version 1.0
//
// Copyright (c) 2020 NauhWuun, All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////
//****************************************************************************

//****************************************************************************
// Update History
//
// Version 1.0, 2021-1-11
//    -First Release
//****************************************************************************

#pragma once
#ifndef SIGSLOT_HPP
#define SIGSLOT_HPP

namespace sigslot 
{
	template<class LockPolicy = std::mutex>
	struct StaticGuard
	{
		LockPolicy mutex;

		StaticGuard() {
			mutex.try_lock();
		}

		explicit StaticGuard(LockPolicy *mtx)
			: mutex(&mtx) 
		{
			mutex.try_lock();
		}

		~StaticGuard() {
			mutex.unlock();
		}
	};

	class has_slots;

	template<typename... args_type>
	struct _connection_bases
	{
		virtual has_slots* getdest() const = 0;
		virtual void emit(args_type...) = 0;
		virtual _connection_bases<args_type...>* clone() = 0;
		virtual _connection_bases<args_type...>* duplicate(has_slots* pnewdest) = 0;
	};

	struct _signal_base {
		virtual void slot_disconnect(has_slots* pslot) = 0;
		virtual void slot_duplicate(const has_slots* poldslot, has_slots* pnewslot) = 0;
	};

	class has_slots
	{
		typedef std::set<_signal_base *> sender_set;
		typedef typename sender_set::const_iterator const_iterator;

		sender_set m_senders;

	public:
		has_slots()
		{}

		has_slots(const has_slots& hs)
		{
			StaticGuard<> guard();
			typename const_iterator it = hs.m_senders.begin();
			typename const_iterator itEnd = hs.m_senders.end();

			while (it != itEnd)
			{
				(*it)->slot_duplicate(&hs, this);
				m_senders.insert(*it);
				++it;
			}
		}

		void signal_connect(_signal_base* sender) {
			StaticGuard<> guard();
			m_senders.insert(sender);
		}

		void signal_disconnect(_signal_base* sender) {
			StaticGuard<> guard();
			m_senders.erase(sender);
		}

		virtual ~has_slots() {
			disconnect_all();
		}

		void disconnect_all()
		{
			StaticGuard<> guard();
			const_iterator it = m_senders.begin();
			const_iterator itEnd = m_senders.end();

			while (it != itEnd) {
				(*it)->slot_disconnect(this);
				++it;
			}

			m_senders.erase(m_senders.begin(), m_senders.end());
		}
	};

	template<class... args_type>
	struct _signal_bases : public _signal_base
	{
		typedef std::list<_connection_bases<args_type...> *> connections_list;
		typename connections_list m_connected_slots;

		_signal_bases()
		{}

		_signal_bases(const _signal_bases<args_type...>& s)
		{
			StaticGuard<> guard();
			typename connections_list::const_iterator it = s.m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = s.m_connected_slots.end();

			while (it != itEnd)
			{
				(*it)->getdest()->signal_connect(this);
				m_connected_slots.push_back((*it)->clone());

				++it;
			}
		}

		void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
		{
			StaticGuard<> guard();
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while (it != itEnd) 
			{
				if ((*it)->getdest() == oldtarget) {
					m_connected_slots.push_back((*it)->duplicate(newtarget));
				}

				++it;
			}
		}

		~_signal_bases() {
			disconnect_all();
		}

		void disconnect_all()
		{
			StaticGuard<> guard();
			typename connections_list::const_iterator it = m_connected_slots.begin();
			typename connections_list::const_iterator itEnd = m_connected_slots.end();

			while (it != itEnd)
			{
				(*it)->getdest()->signal_disconnect(this);
				delete *it;

				++it;
			}

			m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
		}

		void disconnect(has_slots* pclass)
		{
			StaticGuard<> guard();
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while (it != itEnd)
			{
				if ((*it)->getdest() == pclass)
				{
					delete *it;
					m_connected_slots.erase(it);
					pclass->signal_disconnect(this);
					return;
				}

				++it;
			}
		}

		void slot_disconnect(has_slots* pslot)
		{
			StaticGuard<> guard();
			typename connections_list::iterator it = m_connected_slots.begin();
			typename connections_list::iterator itEnd = m_connected_slots.end();

			while (it != itEnd)
			{
				typename connections_list::iterator itNext = it;
				++itNext;

				if ((*it)->getdest() == pslot) {
					m_connected_slots.erase(it);
				}

				it = itNext;
			}
		}  
	};

	template<class dest_type, typename... args_type>
	class _connections : public _connection_bases<args_type...>
	{
		dest_type* m_pobject;
		void (dest_type::* m_pmemfun)(args_type...);

	public:
		_connections() {
			m_pobject = nullptr;
			m_pmemfun = nullptr;
		}

		_connections(dest_type* pobject, void (dest_type::*pmemfun)(args_type...)) {
			m_pobject = pobject;
			m_pmemfun = pmemfun;
		}

		virtual _connection_bases<args_type...>* clone() {
			return new _connections<dest_type, args_type...>(*this);
		}

		virtual _connection_bases<args_type...>* duplicate(has_slots* pnewdest) {
			return new _connections<dest_type, args_type...>((dest_type *)pnewdest, m_pmemfun);
		}

		virtual void emit(args_type... args) {
			(m_pobject->*m_pmemfun)(args...);
		}

		virtual has_slots* getdest() const {
			return m_pobject;
		}
	};

	template<typename... args_type>
	class signals : public _signal_bases<args_type...>
	{
	public:
		signals()
		{}

		signals(const signals<args_type...>& s)
			: _signal_bases<args_type...>(s)
		{}

		template<class desttype>
		void connect(desttype* pclass, void (desttype::* pmemfun)(args_type...))
		{
			StaticGuard<> guard();
			_connections<desttype, args_type...>* conn = new _connections<desttype, args_type...>(pclass, pmemfun);
			_signal_bases<args_type...>::m_connected_slots.push_back(conn);
			pclass->signal_connect(this);
		}

		void emit(args_type... args)
		{
			StaticGuard<> guard();
			typename _signal_bases<args_type...>::connections_list::const_iterator itNext, it = _signal_bases<args_type...>::m_connected_slots.begin();
			typename _signal_bases<args_type...>::connections_list::const_iterator itEnd = _signal_bases<args_type...>::m_connected_slots.end();

			while (it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(args...);

				it = itNext;
			}
		}

		void operator()(args_type... args)
		{
			StaticGuard<> guard();
			typename _signal_bases<args_type...>::connections_list::const_iterator itNext, it = _signal_bases<args_type...>::m_connected_slots.begin();
			typename _signal_bases<args_type...>::connections_list::const_iterator itEnd = _signal_bases<args_type...>::m_connected_slots.end();

			while (it != itEnd)
			{
				itNext = it;
				++itNext;

				(*it)->emit(args...);

				it = itNext;
			}
		}
	};

}
#endif // SIGSLOT_HPP

