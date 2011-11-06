#include "core/coroutine.hpp"

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <vector>

namespace
{
#if defined(__amd64__)
void trampoline_fn(void (*fn)(void* arg), void* arg) __attribute__((sysv_abi));
#else
#if defined(__i386__)
void trampoline_fn(void (*fn)(void* arg), void* arg) __attribute__((stdcall));
#else
#error "This CPU is not supported"
#endif
#endif
	void trampoline_fn(void (*fn)(void* arg), void* arg)
	{
		fn(arg);
		coroutine::cexit();
	}

#ifdef __amd64__
bool stacks_grow_down = true;
void switch_stacks(void (*fn)(void* arg), void* arg, void* new_esp)
{
	__asm__ __volatile__("movq %%rax,%%rsp; call *%%rdx" :: "D"(fn), "S"(arg), "a"(new_esp), "d"(trampoline_fn));
}
#else
#ifdef __i386__
bool stacks_grow_down = true;
void switch_stacks(void (*fn)(void* arg), void* arg, void* new_esp)
{
	__asm__ __volatile__("movl %%eax,%%esp; pushl %%esi; push %%edi ; call *%%edx" :: "D"(fn), "S"(arg),
		"a"(new_esp), "d"(trampoline_fn));
}
#else
#error "This CPU is not supported"
#endif
#endif
	jmp_buf main_saved_env;
	coroutine* executing_coroutine = NULL;
}

coroutine::coroutine(void (*fn)(void* arg), void* arg, size_t stacksize)
{
	dead = false;
	if(executing_coroutine) {
		std::cerr << "FATAL: Coroutine create only allowed from main coroutine!" << std::endl;
		exit(1);
	}
	executing_coroutine = this;
	if(setjmp(main_saved_env)) {
		executing_coroutine = NULL;
		return;
	}
	stackblock = new unsigned char[stacksize];
	unsigned char* esp = stackblock;
	if(stacks_grow_down)
		esp = esp + stacksize;
	switch_stacks(fn, arg, esp);
}

coroutine::~coroutine() throw()
{
	if(!dead) {
		std::cerr << "FATAL: Trying to delete a live coroutine!" << std::endl;
		exit(1);
	}
	delete[] stackblock;
}

void coroutine::resume()
{
	if(executing_coroutine) {
		std::cerr << "FATAL: Coroutine resume only allowed from main coroutine!" << std::endl;
		exit(1);
	}
	if(dead)
		return;
	executing_coroutine = this;
	if(setjmp(main_saved_env)) {
		executing_coroutine = NULL;
		return;
	}
	longjmp(saved_env, 1);
}

void coroutine::yield()
{
	if(!executing_coroutine) {
		std::cerr << "FATAL: Coroutine yield not allowed from main coroutine!" << std::endl;
		exit(1);
	}
	if(setjmp(executing_coroutine->saved_env))
		return;
	longjmp(main_saved_env, 1);
}

bool coroutine::is_dead()
{
	return dead;
}

void coroutine::cexit()
{
	if(!executing_coroutine) {
		std::cerr << "FATAL: Main coroutine can't exit!" << std::endl;
		exit(1);
	}
	executing_coroutine->dead = true;
	yield();
}

#ifdef TEST_COROUTINES

void fn(void* arg)
{
	std::cout << "Print #1 from coroutine (" << arg << ")" << std::endl;
	coroutine::yield();
	std::cout << "Print #2 from coroutine (" << arg << ")" << std::endl;
	coroutine::yield();
	std::cout << "Print #3 from coroutine (" << arg << ")" << std::endl;
}

int main()
{
	int x;
	coroutine c(fn, &x, 8 * 1024 * 1024);
	std::cout << "Back to main thread" << std::endl;
	std::cout << "Coroutine dead flag is " << c.is_dead() << std::endl;
	c.resume();
	std::cout << "Back to main thread" << std::endl;
	std::cout << "Coroutine dead flag is " << c.is_dead() << std::endl;
	c.resume();
	std::cout << "Back to main thread" << std::endl;
	std::cout << "Coroutine dead flag is " << c.is_dead() << std::endl;
	return 0;
}

#endif
