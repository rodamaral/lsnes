#include "core/threaddebug.hpp"
#include "core/window.hpp"
#include "library/threadtypes.hpp"

#define DESIGNATED_THREADS 16

namespace
{
	threadid_class threads[DESIGNATED_THREADS];
	volatile bool thread_marked;
	mutex_class malloc_mutex;
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
	threadid_class t = threads[shouldbe];
	if(t != this_thread_id())
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
	threads[call_me] = this_thread_id();
#endif
}

#ifdef MAKE_MALLOC_THREADSAFE
void init_threaded_malloc()
{
	if(!initialized)
		initialized = true;
}

extern "C"
{

void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void __real_free(void*);

void* __wrap_malloc(size_t block)
{
	if(!initialized)
		return __real_malloc(block);
	umutex_class h(malloc_mutex);
	return __real_malloc(block);
}

void* __wrap_calloc(size_t count, size_t size)
{
	if(!initialized)
		return __real_calloc(count, size);
	umutex_class h(malloc_mutex);
	return __real_calloc(count, size);
}

void* __wrap_realloc(void* block, size_t size)
{
	if(!initialized)
		return __real_realloc(block, size);
	umutex_class h(malloc_mutex);
	return __real_realloc(block, size);
}

void __wrap_free(void* block)
{
	if(!initialized)
		return __real_free(block);
	umutex_class h(malloc_mutex);
	return __real_free(block);
}
}
#else
void init_threaded_malloc()
{
}
#endif
