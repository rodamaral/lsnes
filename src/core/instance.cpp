#include "core/instance.hpp"
#include "core/settings.hpp"
#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "core/random.hpp"
#ifdef __linux__
#include <execinfo.h>
#endif

emulator_instance::emulator_instance()
	: setcache(settings), subtitles(&mlogic), mbranch(&mlogic), mteditor(&mlogic),
	status(status_A, status_B, status_C), mapper(keyboard, command), abindmanager(*this),
	cmapper(memory)
{
	system_thread_available = false;
	queue_function_run = false;
	status_A.valid = false;
	status_B.valid = false;
	status_C.valid = false;
	command.add_set(lsnes_cmds);
	mapper.add_invbind_set(lsnes_invbinds);
	settings.add_set(lsnes_setgrp);
}

emulator_instance lsnes_instance;

emulator_instance& CORE()
{
	if(threads::id() != lsnes_instance.emu_thread) {
		std::cerr << "WARNING: CORE() called in wrong thread." << std::endl;
#ifdef __linux__
		void* arr[256];
		backtrace_symbols_fd(arr, 256, 2);
#endif
	}
	return lsnes_instance;
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


void emulator_instance::queue(const keypress_info& k) throw(std::bad_alloc)
{
	threads::alock h(queue_lock);
	keypresses.push_back(k);
	queue_condition.notify_all();
}

void emulator_instance::queue(const std::string& c) throw(std::bad_alloc)
{
	threads::alock h(queue_lock);
	commands.push_back(c);
	queue_condition.notify_all();
}

void emulator_instance::queue(void (*f)(void* arg), void* arg, bool sync) throw(std::bad_alloc)
{
	if(!system_thread_available) {
		f(arg);
		return;
	}
	threads::alock h(queue_lock);
	++next_function;
	functions.push_back(std::make_pair(f, arg));
	queue_condition.notify_all();
	if(sync)
		while(functions_executed < next_function && system_thread_available) {
			threads::cv_timed_wait(queue_condition, h, threads::ustime(10000));
			random_mix_timing_entropy();
		}
}

void emulator_instance::run_queue(bool unlocked) throw()
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
			CORE().command.invoke(c);
			queue_lock.lock();
			queue_function_run = true;
		}
		//Flush functions.
		while(!functions.empty()) {
			std::pair<void(*)(void*), void*> f = functions.front();
			functions.pop_front();
			queue_lock.unlock();
			f.first(f.second);
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
