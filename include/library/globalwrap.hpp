#ifndef _library__globalwrap__hpp__included__
#define _library__globalwrap__hpp__included__

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
		if(!storage)
			storage = new T();
	}
/**
 * Get the wrapped object.
 *
 * returns: The wrapped object.
 * throws std::bad_alloc: Not enough memory.
 */
	T& operator()() throw(std::bad_alloc)
	{
		if(!storage)
			storage = new T();
		return *storage;
	}
private:
	T* storage;
};

#endif
