#ifndef _library__dispatch__hpp__included__
#define _library__dispatch__hpp__included__

#include <iostream>
#include <map>
#include <set>
#include <list>
#include <functional>
#include "threadtypes.hpp"

mutex_class& dispatch_global_init_lock();

template<typename... T> struct dispatcher;

template<typename... T> struct dispatch_target
{
	dispatch_target()
	{
		src = NULL;
		fn = dummy;
	}
	inline ~dispatch_target();
	inline void set(dispatcher<T...>& d, std::function<void(T...)> _fn);
private:
	static void dummy(T... args) {};
	void set_source(dispatcher<T...>* d) { src = d; }
	void call(T... args) { fn(args...); }
	dispatcher<T...>* src;
	std::function<void(T...)> fn;
	friend class dispatcher<T...>;
};

template<typename... T> struct dispatcher
{
	dispatcher(const char* _name)
	{
		init();
		name = _name;
	}
	~dispatcher()
	{
		delete _targets;
		delete lck;
		lck = NULL;
	}
	void operator()(T... args)
	{
		init();
		uint64_t k = 0;
		typename std::map<uint64_t, dispatch_target<T...>*>::iterator i;
		lck->lock();
		i = targets().lower_bound(k);
		while(i != targets().end()) {
			k = i->first + 1;
			dispatch_target<T...>* t = i->second;
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
	void connect(dispatch_target<T...>& target)
	{
		init();
		umutex_class h(*lck);
		targets()[next_cbseq++] = &target;
		target.set_source(this);
	}
	void disconnect(dispatch_target<T...>& target)
	{
		init();
		if(!lck)
			return;
		umutex_class h(*lck);
		for(auto i = targets().begin(); i != targets().end(); i++)
			if(i->second == &target) {
				targets().erase(i);
				break;
			}
		target.set_source(NULL);
	}
	void errors_to(std::ostream* to)
	{
		errstrm = to ? to : &std::cerr;
	}
private:
	void init()
	{
		if(inited)
			return;
		umutex_class h(dispatch_global_init_lock());
		if(inited)
			return;
		errstrm = &std::cerr;
		next_cbseq = 0;
		name = "(unknown)";
		_targets = NULL;
		lck = new mutex_class;
		inited = true;
	}
	mutex_class* lck;
	std::map<uint64_t, dispatch_target<T...>*>& targets()
	{
		if(!_targets) _targets = new std::map<uint64_t, dispatch_target<T...>*>;
		return *_targets;
	}
	std::map<uint64_t, dispatch_target<T...>*>* _targets;
	uint64_t next_cbseq;
	std::ostream* errstrm;
	const char* name;
	bool inited;
	dispatcher(const dispatcher<T...>&);
	dispatcher<T...>& operator=(const dispatcher<T...>&);
};

template <typename... T> dispatch_target<T...>::~dispatch_target()
{
	if(src)
		src->disconnect(*this);
}

template <typename... T> void dispatch_target<T...>::set(dispatcher<T...>& d, std::function<void(T...)> _fn)
{
	fn = _fn;
	src = &d;
	if(src)
		src->connect(*this);
}

#endif
