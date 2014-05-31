#include "core/misc.hpp"
#include "core/queue.hpp"
#include "core/random.hpp"
#include "library/command.hpp"
#include "library/threads.hpp"

input_queue::input_queue(command::group& _command)
	: command(_command)
{
	system_thread_available = false;
	queue_function_run = false;
}


keypress_info::keypress_info()
{
	key1 = NULL;
	key2 = NULL;
	value = 0;
}

keypress_info::keypress_info(keyboard::modifier_set mod, keyboard::key& _key, short _value)
{
	modifiers = mod;
	key1 = &_key;
	key2 = NULL;
	value = _value;
}

keypress_info::keypress_info(keyboard::modifier_set mod, keyboard::key& _key, keyboard::key& _key2, short _value)
{
	modifiers = mod;
	key1 = &_key;
	key2 = &_key2;
	value = _value;
}


void input_queue::queue(const keypress_info& k) throw(std::bad_alloc)
{
	threads::alock h(queue_lock);
	keypresses.push_back(k);
	queue_condition.notify_all();
}

void input_queue::queue(const std::string& c) throw(std::bad_alloc)
{
	threads::alock h(queue_lock);
	commands.push_back(c);
	queue_condition.notify_all();
}

void input_queue::queue(std::function<void()> f, std::function<void(std::exception& e)> onerror, bool sync)
	throw(std::bad_alloc)
{
	if(!system_thread_available) {
		try {
			f();
		} catch(std::exception& e) {
			onerror(e);
		}
		return;
	}
	threads::alock h(queue_lock);
	++next_function;
	function_queue_entry entry;
	entry.fn = f;
	entry.onerror = onerror;
	functions.push_back(entry);
	queue_condition.notify_all();
	if(sync)
		while(functions_executed < next_function && system_thread_available) {
			threads::cv_timed_wait(queue_condition, h, threads::ustime(10000));
			random_mix_timing_entropy();
		}
}

void input_queue::run_queue(bool unlocked) throw()
{
	if(!unlocked)
		queue_lock.lock();
	try {
		//Flush keypresses.
		while(!keypresses.empty()) {
			keypress_info k = keypresses.front();
			keypresses.pop_front();
			queue_lock.unlock();
			if(k.key1)
				k.key1->set_state(k.modifiers, k.value);
			if(k.key2)
				k.key2->set_state(k.modifiers, k.value);
			queue_lock.lock();
			queue_function_run = true;
		}
		//Flush commands.
		while(!commands.empty()) {
			std::string c = commands.front();
			commands.pop_front();
			queue_lock.unlock();
			command.invoke(c);
			queue_lock.lock();
			queue_function_run = true;
		}
		//Flush functions.
		while(!functions.empty()) {
			function_queue_entry f = functions.front();
			functions.pop_front();
			queue_lock.unlock();
			try {
				f.fn();
			} catch(std::exception& e) {
				f.onerror(e);
			}
			queue_lock.lock();
			++functions_executed;
			queue_function_run = true;
		}
		queue_condition.notify_all();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		std::cerr << "Fault inside platform::run_queues(): " << e.what() << std::endl;
		exit(1);
	}
	if(!unlocked)
		queue_lock.unlock();
}
