#include "core/window.hpp"

struct dummy_mutex : public mutex
{
	~dummy_mutex() throw();
	void lock() throw();
	void unlock() throw();
};

dummy_mutex::~dummy_mutex() throw()
{
}

void dummy_mutex::lock() throw()
{
}

void dummy_mutex::unlock() throw()
{
}

mutex& mutex::aquire() throw(std::bad_alloc)
{
	return *new dummy_mutex();
}

mutex& mutex::aquire_rec() throw(std::bad_alloc)
{
	return *new dummy_mutex();
}

struct dummy_condition : public condition
{
	dummy_condition(mutex& m);
	~dummy_condition() throw();
	bool wait(uint64_t x) throw();
	void signal() throw();
};

dummy_condition::dummy_condition(mutex& m)
	: condition(m)
{
}

dummy_condition::~dummy_condition() throw()
{
}

bool dummy_condition::wait(uint64_t x) throw()
{
	return false;
}

void dummy_condition::signal() throw()
{
}

condition& condition::aquire(mutex& m) throw(std::bad_alloc)
{
	return *new dummy_condition(m);
}

struct dummy_thread_id : public thread_id
{
	~dummy_thread_id() throw();
	bool is_me() throw();
};

dummy_thread_id::~dummy_thread_id() throw()
{
}

bool dummy_thread_id::is_me() throw()
{
	return true;
}

thread_id& thread_id::me() throw(std::bad_alloc)
{
	return *new dummy_thread_id();
}

struct dummy_thread : public thread
{
	dummy_thread();
	~dummy_thread() throw();
	void _join() throw();
};

dummy_thread::dummy_thread()
{
	notify_quit(NULL);
}

dummy_thread::~dummy_thread() throw()
{
}

void dummy_thread::_join() throw()
{
}

thread& thread::create(void* (*entrypoint)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error)
{
	return *new dummy_thread();
}
