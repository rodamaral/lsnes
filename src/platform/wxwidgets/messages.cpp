#include "core/window.hpp"

#include "platform/wxwidgets/window_messages.hpp"
#include "platform/wxwidgets/platform.hpp"

#define MAXMESSAGES 20

wxwin_messages::panel::panel(wxwin_messages* _parent, unsigned lines)
	: wxPanel(_parent)
{
	parent = _parent;
	wxMemoryDC d;
	wxSize s = d.GetTextExtent(wxT("MMMMMM"));
	SetMinSize(wxSize(12 * s.x, lines * s.y));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wxwin_messages::panel::on_paint), NULL, this);
}


wxwin_messages::wxwin_messages()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Messages"), wxDefaultPosition, wxSize(-1, -1), 
		wxMINIMIZE_BOX | wxCLOSE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN)
{
	wxFlexGridSizer* top_s = new wxFlexGridSizer(3, 1, 0, 0);
	top_s->Add(mpanel = new panel(this, MAXMESSAGES));
	platform::msgbuf.set_max_window_size(MAXMESSAGES);

	wxFlexGridSizer* buttons_s = new wxFlexGridSizer(1, 6, 0, 0);
	wxButton* beginning, * pageup, * lineup, * linedown, * pagedown, * end;
	buttons_s->Add(beginning = new wxButton(this, wxID_ANY, wxT("Beginning")), 1, wxGROW);
	buttons_s->Add(pageup = new wxButton(this, wxID_ANY, wxT("Page Up")), 1, wxGROW);
	buttons_s->Add(lineup = new wxButton(this, wxID_ANY, wxT("Line Up")), 1, wxGROW);
	buttons_s->Add(linedown = new wxButton(this, wxID_ANY, wxT("Line Down")), 1, wxGROW);
	buttons_s->Add(pagedown = new wxButton(this, wxID_ANY, wxT("Page Down")), 1, wxGROW);
	buttons_s->Add(end = new wxButton(this, wxID_ANY, wxT("End")), 1, wxGROW);
	beginning->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_scroll_home),
		NULL, this);
	pageup->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_scroll_pageup),
		NULL, this);
	lineup->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_scroll_lineup),
		NULL, this);
	linedown->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_scroll_linedown),
		NULL, this);
	pagedown->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_scroll_pagedown),
		NULL, this);
	end->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_scroll_end),
		NULL, this);
	top_s->Add(buttons_s, 0, wxGROW);

	wxBoxSizer* cmd_s = new wxBoxSizer(wxHORIZONTAL);
	wxButton* execute;
	cmd_s->Add(command = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_PROCESS_ENTER), 1, wxEXPAND);
	cmd_s->Add(execute = new wxButton(this, wxID_ANY, wxT("Execute")), 0, wxGROW);
	command->Connect(wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(wxwin_messages::on_execute),
		NULL, this);
	execute->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_execute),
		NULL, this);
	cmd_s->SetSizeHints(this);
	top_s->Add(cmd_s, 0, wxGROW);

	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_messages::on_close));
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wxwin_messages::~wxwin_messages()
{
}

void wxwin_messages::panel::on_paint(wxPaintEvent& e)
{
	wxPaintDC dc(this);
	dc.Clear();
	int y = 0;
	uint64_t lines, first;
	std::vector<std::string> msgs;
	{
		mutex::holder h(platform::msgbuf_lock());
		lines = platform::msgbuf.get_visible_count();
		first = platform::msgbuf.get_visible_first();
		msgs.resize(lines);
		for(size_t i = 0; i < lines; i++)
			msgs[i] = platform::msgbuf.get_message(first + i);
	}
	for(size_t i = 0; i < lines; i++) {
		wxSize s = dc.GetTextExtent(towxstring(msgs[i]));
		dc.DrawText(towxstring(msgs[i]), 0, y);
		y += s.y;
	}
}

void wxwin_messages::on_close(wxCloseEvent& e)
{
	if(wxwidgets_exiting)
		return;
	e.Veto();
	Hide();
}

void wxwin_messages::reshow()
{
	Show();
}

void wxwin_messages::on_scroll_home(wxCommandEvent& e)
{
	mutex::holder h(platform::msgbuf_lock());
	platform::msgbuf.scroll_beginning();
}

void wxwin_messages::on_scroll_pageup(wxCommandEvent& e)
{
	mutex::holder h(platform::msgbuf_lock());
	platform::msgbuf.scroll_up_page();
}

void wxwin_messages::on_scroll_lineup(wxCommandEvent& e)
{
	mutex::holder h(platform::msgbuf_lock());
	platform::msgbuf.scroll_up_line();
}

void wxwin_messages::on_scroll_linedown(wxCommandEvent& e)
{
	mutex::holder h(platform::msgbuf_lock());
	platform::msgbuf.scroll_down_line();
}

void wxwin_messages::on_scroll_pagedown(wxCommandEvent& e)
{
	mutex::holder h(platform::msgbuf_lock());
	platform::msgbuf.scroll_down_page();
}

void wxwin_messages::on_scroll_end(wxCommandEvent& e)
{
	mutex::holder h(platform::msgbuf_lock());
	platform::msgbuf.scroll_end();
}

void wxwin_messages::on_execute(wxCommandEvent& e)
{
	std::string cmd = tostdstring(command->GetValue());
	if(cmd == "")
		return;
	platform::queue(cmd);
}

void wxwin_messages::notify_update() throw()
{
	mpanel->Refresh();
}

bool wxwin_messages::ShouldPreventAppExit() const
{
	return false;
}
