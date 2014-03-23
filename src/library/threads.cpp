#include "threads.hpp"
#include <cstdint>

namespace threads
{
void lock_multiple(std::initializer_list<lock*> locks)
{
	uintptr_t next = 0;
	while(true) {
		uintptr_t minaddr = 0;
		lock* minlock = NULL;
		for(auto i : locks) {
			uintptr_t addr = reinterpret_cast<uintptr_t>(i);
			if(addr < minaddr && addr > next) {
				minaddr = addr;
				minlock = i;
			}
		}
		//If no more locks, exit.
		if(!minlock) return;
		//Lock minlock.
		try {
			minlock->lock();
			next = minaddr;
		} catch(...) {
			//Unlock all locks we got.
			for(auto i : locks) {
				uintptr_t addr = reinterpret_cast<uintptr_t>(i);
				if(addr < next)
					i->unlock();
			}
			throw;
		}
	}
}

template<typename T> void _unlock_multiple(T locks)
{
	uintptr_t next = 0;
	while(true) {
		uintptr_t minaddr = 0;
		lock* minlock = NULL;
		for(auto i : locks) {
			uintptr_t addr = reinterpret_cast<uintptr_t>(i);
			if(addr < minaddr && addr > next) {
				minaddr = addr;
				minlock = i;
			}
		}
		//If no more locks, exit.
		if(!minlock) return;
		//Unlock minlock.
		minlock->unlock();
		next = minaddr;
	}
}

void unlock_multiple(std::initializer_list<lock*> locks) { _unlock_multiple(locks); }
void unlock_multiple(std::vector<lock*> locks) { _unlock_multiple(locks); }

}
