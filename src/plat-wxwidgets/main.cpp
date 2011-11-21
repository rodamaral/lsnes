#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/lua.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/window.hpp"
#include "core/zip.hpp"

#include "plat-wxwidgets/messages_window.hpp"
#include "plat-wxwidgets/rom_select_window.hpp"
#include "plat-wxwidgets/status_window.hpp"

#include <cassert>
#include <boost/lexical_cast.hpp>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>


class lsnes_app : public wxApp
{
public:
	virtual bool OnInit();
	virtual int OnExit();
};

IMPLEMENT_APP(lsnes_app)

bool lsnes_app::OnInit()
{
	set_random_seed();
	foreground_application();

	{
		std::ostringstream x;
		x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
		bsnes_core_version = x.str();
	}
	window::init();
	init_lua();

	messages << "BSNES version: " << bsnes_core_version << std::endl;
	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;

	std::string cfgpath = get_config_path();
	messages << "Saving per-user data to: " << get_config_path() << std::endl;

	wx_messages_window* msgs = new wx_messages_window();
	window1 = msgs;
	msgs->Show();

	wx_rom_select_window* romwin = new wx_rom_select_window();
	romwin->Show();

	return true;
}

int lsnes_app::OnExit()
{
	information_dispatch::do_dump_end();
	rrdata::close();
	window::quit();
	return 0;
}

void window::notify_message() throw(std::bad_alloc, std::runtime_error)
{
	if(wx_messages_window::ptr)
		wx_messages_window::ptr->notify_message();
}
