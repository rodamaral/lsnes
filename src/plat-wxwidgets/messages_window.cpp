#include "core/window.hpp"

#include "plat-wxwidgets/emufn.hpp"
#include "plat-wxwidgets/messages_window.hpp"


#define MAXMESSAGES 20

class wx_message_panel : public wxPanel
{
public:
	wx_message_panel(unsigned lines);
	void on_paint(wxPaintEvent& e);
};

wx_messages_window* wx_messages_window::ptr;

wx_message_panel::wx_message_panel(unsigned lines)
	: wxPanel(wx_messages_window::ptr)
{
	wxMemoryDC d;
	wxSize s = d.GetTextExtent(wxT("MMMMMM"));
	SetMinSize(wxSize(12 * s.x, lines * s.y));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wx_message_panel::on_paint), NULL, this);
}


namespace {
	wx_message_panel* mpanel;
}

wx_messages_window::wx_messages_window()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Messages"), wxDefaultPosition, wxSize(-1, -1), secondary_window_style)
{
	ptr = this;
	wxFlexGridSizer* top_s = new wxFlexGridSizer(3, 1, 0, 0);
	top_s->Add(mpanel = new wx_message_panel(MAXMESSAGES));
	window::msgbuf.set_max_window_size(MAXMESSAGES);

	wxFlexGridSizer* buttons_s = new wxFlexGridSizer(1, 6, 0, 0);
	wxButton* beginning, * pageup, * lineup, * linedown, * pagedown, * end;
	buttons_s->Add(beginning = new wxButton(this, wxID_ANY, wxT("Beginning")), 1, wxGROW);
	buttons_s->Add(pageup = new wxButton(this, wxID_ANY, wxT("Page Up")), 1, wxGROW);
	buttons_s->Add(lineup = new wxButton(this, wxID_ANY, wxT("Line Up")), 1, wxGROW);
	buttons_s->Add(linedown = new wxButton(this, wxID_ANY, wxT("Line Down")), 1, wxGROW);
	buttons_s->Add(pagedown = new wxButton(this, wxID_ANY, wxT("Page Down")), 1, wxGROW);
	buttons_s->Add(end = new wxButton(this, wxID_ANY, wxT("End")), 1, wxGROW);
	beginning->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_scroll_home),
		NULL, this);
	pageup->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_scroll_pageup),
		NULL, this);
	lineup->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_scroll_lineup),
		NULL, this);
	linedown->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_scroll_linedown),
		NULL, this);
	pagedown->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_scroll_pagedown),
		NULL, this);
	end->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_scroll_end),
		NULL, this);
	top_s->Add(buttons_s, 0, wxGROW);

	wxBoxSizer* cmd_s = new wxBoxSizer(wxHORIZONTAL);
	wxButton* execute;
	cmd_s->Add(command = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_PROCESS_ENTER), 1, wxEXPAND);
	cmd_s->Add(execute = new wxButton(this, wxID_ANY, wxT("Execute")), 0, wxGROW);
	command->Connect(wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(wx_messages_window::on_execute),
		NULL, this);
	execute->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wx_messages_window::on_execute),
		NULL, this);
	cmd_s->SetSizeHints(this);
	top_s->Add(cmd_s, 0, wxGROW);

	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wx_messages_window::~wx_messages_window()
{
	ptr = NULL;
}

void wx_message_panel::on_paint(wxPaintEvent& e)
{
	wxPaintDC dc(this);
	dc.Clear();
	int y = 0;
	size_t lines = window::msgbuf.get_visible_count();
	size_t first = window::msgbuf.get_visible_first();
	for(size_t i = 0; i < lines; i++) {
		wxSize s = dc.GetTextExtent(towxstring(window::msgbuf.get_message(first + i)));
		dc.DrawText(towxstring(window::msgbuf.get_message(first + i)), 0, y);
		y += s.y;
	}
}

void wx_messages_window::on_scroll_home(wxCommandEvent& e)
{
	window::msgbuf.scroll_beginning();
}

void wx_messages_window::on_scroll_pageup(wxCommandEvent& e)
{
	window::msgbuf.scroll_up_page();
}

void wx_messages_window::on_scroll_lineup(wxCommandEvent& e)
{
	window::msgbuf.scroll_up_line();
}

void wx_messages_window::on_scroll_linedown(wxCommandEvent& e)
{
	window::msgbuf.scroll_down_line();
}

void wx_messages_window::on_scroll_pagedown(wxCommandEvent& e)
{
	window::msgbuf.scroll_down_page();
}

void wx_messages_window::on_scroll_end(wxCommandEvent& e)
{
	window::msgbuf.scroll_end();
}

void wx_messages_window::on_execute(wxCommandEvent& e)
{
	std::string cmd = tostdstring(command->GetValue());
	if(cmd == "")
		return;
	exec_command(cmd);
}

void wx_messages_window::notify_message()
{
	mpanel->Refresh();
}

bool wx_messages_window::ShouldPreventAppExit() const
{
	return false;
}
