#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>

#include "core/emustatus.hpp"
#include "core/instance.hpp"
#include "core/window.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_status.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include <iostream>

#define STATWIDTH 40
#define MAXSTATUS 15

wxwin_status::panel::panel(wxWindow* _parent, emulator_instance& _inst, wxWindow* focuswin, unsigned lines)
	: text_framebuffer_panel(_parent, STATWIDTH, lines ? lines : MAXSTATUS, wxID_ANY, focuswin), inst(_inst)
{
	watch_flag = 0;
}

wxwin_status::wxwin_status(int flag, emulator_instance& _inst, const std::string& title)
	: wxFrame(NULL, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN), inst(_inst)
{
	CHECK_UI_THREAD;
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	top_s->Add(spanel = new wxwin_status::panel(this, inst, NULL, MAXSTATUS), 1, wxGROW);
	spanel->set_watch_flag(flag);
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wxwin_status::~wxwin_status()
{
}

namespace
{
	void register_entry(size_t& count, size_t& width, const std::string& text)
	{
		width = max(width, text_framebuffer::text_width(text));
		count++;
	}

	void show_entry(text_framebuffer_panel& tp, bool& sofar, size_t& line, size_t width, const std::string& name,
		const std::u32string& text)
	{
		size_t n = tp.write(name, width + 1, 0, line, 0, 0xFFFFFF);
		tp.write(text, 0, n, line++, 0, 0xFFFFFF);
		sofar = true;
	}

	void draw_split(text_framebuffer_panel& tp, size_t& line)
	{
		auto s = tp.get_characters();
		for(unsigned i = 0; i < s.first; i++)
			tp.write(U"\u2500", 0, i, line, 0, 0xFFFFFF);
		line++;
	}

	int num_nonzeroes() { return 0; }
	template<typename T, typename... U> int num_nonzeroes(T hd, U... tl) {
		return (hd ? 1 : 0) + num_nonzeroes(tl...);
	}
	template<typename... T> bool one_nonzero(T... args) { return (num_nonzeroes(args...) == 1); }
}

void wxwin_status::panel::prepare_paint()
{
	CHECK_UI_THREAD;
	clear();

	auto& newstatus = inst.status->get_read();
	try {
		bool entry_so_far = false;
		size_t mem_width = 0;
		size_t oth_width = 0;
		size_t lua_width = 0;
		size_t mem_count = 0;
		size_t oth_count = 0;
		size_t lua_count = 0;
		bool single = false;
		for(auto& i : newstatus.mvars) register_entry(mem_count, mem_width, i.first);
		if(newstatus.rtc_valid) register_entry(oth_count, oth_width, "RTC");
		for(size_t j = 0; j < newstatus.inputs.size(); j++)
			register_entry(oth_count, oth_width, (stringfmt() << "P" << (j + 1)).str());
		for(auto& i : newstatus.lvars) register_entry(lua_count, lua_width, i.first);

		if(watch_flag < 0) {
			oth_count = 0;
			lua_count = 0;
		}
		if(watch_flag > 0)
			mem_count = 0;
		single = one_nonzero(mem_count, oth_count, lua_count);

		regex_results r;
		size_t p = 0;
		if(mem_count) {
			if(entry_so_far) draw_split(*this, p);
			if(!single) write("Memory watches:", 0, 0, p++, 0, 0xFFFFFF);
			for(auto i : newstatus.mvars) show_entry(*this, entry_so_far, p, mem_width, i.first,
				i.second);
		}

		if(oth_count) {
			if(entry_so_far) draw_split(*this, p);
			if(!single) write("Status:", 0, 0, p++, 0, 0xFFFFFF);
			if(newstatus.rtc_valid) show_entry(*this, entry_so_far, p, oth_width, "RTC", newstatus.rtc);
			for(size_t j = 0; j < newstatus.inputs.size(); j++)
				show_entry(*this, entry_so_far, p, oth_width, (stringfmt() << "P" << (j + 1)).str(),
					newstatus.inputs[j]);
		}

		if(lua_count) {
			if(entry_so_far) draw_split(*this, p);
			if(!single) write("From Lua:", 0, 0, p++, 0, 0xFFFFFF);
			for(auto i : newstatus.lvars) show_entry(*this, entry_so_far, p, lua_width, i.first,
				i.second);
		}
	} catch(...) {
	}
	inst.status->put_read();
}

void wxwin_status::notify_update() throw()
{
	CHECK_UI_THREAD;
	spanel->request_paint();
}

bool wxwin_status::ShouldPreventAppExit() const
{
	return false;
}
