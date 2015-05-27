#ifndef _library__eatarg__hpp__included__
#define _library__eatarg__hpp__included__

void  __attribute__((noinline)) _eat_argument(void* arg);

template<typename T> void eat_argument(T arg) {
	T* arg2 = &arg;
	_eat_argument((void*)arg2);
}

#endif
