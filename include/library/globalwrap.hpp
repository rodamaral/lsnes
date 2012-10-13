#ifndef _library__globalwrap__hpp__included__
#define _library__globalwrap__hpp__included__

#include <iostream>

/**
 * Wrapper for glboal/module-local objects accessable in global ctor context.
 */
template<class T>
class globalwrap
{
public:
/**
 * Ctor, forces the object to be constructed (to avoid races).
 */
	globalwrap() throw(std::bad_alloc)
	{
		(*this)();
	}
/**
 * Get the wrapped object.
 *
 * returns: The wrapped object.
 * throws std::bad_alloc: Not enough memory.
 */
	T& operator()() throw(std::bad_alloc)
	{
		if(!storage) {
			if(!state)	//State initializes to 0.
				state = 1;
			else if(state == 1)
				std::cerr << "Warning: Recursive use of globalwrap<T>." << std::endl;
			storage = new T();
			state = 2;
		}
		return *storage;
	}
private:
	T* storage;
	unsigned state;
};

#endif
