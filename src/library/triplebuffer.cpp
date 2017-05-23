#include "triplebuffer.hpp"
#include <iostream>

namespace triplebuffer
{
logic::logic() throw()
{
	last_complete = 0;
	current_read = 0;
	current_write = 0;
	count_read = 0;
	count_write = 0;
}

unsigned logic::get_read() throw()
{
	threads::alock h(lock);
	if(count_read > 0) {
		//We already are reading => The same as previously.
		count_read++;
		return current_read;
	} else {
		//We are beginning a new read => Pick last_complete.
		count_read++;
		current_read = last_complete;
		return last_complete;
	}
}

void logic::put_read() throw(std::logic_error)
{
	threads::alock h(lock);
	if(!count_read) throw std::logic_error("Internal error: put_read() with 0 counter");
	count_read--;
}

unsigned logic::get_write() throw()
{
	const unsigned magic = 0x010219;
	threads::alock h(lock);
	if(count_write > 0) {
		//We already are writing => The same as previously.
		count_write++;
		return current_write;
	} else {
		//We are beginning a new write => Pick one that isn't last_complete nor current_read.
		count_write++;
		unsigned tmp = ((last_complete << 2) | current_read) << 1;
		current_write = (magic >> tmp) & 3;
		return current_write;
	}
}

void logic::put_write() throw(std::logic_error)
{
	threads::alock h(lock);
	if(!count_write) throw std::logic_error("Internal error: put_write() with 0 counter");
	count_write--;
	//If we reached 0 counter, mark buffer as complete.
	if(!count_write)
		last_complete = current_write;
}

void logic::read_last_write_synchronous(std::function<void(unsigned)> fn) throw()
{
	threads::alock h(lock);
	fn(last_complete);
}

}
