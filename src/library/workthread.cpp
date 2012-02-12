#include "library/workthread.hpp"
#include <stdexcept>

struct worker_thread_reflector
{
	int operator()(worker_thread* x)
	{
		(*x)(42);
	}
};

worker_thread::worker_thread()
{
	thread = NULL;
	reflector = NULL;
	workflag = 0;
	busy = false;
	exception_caught = false;
	exception_oom = false;
	joined = false;
}

worker_thread::~worker_thread()
{
	set_workflag(WORKFLAG_QUIT_REQUEST);
	if(!joined && thread)
		thread->join();
	delete thread;
	delete reflector;
}

void worker_thread::request_quit()
{
	{
		//If the thread isn't there yet, wait for it.
		umutex_class h(mutex);
		if(!thread)
			condition.wait(h);
	}
	set_workflag(WORKFLAG_QUIT_REQUEST);
	if(!joined)
		thread->join();
	joined = true;
}

void worker_thread::set_busy()
{
	busy = true;
}

void worker_thread::clear_busy()
{
	umutex_class h(mutex);
	busy = false;
	condition.notify_all();
}

void worker_thread::wait_busy()
{
	umutex_class h(mutex);
	while(busy)
		condition.wait(h);
}

void worker_thread::rethrow()
{
	if(exception_caught) {
		if(exception_oom)
			throw std::bad_alloc();
		else
			throw std::runtime_error(exception_text);
	}
}

void worker_thread::set_workflag(uint32_t flag)
{
	umutex_class h(mutex);
	workflag |= flag;
	condition.notify_all();
}

uint32_t worker_thread::clear_workflag(uint32_t flag)
{
	umutex_class h(mutex);
	uint32_t tmp = workflag;
	workflag &= ~flag;
	return tmp;
}

uint32_t worker_thread::wait_workflag()
{
	umutex_class h(mutex);
	while(!workflag)
		condition.wait(h);
	return workflag;
}

int worker_thread::operator()(int dummy)
{
	try {
		entry();
	} catch(std::bad_alloc& e) {
		exception_oom = true;
		exception_caught = true;
		return 1;
	} catch(std::exception& e) {
		exception_text = e.what();
		exception_caught = true;
		return 1;
	}
	return 0;
}

void worker_thread::fire()
{
	reflector = new worker_thread_reflector;
	thread = new thread_class(*reflector, this);
}
