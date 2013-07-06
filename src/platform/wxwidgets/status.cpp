#include "core/window.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_status.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include <iostream>

#define STATWIDTH 40
#define MAXSTATUS 15

namespace
{
	std::string string_pad(const std::string& x, size_t width)
	{
		if(x.length() >= width)
			return x;
		std::string y = x;
		y.append(width - y.length(), ' ');
		return y;
	}
}

wxwin_status::panel::panel(wxWindow* _parent, wxWindow* focuswin, unsigned lines)
	: text_framebuffer_panel(_parent, STATWIDTH, lines ? lines : MAXSTATUS, wxID_ANY, focuswin)
{
	watch_flag = 0;
}

wxwin_status::wxwin_status(int flag, const std::string& title)
	: wxFrame(NULL, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN)
{
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	top_s->Add(spanel = new wxwin_status::panel(this, NULL, MAXSTATUS), 1, wxGROW);
	spanel->set_watch_flag(flag);
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wxwin_status::~wxwin_status()
{
}

void wxwin_status::panel::prepare_paint()
{
	//Quickly copy the status area.
	auto& s = platform::get_emustatus();
	std::map<std::string, std::u32string> newstatus;
	emulator_status::iterator i = s.first();
	while(s.next(i))
		newstatus[i.key] = i.value;

	clear();

	size_t mem_width = 0;
	size_t oth_width = 0;
	size_t mem_count = 0;
	size_t oth_count = 0;
	bool single = false;
	for(auto i : newstatus) {
		if(i.first.length() > 0 && i.first[0] == '!')
			continue;
		bool x = regex_match("M\\[.*\\]", i.first);
		if(x) {
			mem_width = max(mem_width, text_framebuffer::text_width(i.first) - 3);
			mem_count++;
		} else {
			oth_width = max(oth_width, text_framebuffer::text_width(i.first));
			oth_count++;
		}
	}
	if(watch_flag < 0)
		oth_count = 0;
	if(watch_flag > 0)
		mem_count = 0;
	if(!oth_count || !mem_count)
		single = true;

	regex_results r;
	size_t p = 0;
	if(mem_count) {
		if(!single)
			write("Memory watches:", 0, 0, p++, 0, 0xFFFFFF);
		for(auto i : newstatus) {
			if(i.first.length() > 0 && i.first[0] == '!')
				continue;
			if(r = regex("M\\[(.*)\\]", i.first)) {
				size_t n = write(r[1], mem_width + 1, 0, p, 0, 0xFFFFFF);
				write(i.second, 0, n, p++, 0, 0xFFFFFF);
			}
		}
	}
	if(mem_count && oth_count) {
		auto s = get_characters();
		for(unsigned i = 0; i < s.first; i++)
			write(U"\u2500", 0, i, p, 0, 0xFFFFFF);
		p++;
	}

	if(oth_count) {
		if(!single)
			write("Status:", 0, 0, p++, 0, 0xFFFFFF);
		for(auto i : newstatus) {
			if(i.first.length() > 0 && i.first[0] == '!')
				continue;
			if(regex_match("M\\[.*\\]", i.first))
				continue;
			size_t n = write(i.first, oth_width + 1, 0, p, 0, 0xFFFFFF);
			write(i.second, 0, n, p++, 0, 0xFFFFFF);
		}
	}

	{
		std::map<std::string, std::u32string> specials;
		for(auto i : newstatus)
			if(i.first.length() > 0 && i.first[0] == '!')
				specials[i.first] = i.second;
		main_window->update_statusbar(specials);
	}

}

void wxwin_status::notify_update() throw()
{
	spanel->request_paint();
}

bool wxwin_status::ShouldPreventAppExit() const
{
	return false;
}
