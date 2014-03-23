#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "library/globalwrap.hpp"
#include "library/threads.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"

#include <set>
#include <map>

command::group lsnes_cmd;

namespace
{
	threads::lock alias_ibind_mutex;
	std::map<std::string, keyboard::invbind*> alias_binds;
}

void refresh_alias_binds()
{
	threads::alock h(alias_ibind_mutex);
	auto a = lsnes_cmd.get_aliases();
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
			alias_binds[i] = new keyboard::invbind(lsnes_mapper, i, "Alias‣" + i);
	}
}

void kill_alias_binds()
{
	for(auto i : alias_binds) delete i.second;
	alias_binds.clear();
}
