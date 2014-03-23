#ifndef _library__dispatch__hpp__included__
#define _library__dispatch__hpp__included__

#include <iostream>
#include <map>
#include <set>
#include <list>
#include <functional>
#include "threads.hpp"

namespace dispatch
{
threads::lock& global_init_lock();

template<typename... T> struct source;

/**
 * Dispatch target handler.
 */
template<typename... T> struct target
{
/**
 * Create a new target handler.
 */
	target()
	{
		src = NULL;
		fn = dummy;
	}
/**
 * Destroy a target, detaching it from source.
 */
	inline ~target();
/**
 * Connect a target to given source and give a handler.
 *
 * Parameter d: The source to connect to.
 * Parameter _fn: The function to use as handler.
 */
	inline void set(source<T...>& d, std::function<void(T...)> _fn);
private:
	static void dummy(T... args) {};
	void set_source(source<T...>* d) { src = d; }
	void call(T... args) { fn(args...); }
	source<T...>* src;
	std::function<void(T...)> fn;
	friend class source<T...>;
};

/**
 * Dispatch source (event generator).
 */
template<typename... T> struct source
{
/**
 * Create a new event source.
 *
 * Parameter _name: The name of the event.
 */
	source(const char* _name)
	{
		init();
		name = _name;
	}
/**
 * Destory an event source.
 *
 * All targets are disconnected.
 */
	~source()
	{
		delete _targets;
		delete lck;
		lck = NULL;
	}
/**
 * Send an event.
 *
 * Parameter args: The arguments to send.
 */
	void operator()(T... args)
	{
		init();
		uint64_t k = 0;
		typename std::map<uint64_t, target<T...>*>::iterator i;
		lck->lock();
		i = targets().lower_bound(k);
		while(i != targets().end()) {
			k = i->first + 1;
			target<T...>* t = i->second;
			lck->unlock();
			try {
				t->call(args...);
			} catch(std::exception& e) {
				(*errstrm) << name << ": Error in handler: " << e.what() << std::endl;
			}
			lck->lock();
			i = targets().lower_bound(k);
		}
		lck->unlock();
	}
/**
 * Connect a new target.
 *
 * Parameters target: The target to connect.
 */
	void connect(target<T...>& target)
	{
		init();
		threads::alock h(*lck);
		targets()[next_cbseq++] = &target;
		target.set_source(this);
	}
/**
 * Disconnect a target.
 *
 * Parameters target: The target to disconnect.
 */
	void disconnect(target<T...>& target)
	{
		init();
		if(!lck)
			return;
		threads::alock h(*lck);
		for(auto i = targets().begin(); i != targets().end(); i++)
			if(i->second == &target) {
				targets().erase(i);
				break;
			}
		target.set_source(NULL);
	}
/**
 * Set stream to send error messages to.
 *
 * Parameter to: The stream (if NULL, use std::cerr).
 */
	void errors_to(std::ostream* to)
	{
		errstrm = to ? to : &std::cerr;
	}
private:
	void init()
	{
		if(inited)
			return;
		threads::alock h(global_init_lock());
		if(inited)
			return;
		errstrm = &std::cerr;
		next_cbseq = 0;
		name = "(unknown)";
		_targets = NULL;
		lck = new threads::lock;
		inited = true;
	}
	threads::lock* lck;
	std::map<uint64_t, target<T...>*>& targets()
	{
		if(!_targets) _targets = new std::map<uint64_t, target<T...>*>;
		return *_targets;
	}
	std::map<uint64_t, target<T...>*>* _targets;
	uint64_t next_cbseq;
	std::ostream* errstrm;
	const char* name;
	bool inited;
	source(const source<T...>&);
	source<T...>& operator=(const source<T...>&);
};

template <typename... T> target<T...>::~target()
{
	if(src)
		src->disconnect(*this);
}

template <typename... T> void target<T...>::set(source<T...>& d, std::function<void(T...)> _fn)
{
	fn = _fn;
	src = &d;
	if(src)
		src->connect(*this);
}
}
#endif
