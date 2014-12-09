#include "platform/wxwidgets/window_messages.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include <wx/clipbrd.h>

#include "library/minmax.hpp"
#include "core/instance.hpp"
#include "core/window.hpp"
#include "core/project.hpp"
#include "core/ui-services.hpp"

#define MAXMESSAGES 20
#define PANELWIDTH 48
#define COMMAND_HISTORY_SIZE 500

wxwin_messages::panel::panel(wxwin_messages* _parent, emulator_instance& _inst, unsigned lines)
	: text_framebuffer_panel(_parent, PANELWIDTH, lines, wxID_ANY, NULL), inst(_inst)
{
	CHECK_UI_THREAD;
	parent = _parent;
	auto pcell = get_cell();
	line_separation = pcell.second;
	line_clicked = 0;
	line_current = 0;
	mouse_held = false;
	ilines = lines;
	this->Connect(wxEVT_SIZE, wxSizeEventHandler(wxwin_messages::panel::on_resize), NULL, this);
	this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxwin_messages::panel::on_mouse), NULL, this);
	this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxwin_messages::panel::on_mouse), NULL, this);
	this->Connect(wxEVT_MOTION, wxMouseEventHandler(wxwin_messages::panel::on_mouse), NULL, this);
	this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(wxwin_messages::panel::on_mouse), NULL, this);

	SetMinSize(wxSize(PANELWIDTH * pcell.first, lines * pcell.second));
}

void wxwin_messages::panel::on_mouse(wxMouseEvent& e)
{
	CHECK_UI_THREAD;
	//Handle mouse wheels first.
	if(e.GetWheelRotation() && e.GetWheelDelta()) {
		int unit = e.GetWheelDelta();
		scroll_acc += e.GetWheelRotation();
		while(scroll_acc <= -unit) {
			threads::alock h(platform::msgbuf_lock());
			platform::msgbuf.scroll_down_line();
			scroll_acc += unit;
		}
		while(scroll_acc >= e.GetWheelDelta()) {
			threads::alock h(platform::msgbuf_lock());
			platform::msgbuf.scroll_up_line();
			scroll_acc -= unit;
		}
	}

	uint64_t gfirst;
	{
		threads::alock h(platform::msgbuf_lock());
		gfirst = platform::msgbuf.get_visible_first();
	}
	uint64_t local_line = e.GetY() / line_separation;
	uint64_t global_line = gfirst + local_line;
	if(e.RightDown()) {
		line_clicked = global_line;
		mouse_held = true;
		return;
	} else if(e.RightUp()) {
		line_declicked = global_line;
		mouse_held = false;
		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxwin_messages::panel::on_menu),
			NULL, this);
		menu.Append(wxID_COPY, wxT("Copy to clipboard"));
		menu.Append(wxID_SAVE, wxT("Save to file"));
		PopupMenu(&menu);
	} else {
		line_current = global_line;
	}
	Refresh();
}

void wxwin_messages::panel::on_menu(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::string str;
	uint64_t m = min(line_clicked, line_declicked);
	uint64_t M = max(line_clicked, line_declicked);
	size_t lines = 0;
	{
		threads::alock h(platform::msgbuf_lock());
		for(uint64_t i = m; i <= M; i++) {
			try {
				std::string mline = platform::msgbuf.get_message(i);
				if(lines == 1) str += "\n";
				str += mline;
				if(lines >= 1) str += "\n";
				lines++;
			} catch(...) {
			}
		}
	}
	switch(e.GetId()) {
	case wxID_COPY:
		if (wxTheClipboard->Open()) {
			wxTheClipboard->SetData(new wxTextDataObject(towxstring(str)));
			wxTheClipboard->Close();
		}
		break;
	case wxID_SAVE:
		try {
			std::string filename = choose_file_save(this, "Save messages to",
				UI_get_project_otherpath(inst), filetype_textfile);
			std::ofstream s(filename, std::ios::app);
			if(!s) throw std::runtime_error("Error opening output file");
			if(lines == 1) str += "\n";
			s << str;
			if(!s) throw std::runtime_error("Error writing output file");
		} catch(canceled_exception& e) {
		} catch(std::exception& e) {
			wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION | wxOK,
				this);
		}
		break;
	}
}

wxSize wxwin_messages::panel::DoGetBestSize() const
{
	return wxSize(80 * 8, ilines * 16);
}

wxwin_messages::wxwin_messages(emulator_instance& _inst)
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Messages"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxCLOSE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN |
		wxRESIZE_BORDER), inst(_inst)
{
	CHECK_UI_THREAD;
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);
	top_s->Add(mpanel = new panel(this, inst, MAXMESSAGES), 1, wxEXPAND);
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

void wxwin_messages::panel::prepare_paint()
{
	CHECK_UI_THREAD;
	int y = 0;
	uint64_t lines, first;
	uint64_t xm = min(line_clicked, line_current);
	uint64_t xM = max(line_clicked, line_current);
	std::vector<std::string> msgs;
	{
		threads::alock h(platform::msgbuf_lock());
		lines = platform::msgbuf.get_visible_count();
		first = platform::msgbuf.get_visible_first();
		msgs.resize(lines);
		for(size_t i = 0; i < lines; i++)
			msgs[i] = platform::msgbuf.get_message(first + i);
	}
	auto size = get_characters();
	for(size_t i = 0; i < lines; i++) {
		uint64_t global_line = first + i;
		bool sel = (global_line >= xm && global_line <= xM) && mouse_held;
		std::string& msg = msgs[i];
		uint32_t fg = sel ? 0x0000FF : 0x000000;
		uint32_t bg = sel ? 0x000000 : 0xFFFFFF;
		write(msg, size.first, 0, i, fg, bg);
		y += 1;
	}
}

void wxwin_messages::panel::on_resize(wxSizeEvent& e)
{
	CHECK_UI_THREAD;
	wxSize newsize = e.GetSize();
	auto tcell = get_cell();
	size_t lines = newsize.y / tcell.second;
	size_t linelen = newsize.x / tcell.first;
	if(lines < 1) lines = 1;
	if(linelen < 1) linelen = 1;
	platform::msgbuf.set_max_window_size(lines);
	set_size(linelen, lines);
	request_paint();
	e.Skip();
}

void wxwin_messages::on_close(wxCloseEvent& e)
{
	CHECK_UI_THREAD;
	if(wxwidgets_exiting)
		return;
	e.Veto();
	Hide();
}

void wxwin_messages::reshow()
{
	CHECK_UI_THREAD;
	Show();
}

void wxwin_messages::on_scroll_home(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	threads::alock h(platform::msgbuf_lock());
	platform::msgbuf.scroll_beginning();
}

void wxwin_messages::on_scroll_pageup(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	threads::alock h(platform::msgbuf_lock());
	platform::msgbuf.scroll_up_page();
}

void wxwin_messages::on_scroll_lineup(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	threads::alock h(platform::msgbuf_lock());
	platform::msgbuf.scroll_up_line();
}

void wxwin_messages::on_scroll_linedown(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	threads::alock h(platform::msgbuf_lock());
	platform::msgbuf.scroll_down_line();
}

void wxwin_messages::on_scroll_pagedown(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	threads::alock h(platform::msgbuf_lock());
	platform::msgbuf.scroll_down_page();
}

void wxwin_messages::on_scroll_end(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	threads::alock h(platform::msgbuf_lock());
	platform::msgbuf.scroll_end();
}

void wxwin_messages::on_execute(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
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
	inst.iqueue->queue(cmd);
}

void wxwin_messages::notify_update() throw()
{
	mpanel->request_paint();
}

bool wxwin_messages::ShouldPreventAppExit() const
{
	return false;
}
