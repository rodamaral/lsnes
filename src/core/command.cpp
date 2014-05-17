#include "core/command.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "library/globalwrap.hpp"
#include "library/threads.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"

#include <set>
#include <map>

command::set lsnes_cmds;

namespace
{
	threads::rlock* global_lock;
	threads::rlock& get_invbind_lock()
	{
		if(!global_lock) global_lock = new threads::rlock;
		return *global_lock;
	}
	std::map<std::string, keyboard::invbind*> alias_binds;
}

alias_binds_manager::alias_binds_manager(emulator_instance& _instance)
	: instance(_instance)
{
}

alias_binds_manager::~alias_binds_manager()
{
	for(auto i : alias_binds) delete i.second;
	alias_binds.clear();
}

void alias_binds_manager::operator()()
{
	threads::arlock h(get_invbind_lock());
	auto a = instance.command.get_aliases();
	for(auto i : alias_binds) {
		if(!a.count(i.first)) {
			delete i.second;
			alias_binds[i.first] = NULL;
		}
	}
	for(auto i : a) {
		if(i == "" || i[0] == '-')
			continue;
		if(!alias_binds.count(i) || alias_binds[i] == NULL)
			alias_binds[i] = new keyboard::invbind(instance.mapper, i, "Alias‣" + i);
	}
}
