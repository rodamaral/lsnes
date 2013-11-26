#include "platform/wxwidgets/window_messages.hpp"
#include "platform/wxwidgets/platform.hpp"

#include "core/window.hpp"

#define MAXMESSAGES 20
#define COMMAND_HISTORY_SIZE 500

wxwin_messages::panel::panel(wxwin_messages* _parent, unsigned lines)
	: wxPanel(_parent)
{
	parent = _parent;
	wxMemoryDC d;
	wxSize s = d.GetTextExtent(wxT("MMMMMM"));
	line_separation = s.y;
	ilines = lines;
	//wxSizer* dummy_pad = new wxBoxSizer(wxVERTICAL);
	//SetSizer(dummy_pad);
	//dummy_pad->Add(12 * s.x, lines * s.y);
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wxwin_messages::panel::on_paint), NULL, this);
	this->Connect(wxEVT_SIZE, wxSizeEventHandler(wxwin_messages::panel::on_resize), NULL, this);
	//Fit();
	SetMinSize(wxSize(6 * s.x, 5 * s.y));
}

wxSize wxwin_messages::panel::DoGetBestSize() const
{
	wxMemoryDC d;
	wxSize s = d.GetTextExtent(wxT("MMMMMM"));
	std::cerr << "Requesting " << 12 * s.x << "*" << ilines * s.y << std::endl;
	return wxSize(12 * s.x, ilines * s.y);
}

wxwin_messages::wxwin_messages()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Messages"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxCLOSE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN |
		wxRESIZE_BORDER)
{
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);
	top_s->Add(mpanel = new panel(this, MAXMESSAGES), 1, wxEXPAND);
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
	cmd_s->Add(command = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		0, NULL, wxTE_PROCESS_ENTER), 1, wxEXPAND);
	cmd_s->Add(execute = new wxButton(this, wxID_ANY, wxT("Execute")), 0, wxGROW);
	command->Connect(wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(wxwin_messages::on_execute),
		NULL, this);
	execute->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_messages::on_execute),
		NULL, this);
	cmd_s->SetSizeHints(this);
	top_s->Add(cmd_s, 0, wxGROW);

	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_messages::on_close));
	//Very nasty hack.
	wxSize tmp = mpanel->GetMinSize();
	mpanel->SetMinSize(mpanel->DoGetBestSize());
	top_s->SetSizeHints(this);
	wxSize tmp2 = GetClientSize();
	mpanel->SetMinSize(tmp);
	top_s->SetSizeHints(this);
	SetClientSize(tmp2);
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
		umutex_class h(platform::msgbuf_lock());
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

void wxwin_messages::panel::on_resize(wxSizeEvent& e)
{
	wxSize newsize = e.GetSize();
	size_t lines = newsize.y / line_separation;
	if(lines < 1) lines = 1;
	platform::msgbuf.set_max_window_size(lines);
	Refresh();
	e.Skip();
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
	umutex_class h(platform::msgbuf_lock());
	platform::msgbuf.scroll_beginning();
}

void wxwin_messages::on_scroll_pageup(wxCommandEvent& e)
{
	umutex_class h(platform::msgbuf_lock());
	platform::msgbuf.scroll_up_page();
}

void wxwin_messages::on_scroll_lineup(wxCommandEvent& e)
{
	umutex_class h(platform::msgbuf_lock());
	platform::msgbuf.scroll_up_line();
}

void wxwin_messages::on_scroll_linedown(wxCommandEvent& e)
{
	umutex_class h(platform::msgbuf_lock());
	platform::msgbuf.scroll_down_line();
}

void wxwin_messages::on_scroll_pagedown(wxCommandEvent& e)
{
	umutex_class h(platform::msgbuf_lock());
	platform::msgbuf.scroll_down_page();
}

void wxwin_messages::on_scroll_end(wxCommandEvent& e)
{
	umutex_class h(platform::msgbuf_lock());
	platform::msgbuf.scroll_end();
}

void wxwin_messages::on_execute(wxCommandEvent& e)
{
	std::string cmd = tostdstring(command->GetValue());
	if(cmd == "")
		return;
	command->Insert(towxstring(cmd), 0);
	//If command is already there, delete the previous.
	for(unsigned i = 1; i < command->GetCount(); i++)
		if(tostdstring(command->GetString(i)) == cmd) {
			command->Delete(i);
			break;
		}
	//Delete old commands to prevent box becoming unmageable.
	if(command->GetCount() > COMMAND_HISTORY_SIZE)
		command->Delete(command->GetCount() - 1);
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
