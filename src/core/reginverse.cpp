#include "library/keyboard.hpp"
#include "cmdhelp/inverselist.hpp"
#include "core/keymapper.hpp"

namespace
{
class register_command_inverses
{
public:
	register_command_inverses()
	{
		const char** ptr = STUBS::inverse_cmd_list;
		while(*ptr) {
			ib.insert(new keyboard::invbind_info(lsnes_invbinds, ptr[0], ptr[1]));
			ptr += 2;
		}
	}
	~register_command_inverses()
	{
		for(auto i : ib)
			delete i;
		ib.clear();
	}
private:
	std::set<keyboard::invbind_info*> ib;
};

register_command_inverses x;
}
