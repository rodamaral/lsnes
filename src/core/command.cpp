#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "library/command.hpp"
#include "library/globalwrap.hpp"
#include "library/keyboard-mapper.hpp"
#include "library/threads.hpp"

#include <set>
#include <map>

command::set lsnes_cmds;

alias_binds_manager::alias_binds_manager(keyboard::mapper& _mapper, command::group& _command)
	: mapper(_mapper), command(_command)
{
}

alias_binds_manager::~alias_binds_manager()
{
	for(auto i : alias_binds) delete i.second;
	alias_binds.clear();
}

void alias_binds_manager::operator()()
{
	threads::alock h(mut);
	auto a = command.get_aliases();
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
			alias_binds[i] = new keyboard::invbind(mapper, i, "Alias‣" + i);
	}
}
