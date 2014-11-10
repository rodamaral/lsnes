#include "gc.hpp"
#include <list>
#include <iostream>
#include <set>

namespace GC
{
namespace
{
	std::set<item*>* gc_items;
}

item::item()
{
	if(!gc_items) gc_items = new std::set<item*>;
	gc_items->insert(this);
	root_count = 1;
}

item::~item()
{
	gc_items->erase(this);
}

void item::mark_root()
{
	root_count++;
}

void item::unmark_root()
{
	if(root_count) root_count--;
}

void item::do_gc()
{
	if(!gc_items) return;
	for(auto i : *gc_items)
		i->reachable = false;
	for(auto i : *gc_items) {
		if(i->root_count) {
			i->mark();
		}
	}
	for(auto i = gc_items->begin(); i != gc_items->end();) {
		if(!(*i)->reachable) {
			auto ptr = i;
			i++;
			delete *ptr;
		} else
			i++;
	}
}

void item::mark()
{
	bool was_reachable = reachable;
	reachable = true;
	if(!was_reachable) {
		trace();
	}
}
}
