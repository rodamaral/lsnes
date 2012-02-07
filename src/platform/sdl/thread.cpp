#include "core/window.hpp"


#include <SDL.h>
#include <SDL_thread.h>

struct sdl_mutex : public mutex
{
	sdl_mutex() throw(std::bad_alloc);
	~sdl_mutex() throw();
	void lock() throw();
	void unlock() throw();
	SDL_mutex* m;
};

sdl_mutex::sdl_mutex() throw(std::bad_alloc)
{
	m = SDL_CreateMutex();
	if(!m)
		throw std::bad_alloc();
}

sdl_mutex::~sdl_mutex() throw()
{
	SDL_DestroyMutex(m);
}

void sdl_mutex::lock() throw()
{
	SDL_mutexP(m);
}

void sdl_mutex::unlock() throw()
{
	SDL_mutexV(m);
}

mutex& mutex::aquire() throw(std::bad_alloc)
{
	return *new sdl_mutex;
}

struct sdl_condition : public condition
{
	sdl_condition(mutex& m) throw(std::bad_alloc);
	~sdl_condition() throw();
	bool wait(uint64_t x) throw();
	void signal() throw();
	SDL_cond* c;
};

sdl_condition::sdl_condition(mutex& m) throw(std::bad_alloc)
	: condition(m)
{
	c = SDL_CreateCond();
	if(!c)
		throw std::bad_alloc();
}

sdl_condition::~sdl_condition() throw()
{
	SDL_DestroyCond(c);
}

bool sdl_condition::wait(uint64_t x) throw()
{
	sdl_mutex* m = dynamic_cast<sdl_mutex*>(&associated());
	if(!m)
		return false;
	return (SDL_CondWaitTimeout(c, m->m, (x + 999) / 1000) == 0);
}

void sdl_condition::signal() throw()
{
	SDL_CondBroadcast(c);
}

condition& condition::aquire(mutex& m) throw(std::bad_alloc)
{
	return *new sdl_condition(m);
}

struct sdl_thread_id : public thread_id
{
	sdl_thread_id() throw();
	~sdl_thread_id() throw();
	bool is_me() throw();
	uint32_t id;
};

sdl_thread_id::sdl_thread_id() throw()
{
	id = SDL_ThreadID();
}


sdl_thread_id::~sdl_thread_id() throw()
{
}

bool sdl_thread_id::is_me() throw()
{
	return (id == SDL_ThreadID());
}

thread_id& thread_id::me() throw(std::bad_alloc)
{
	return *new sdl_thread_id;
}

struct sdl_thread : public thread
{
	sdl_thread(void* (*fn)(void* arg), void* arg) throw(std::runtime_error);
	~sdl_thread() throw();
	void _join() throw();
	void* (*entry)(void* arg);
	void* entry_arg;
	static int sdl_entrypoint(void* arg);
	SDL_Thread* handle;
};

int sdl_thread::sdl_entrypoint(void* arg)
{
	sdl_thread* t = reinterpret_cast<sdl_thread*>(arg);
	t->notify_quit(t->entry(t->entry_arg));
}

sdl_thread::sdl_thread(void* (*fn)(void* arg), void* arg) throw(std::runtime_error)
{
	entry = fn;
	entry_arg = arg;
	handle = SDL_CreateThread(sdl_entrypoint, this);
	if(!handle)
		throw std::runtime_error("Can't create thread");
}

sdl_thread::~sdl_thread() throw()
{
	_join();
}

void sdl_thread::_join() throw()
{
	if(handle)
		SDL_WaitThread(handle, NULL);
	handle = NULL;
}

thread& thread::create(void* (*entrypoint)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error)
{
	return *new sdl_thread(entrypoint, arg);
}
