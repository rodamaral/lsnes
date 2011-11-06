#ifndef _coroutine__hpp__included__
#define _coroutine__hpp__included__

#include <cstdlib>
#include <csetjmp>

/**
 * A coroutine.
 */
class coroutine
{
public:
/**
 * Create a new coroutine with specified starting function and stack size. The coroutine created will run until it
 * yields for the first time.
 *
 * This can only be called from outside any coroutine.
 *
 * parameter fn: The function to call.
 * parameter arg: Argument to pass to the function.
 * parameter stacksize: Size of stack to allocate for function.
 * throws std::bad:alloc: Not enough memory.
 */
	coroutine(void (*fn)(void* arg), void* arg, size_t stacksize);
/**
 * Destructor.
 */
	~coroutine() throw();
/**
 * Resume yielded coroutine.
 *
 * This can only be called from outside any coroutine.
 */
	void resume();
/**
 * Yield execution, causing coroutine::resume() or coroutine::coroutine() that called this coroutine to return.
 *
 * This can only be called from coroutine.
 */
	static void yield();

/**
 * Is the coroutine dead (has returned?)
 */
	bool is_dead();

/**
 * Exit the coroutine (yield and mark it dead).
 */
	static void cexit();
private:
	jmp_buf saved_env;
	bool dead;
	unsigned char* stackblock;
};



#endif