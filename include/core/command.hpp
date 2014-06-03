#ifndef _command__hpp__included__
#define _command__hpp__included__

#include "library/command.hpp"
#include "library/threads.hpp"
#include <stdexcept>
#include <string>
#include <set>

extern command::set lsnes_cmds;

namespace keyboard
{
	class mapper;
	class invbind;
}

class alias_binds_manager
{
public:
	alias_binds_manager(keyboard::mapper& _mapper, command::group& _command);
	~alias_binds_manager();
	void operator()();
private:
	keyboard::mapper& mapper;
	command::group& command;
	threads::lock mut;
	std::map<std::string, keyboard::invbind*> alias_binds;
};

#endif
