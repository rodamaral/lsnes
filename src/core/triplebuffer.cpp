#include "core/triplebuffer.hpp"

triplebuffer_logic::triplebuffer_logic() throw(std::bad_alloc)
{
	mut = &mutex::aquire();
	last_complete_slot = 0;
	read_active = false;
	write_active = false;
	read_active_slot = 0;
	write_active_slot = 0;
}

triplebuffer_logic::~triplebuffer_logic() throw()
{
	delete mut;
}

unsigned triplebuffer_logic::start_write() throw()
{
	mutex::holder h(*mut);
	if(!write_active) {
		//We need to avoid hitting last complete slot or slot that is active for read.
		if(last_complete_slot != 0 && read_active_slot != 0)
			write_active_slot = 0;
		else if(last_complete_slot != 1 && read_active_slot != 1)
			write_active_slot = 1;
		else
			write_active_slot = 2;
	}
	write_active++;
	return write_active_slot;
}

void triplebuffer_logic::end_write() throw()
{
	mutex::holder h(*mut);
	if(!--write_active)
		last_complete_slot = write_active_slot;
}

unsigned triplebuffer_logic::start_read() throw()
{
	mutex::holder h(*mut);
	if(!read_active)
		read_active_slot = last_complete_slot;
	read_active++;
	return read_active_slot;
}

void triplebuffer_logic::end_read() throw()
{
	mutex::holder h(*mut);
	read_active--;
}
