#ifndef _queue__hpp__included__
#define _queue__hpp__included__

#include "library/exrethrow.hpp"
#include "library/keyboard.hpp"
#include "library/threads.hpp"
#include <deque>

namespace command
{
	class group;
}

/**
 * Information about keypress.
 */
struct keypress_info
{
/**
 * Create null keypress (no modifiers, NULL key and released).
 */
	keypress_info();
/**
 * Create new keypress.
 */
	keypress_info(keyboard::modifier_set mod, keyboard::key& _key, short _value);
/**
 * Create new keypress (two keys).
 */
	keypress_info(keyboard::modifier_set mod, keyboard::key& _key, keyboard::key& _key2, short _value);
/**
 * Modifier set.
 */
	keyboard::modifier_set modifiers;
/**
 * The actual key (first)
 */
	keyboard::key* key1;
/**
 * The actual key (second)
 */
	keyboard::key* key2;
/**
 * Value for the press
 */
	short value;
};

template<typename T>
void functor_call_helper2(void* args)
{
	(*reinterpret_cast<T*>(args))();
	delete reinterpret_cast<T*>(args);
}

struct input_queue
{
	struct function_queue_entry
	{
		std::function<void()> fn;
		std::function<void(std::exception& e)> onerror;
	};

	//Queue stuff.
	threads::lock queue_lock;
	threads::cv queue_condition;
	std::deque<keypress_info> keypresses;
	std::deque<std::pair<const char*, std::string>> commands;
	std::deque<function_queue_entry> functions;
	volatile uint64_t functions_executed;
	volatile uint64_t next_function;
	volatile bool system_thread_available;
	bool queue_function_run;
/**
 * Ctor.
 */
	input_queue(command::group& _command);
/**
 * Queue keypress.
 *
 * - Can be called from any thread.
 *
 * Parameter k: The keypress to queue.
 */
	void queue(const keypress_info& k) throw(std::bad_alloc);
/**
 * Queue command.
 *
 * - Can be called from any thread.
 *
 * Parameter c: The command to queue.
 */
	void queue(const std::string& c) throw(std::bad_alloc);
/**
 * Queue command and arguments.
 *
 * - Can be called from any thread.
 *
 * Parameter c: The command to queue.
 * Parameter a: The arguments for function.
 */
	void queue(const char* c, const std::string& a) throw(std::bad_alloc);
/**
 * Queue function to be called in emulation thread.
 *
 * - Can be called from any thread (exception: Synchronous mode can not be used from emulation nor main threads).
 *
 * Parameter f: The function to execute.
 * Parameter onerror: Function to call on error.
 * Parameter arg: Argument to pass to the function.
 * Parameter sync: If true, execute function call synchronously, else asynchronously.
 */
	void queue(std::function<void()> f, std::function<void(std::exception& e)> onerror, bool sync)
		throw(std::bad_alloc);
/**
 * Run all queues.
 */
	void run_queues() throw();
/**
 * Call function synchronously in emulation thread.
 */
	void run(std::function<void()> fn)
	{
		exrethrow::storage ex;
		queue(fn, [&ex](std::exception& e) { ex = exrethrow::storage(e); }, true);
		if(ex) ex.rethrow();
	}
/**
 * Queue asynchrous function in emulation thread.
 */
	void run_async(std::function<void()> fn,
		std::function<void(std::exception& e)> onerror)
	{
		queue(fn, onerror, false);
	}
/**
 * Run internal queues.
 */
	void run_queue(bool unlocked) throw();
private:
	command::group& command;
};

#endif
