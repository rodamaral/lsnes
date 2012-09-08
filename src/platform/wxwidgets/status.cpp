#include "core/window.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_status.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"

#define MAXSTATUS 30

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
	: wxPanel(_parent)
{
	tfocuswin = focuswin;
	parent = _parent;
	dirty = false;
	wxMemoryDC d;
	wxFont sysfont = wxSystemSettings::GetFont(wxSYS_OEM_FIXED_FONT);
	wxFont tsysfont(sysfont.GetPointSize(), wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	d.SetFont(tsysfont);
	wxSize s = d.GetTextExtent(wxT("MM"));
	SetMinSize(wxSize(20 * s.x, lines * s.y));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wxwin_status::panel::on_paint), NULL, this);
	this->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxwin_status::panel::on_focus), NULL, this);
}

bool wxwin_status::panel::AcceptsFocus () const
{
	return false;
}

void wxwin_status::panel::on_focus(wxFocusEvent& e)
{
	if(tfocuswin)
		tfocuswin->SetFocus();
}

wxwin_status::wxwin_status()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Status"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN)
{
	wxFlexGridSizer* top_s = new wxFlexGridSizer(1, 1, 0, 0);
	top_s->Add(spanel = new wxwin_status::panel(this, NULL, MAXSTATUS));
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wxwin_status::~wxwin_status()
{
}

void wxwin_status::panel::on_paint(wxPaintEvent& e)
{
	//Quickly copy the status area.
	auto& s = platform::get_emustatus();
	std::map<std::string, std::string> newstatus;
	emulator_status::iterator i = s.first();
	while(s.next(i))
		newstatus[i.key] = i.value;

	wxPaintDC dc(this);
	dc.Clear();
	wxFont sysfont = wxSystemSettings::GetFont(wxSYS_OEM_FIXED_FONT);
	wxFont tsysfont(sysfont.GetPointSize(), wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	dc.SetFont(tsysfont);
	int y = 0;
	bool has_watches = false;
	size_t mem_width = 0;
	size_t oth_width = 0;
	for(auto i : newstatus) {
		bool x = regex_match("M\\[.*\\]", i.first);
		if(x)
			mem_width = max(mem_width, i.first.length() - 3);
		else
			oth_width = max(oth_width, i.first.length());
		has_watches |= x;
	}
	regex_results r;
	if(has_watches) {
		std::string pstr = "Memory watches:";
		wxSize s = dc.GetTextExtent(towxstring(pstr));
		dc.DrawText(towxstring(pstr), 0, y);
		y += s.y;
		for(auto i : newstatus) {
			if(r = regex("M\\[(.*)\\]", i.first)) {
				pstr = string_pad(r[1] + ": ", mem_width + 2) + i.second;
				wxSize s = dc.GetTextExtent(towxstring(pstr));
				dc.DrawText(towxstring(pstr), 0, y);
				y += s.y;
			}
		}
		s = dc.GetSize();
		dc.SetPen(wxPen(wxColour(0, 0, 0)));
		dc.DrawLine(0, y + 1, s.x, y + 1);
		y += 3;
		pstr = "Other status:";
		s = dc.GetTextExtent(towxstring(pstr));
		dc.DrawText(towxstring(pstr), 0, y);
		y += s.y;
	}
	for(auto i : newstatus) {
		if(regex_match("M\\[.*\\]", i.first))
			continue;
		std::string pstr = string_pad(i.first + ": ", oth_width + 2) + i.second;
		wxSize s = dc.GetTextExtent(towxstring(pstr));
		dc.DrawText(towxstring(pstr), 0, y);
		y += s.y;
	}
	dirty = false;
}

void wxwin_status::notify_update() throw()
{
	if(spanel->dirty)
		return;
	spanel->dirty = true;
	spanel->Refresh();
}

bool wxwin_status::ShouldPreventAppExit() const
{
	return false;
}
