#include "core/threaddebug.hpp"
#include "core/window.hpp"

#define DESIGNATED_THREADS 16

namespace
{
	volatile thread_id* threads[DESIGNATED_THREADS];
	volatile bool thread_marked;
	mutex* malloc_mutex;
}

void assert_thread(signed shouldbe, const std::string& desc)
{
#ifndef DISABLE_THREAD_ASSERTIONS
	if(shouldbe < 0 || shouldbe >= DESIGNATED_THREADS) {
		std::cerr << "WARNING: assert_thread(" << shouldbe << ") called." << std::endl;
		return;
	}
	if(!thread_marked)
		return;
	thread_id* t = const_cast<thread_id*>(threads[shouldbe]);
	if(!t || !t->is_me())
		std::cerr << "WARNING: " << desc << ": Wrong thread!" << std::endl;
#endif
}

void mark_thread_as(signed call_me)
{
#ifndef DISABLE_THREAD_ASSERTIONS
	if(call_me < 0 || call_me >= DESIGNATED_THREADS) {
		std::cerr << "WARNING: mark_thread_as(" << call_me << ") called." << std::endl;
		return;
	}
	thread_marked = true;
	threads[call_me] = &thread_id::me();
#endif
}

#ifdef MAKE_MALLOC_THREADSAFE
void init_threaded_malloc()
{
	if(!malloc_mutex)
		malloc_mutex = &mutex::aquire();
}

extern "C"
{

void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void __real_free(void*);

void* __wrap_malloc(size_t block)
{
	if(!malloc_mutex)
		return __real_malloc(block);
	mutex::holder(*malloc_mutex);
	return __real_malloc(block);
}

void* __wrap_calloc(size_t count, size_t size)
{
	if(!malloc_mutex)
		return __real_calloc(count, size);
	mutex::holder(*malloc_mutex);
	return __real_calloc(count, size);
}

void* __wrap_realloc(void* block, size_t size)
{
	if(!malloc_mutex)
		return __real_realloc(block, size);
	mutex::holder(*malloc_mutex);
	return __real_realloc(block, size);
}

void __wrap_free(void* block)
{
	if(!malloc_mutex)
		return __real_free(block);
	mutex::holder(*malloc_mutex);
	return __real_free(block);
}
}
#else
void init_threaded_malloc()
{
}
#endif
