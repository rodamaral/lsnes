#include "workthread.hpp"
#include <stdexcept>
#include <sys/time.h>
#include <iostream>

namespace workthread
{
const uint32_t quit_request = 0x80000000U;

namespace
{
	uint64_t ticks()
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
	}
}

struct reflector
{
	int operator()(worker* x)
	{
		(*x)(42);
		return 0;
	}
};

worker::worker()
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

worker::~worker()
{
	set_workflag(quit_request);
	if(!joined && thread)
		thread->join();
	delete thread;
	delete reflector;
}

void worker::request_quit()
{
	{
		//If the thread isn't there yet, wait for it.
		threads::alock h(mlock);
		if(!thread)
			condition.wait(h);
	}
	set_workflag(quit_request);
	if(!joined)
		thread->join();
	joined = true;
}

void worker::set_busy()
{
	busy = true;
}

void worker::clear_busy()
{
	threads::alock h(mlock);
	busy = false;
	condition.notify_all();
}

void worker::wait_busy()
{
	threads::alock h(mlock);
	if(busy) {
		uint64_t tmp = ticks();
		while(busy)
			condition.wait(h);
		waitamt_busy += (ticks() - tmp);
	}
}

void worker::rethrow()
{
	if(exception_caught) {
		if(exception_oom)
			throw std::bad_alloc();
		else
			throw std::runtime_error(exception_text);
	}
}

void worker::set_workflag(uint32_t flag)
{
	threads::alock h(mlock);
	workflag |= flag;
	condition.notify_all();
}

uint32_t worker::clear_workflag(uint32_t flag)
{
	threads::alock h(mlock);
	uint32_t tmp = workflag;
	workflag &= ~flag;
	return tmp;
}

uint32_t worker::wait_workflag()
{
	threads::alock h(mlock);
	if(!workflag) {
		uint64_t tmp = ticks();
		while(!workflag)
			condition.wait(h);
		waitamt_work += (ticks() - tmp);
	}
	return workflag;
}

std::pair<uint64_t, uint64_t> worker::get_wait_count()
{
	threads::alock h(mlock);
	return std::make_pair(waitamt_busy, waitamt_work);
}

int worker::operator()(int dummy)
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

void worker::fire()
{
	reflector = new workthread::reflector;
	thread = new threads::thread(*reflector, this);
}
}
