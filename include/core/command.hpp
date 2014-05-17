#ifndef _command__hpp__included__
#define _command__hpp__included__

#include <stdexcept>
#include <string>
#include <set>
#include "library/command.hpp"
#include "library/keyboard-mapper.hpp"

extern command::set lsnes_cmds;

class emulator_instance;

class alias_binds_manager
{
public:
	alias_binds_manager(emulator_instance& _instance);
	~alias_binds_manager();
	void operator()();
private:
	emulator_instance& instance;
	std::map<std::string, keyboard::invbind*> alias_binds;
};

#endif
