#include "library/workthread.hpp"
#include <stdexcept>
#include <sys/time.h>

namespace
{
	uint64_t ticks()
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
	}
}

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
	waitamt_busy = 0;
	waitamt_work = 0;
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
	if(busy) {
		uint64_t tmp = ticks();
		while(busy)
			condition.wait(h);
		waitamt_busy += (ticks() - tmp);
	}
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
	if(!workflag) {
		uint64_t tmp = ticks();
		while(!workflag)
			condition.wait(h);
		waitamt_work += (ticks() - tmp);
	}
	return workflag;
}

std::pair<uint64_t, uint64_t> worker_thread::get_wait_count()
{
	umutex_class h(mutex);
	return std::make_pair(waitamt_busy, waitamt_work);
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
