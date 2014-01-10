#include "gc.hpp"
#include <list>
#include <iostream>
#include <set>

namespace
{
	std::set<garbage_collectable*> gc_items;
}

garbage_collectable::garbage_collectable()
{
	gc_items.insert(this);
	root_count = 1;
}

garbage_collectable::~garbage_collectable()
{
	gc_items.erase(this);
}

void garbage_collectable::mark_root()
{
	root_count++;
}

void garbage_collectable::unmark_root()
{
	if(root_count) root_count--;
}

void garbage_collectable::do_gc()
{
	for(auto i : gc_items)
		i->reachable = false;
	for(auto i : gc_items) {
		if(i->root_count) {
			i->mark();
		}
	}
	for(auto i = gc_items.begin(); i != gc_items.end();) {
		if(!(*i)->reachable) {
			auto ptr = i;
			i++;
			delete *ptr;
		} else
			i++;
	}
}

void garbage_collectable::mark()
{
	bool was_reachable = reachable;
	reachable = true;
	if(!was_reachable) {
		trace();
	}
}
