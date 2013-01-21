#include "core/window.hpp"

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
