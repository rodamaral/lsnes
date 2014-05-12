#include "stateobject.hpp"
#include "threads.hpp"
#include <map>

namespace stateobject
{
threads::lock* lock;
std::map<void*, void*>* states;

void* _get(void* obj, void* (*create)())
{
	if(!lock) lock = new threads::lock;
	threads::alock h(*lock);
	if(!states) states = new std::map<void*, void*>;
	if(!states->count(obj)) {
		(*states)[obj] = NULL;
		try {
			(*states)[obj] = create();
		} catch(...) {
			states->erase(obj);
			throw;
		}
	}
	return (*states)[obj];
}

void* _get_soft(void* obj)
{
	if(!lock) return NULL;
	threads::alock h(*lock);
	if(!states) return NULL;
	if(states->count(obj))
		return (*states)[obj];
	return NULL;
}

void _clear(void* obj, void (*destroy)(void* obj))
{
	if(!lock) return;
	threads::alock h(*lock);
	if(!states) return;
	if(!states->count(obj)) return;
	destroy((*states)[obj]);
	states->erase(obj);
}

}
