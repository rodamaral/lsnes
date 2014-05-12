#ifndef _library__stateobject__hpp__included__
#define _library__stateobject__hpp__included__

#include <stdexcept>

namespace stateobject
{
//Internal methods.
void* _get(void* obj, void* (*create)());
void* _get_soft(void* obj);
void _clear(void* obj, void (*destroy)(void* obj));

template<typename T, typename U> class type
{
public:
/**
 * Get state object, creating it if needed.
 *
 * Parameter obj: The object to get the state for.
 * Returns: The object state.
 */
	static U& get(T* obj) throw(std::bad_alloc)
	{
		return *reinterpret_cast<U*>(_get(obj, []() -> void* { return new U; }));
	}
/**
 * Get state object, but don't create if it does not already exist.
 *
 * Parameter obj: The object to get the state for.
 * Returns: The object state, or NULL if no existing state.
 */
	static U* get_soft(T* obj) throw() { return reinterpret_cast<U*>(_get_soft(obj)); }
/**
 * Clear state object.
 */
	static void clear(T* obj) throw()
	{
		return _clear(obj, [](void* obj) -> void { delete reinterpret_cast<U*>(obj); });
	}
};

}

#endif
