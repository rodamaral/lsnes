#include "core/window.hpp"

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
