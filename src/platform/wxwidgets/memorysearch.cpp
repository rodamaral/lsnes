#include "core/dispatch.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "library/string.hpp"

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/scrollbar.hpp"
#include "platform/wxwidgets/textrender.hpp"

#include <sstream>
#include <iomanip>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#define wxID_RESET (wxID_HIGHEST + 1)
#define wxID_UPDATE (wxID_HIGHEST + 2)
#define wxID_TYPESELECT (wxID_HIGHEST + 3)
#define wxID_HEX_SELECT (wxID_HIGHEST + 4)
#define wxID_ADD (wxID_HIGHEST + 5)
#define wxID_SET_REGIONS (wxID_HIGHEST + 6)
#define wxID_AUTOUPDATE (wxID_HIGHEST + 7)
#define wxID_DISQUALIFY (wxID_HIGHEST + 8)
#define wxID_BUTTONS_BASE (wxID_HIGHEST + 128)

#define DATATYPES 8
#define BROW_SIZE 13
#define PRIMITIVES 12

#define CANDIDATE_LIMIT 512

class wxwindow_memorysearch;

namespace
{
	const char* watchchars = "bBwWdDqQ";

	wxwindow_memorysearch* mwatch;
	const char* datatypes[] = {
		"signed byte",
		"unsigned byte",
		"signed word",
		"unsigned word",
		"signed dword",
		"unsigned dword",
		"signed qword",
		"unsigned qword"
	};

	const char* searchtypes[] = {
		"value",
		"diff.",
		"<",
		"<=",
		"==",
		"!=",
		">=",
		">",
		"seq<",
		"seq<=",
		"seq>=",
		"seq>",
		"true"
	};

	typedef void (memorysearch::*primitive_search_t)();
	
	primitive_search_t primitive_searches[DATATYPES][PRIMITIVES] = {
		{ &memorysearch::byte_slt, &memorysearch::byte_sle, &memorysearch::byte_seq, &memorysearch::byte_sne,
		&memorysearch::byte_sge, &memorysearch::byte_sgt, &memorysearch::byte_seqlt,
		&memorysearch::byte_seqle, &memorysearch::byte_seqge, &memorysearch::byte_seqgt,
		&memorysearch::update },
		{ &memorysearch::byte_ult, &memorysearch::byte_ule, &memorysearch::byte_ueq, &memorysearch::byte_une,
		&memorysearch::byte_uge, &memorysearch::byte_ugt, &memorysearch::byte_seqlt,
		&memorysearch::byte_seqle, &memorysearch::byte_seqge, &memorysearch::byte_seqgt,
		&memorysearch::update },
		{ &memorysearch::word_slt, &memorysearch::word_sle, &memorysearch::word_seq, &memorysearch::word_sne,
		&memorysearch::word_sge, &memorysearch::word_sgt, &memorysearch::word_seqlt,
		&memorysearch::word_seqle, &memorysearch::word_seqge, &memorysearch::word_seqgt,
		&memorysearch::update },
		{ &memorysearch::word_ult, &memorysearch::word_ule, &memorysearch::word_ueq, &memorysearch::word_une,
		&memorysearch::word_uge, &memorysearch::word_ugt, &memorysearch::word_seqlt,
		&memorysearch::word_seqle, &memorysearch::word_seqge, &memorysearch::word_seqgt,
		&memorysearch::update },
		{ &memorysearch::dword_slt, &memorysearch::dword_sle, &memorysearch::dword_seq,
		&memorysearch::dword_sne, &memorysearch::dword_sge, &memorysearch::dword_sgt,
		&memorysearch::dword_seqlt, &memorysearch::dword_seqle, &memorysearch::dword_seqge,
		&memorysearch::dword_seqgt, &memorysearch::update },
		{ &memorysearch::dword_ult, &memorysearch::dword_ule, &memorysearch::dword_ueq,
		&memorysearch::dword_une, &memorysearch::dword_uge, &memorysearch::dword_ugt,
		&memorysearch::dword_seqlt, &memorysearch::dword_seqle, &memorysearch::dword_seqge,
		&memorysearch::dword_seqgt, &memorysearch::update },
		{ &memorysearch::qword_slt, &memorysearch::qword_sle, &memorysearch::qword_seq,
		&memorysearch::qword_sne, &memorysearch::qword_sge, &memorysearch::qword_sgt,
		&memorysearch::qword_seqlt, &memorysearch::qword_seqle, &memorysearch::qword_seqge,
		&memorysearch::qword_seqgt, &memorysearch::update },
		{ &memorysearch::qword_ult, &memorysearch::qword_ule, &memorysearch::qword_ueq,
		&memorysearch::qword_une, &memorysearch::qword_uge, &memorysearch::qword_ugt,
		&memorysearch::qword_seqlt, &memorysearch::qword_seqle, &memorysearch::qword_seqge,
		&memorysearch::qword_seqgt, &memorysearch::update }
	};

	std::string hexformat_address(uint64_t addr)
	{
		std::ostringstream x;
		x << std::setfill('0') << std::setw(16) << std::hex << addr;
		return x.str();
	}

	template<typename T> std::string format_number_signed(T val, bool hex);
	template<typename T> std::string format_number_unsigned(T val, bool hex);

	template<typename T> std::string format_number_signedh(T val, unsigned hwidth, bool hex)
	{
		std::ostringstream x;
		if(hex) {
			if(val >= 0)
				x << "+" << std::setfill('0') << std::setw(hwidth) << std::hex <<
					static_cast<uint64_t>(val);
			else {
				int64_t y2 = val;
				uint64_t y = static_cast<uint64_t>(y2);
				x << "-" << std::setfill('0') << std::setw(hwidth) << std::hex << (~y + 1);
			}
		} else
			x << static_cast<int64_t>(val);
		return x.str();
	}

	template<typename T> std::string format_number_unsignedh(T val, unsigned hwidth, bool hex)
	{
		std::ostringstream x;
		if(hex)
			x << std::setfill('0') << std::setw(hwidth) << std::hex << static_cast<uint64_t>(val);
		else
			x << static_cast<uint64_t>(val);
		return x.str();
	}

	template<> std::string format_number_signed<uint8_t>(uint8_t val, bool hex)
	{
		return format_number_signedh(static_cast<int8_t>(val), 2, hex);
	}

	template<> std::string format_number_signed<uint16_t>(uint16_t val, bool hex)
	{
		return format_number_signedh(static_cast<int16_t>(val), 4, hex);
	}

	template<> std::string format_number_signed<uint32_t>(uint32_t val, bool hex)
	{
		return format_number_signedh(static_cast<int32_t>(val), 8, hex);
	}

	template<> std::string format_number_signed<uint64_t>(uint64_t val, bool hex)
	{
		return format_number_signedh(static_cast<int64_t>(val), 16, hex);
	}

	template<> std::string format_number_unsigned<uint8_t>(uint8_t val, bool hex)
	{
		return format_number_unsignedh(val, 2, hex);
	}

	template<> std::string format_number_unsigned<uint16_t>(uint16_t val, bool hex)
	{
		return format_number_unsignedh(val, 4, hex);
	}

	template<> std::string format_number_unsigned<uint32_t>(uint32_t val, bool hex)
	{
		return format_number_unsignedh(val, 8, hex);
	}

	template<> std::string format_number_unsigned<uint64_t>(uint64_t val, bool hex)
	{
		return format_number_unsignedh(val, 16, hex);
	}
}

class wxwindow_memorysearch_vmasel : public wxDialog
{
public:
	wxwindow_memorysearch_vmasel(wxWindow* p, const std::set<std::string>& enabled);
	bool ShouldPreventAppExit() const;
	std::set<std::string> get_vmas();
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
private:
	std::set<std::string> vmas;
	std::vector<wxCheckBox*> checkboxes;
	wxButton* ok;
	wxButton* cancel;
};

wxwindow_memorysearch_vmasel::wxwindow_memorysearch_vmasel(wxWindow* p, const std::set<std::string>& enabled)
	: wxDialog(p, wxID_ANY, towxstring("lsnes: Select enabled regions"), wxDefaultPosition, wxSize(300, -1))
{
	auto i = get_regions();
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(i.size() + 1, 1, 0, 0);
	SetSizer(top_s);
	for(auto j : i) {
		if(j.readonly || j.iospace)
			continue;
		wxCheckBox* t;
		top_s->Add(t = new wxCheckBox(this, wxID_ANY, towxstring(j.region_name)), 0, wxGROW);
		if(enabled.count(j.region_name))
			t->SetValue(true);
		checkboxes.push_back(t);
	}
	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("Ok")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")), 0, wxGROW);
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwindow_memorysearch_vmasel::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwindow_memorysearch_vmasel::on_cancel), NULL, this);
	top_s->Add(pbutton_s);

	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

bool wxwindow_memorysearch_vmasel::ShouldPreventAppExit() const
{
	return false;
}

std::set<std::string> wxwindow_memorysearch_vmasel::get_vmas()
{
	return vmas;
}

void wxwindow_memorysearch_vmasel::on_ok(wxCommandEvent& e)
{
	for(auto i : checkboxes)
		if(i->GetValue())
			vmas.insert(tostdstring(i->GetLabel()));
	EndModal(wxID_OK);
}

void wxwindow_memorysearch_vmasel::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}


class wxwindow_memorysearch : public wxFrame
{
public:
	class panel : public text_framebuffer_panel
	{
	public:
		panel(wxwindow_memorysearch* parent);
		void set_selection(uint64_t first, uint64_t last);
		void get_selection(uint64_t& first, uint64_t& last);
	protected:
		void prepare_paint();
	private:
		wxwindow_memorysearch* parent;
		uint64_t first_sel;
		uint64_t last_sel;
	};
	wxwindow_memorysearch();
	~wxwindow_memorysearch();
	bool ShouldPreventAppExit() const;
	void on_close(wxCloseEvent& e);
	void on_button_click(wxCommandEvent& e);
	void auto_update();
	void on_mousedrag(wxMouseEvent& e);
	void on_mouse(wxMouseEvent& e);
	bool update_queued;
private:
	friend class panel;
	template<typename T> void valuesearch(bool diff);
	template<typename T> void valuesearch2(T value);
	template<typename T> void valuesearch3(T value);
	void update();
	void on_mouse0(wxMouseEvent& e, bool polarity);
	void on_mousedrag();
	void on_mouse2(wxMouseEvent& e);
	wxStaticText* count;
	memorysearch* msearch;
	scroll_bar* scroll;
	panel* matches;
	wxComboBox* type;
	wxCheckBox* hexmode2;
	wxCheckBox* autoupdate;
	std::map<uint64_t, uint64_t> addresses;
	uint64_t act_line;
	uint64_t drag_startline;
	bool dragging;
	int mpx, mpy;
	unsigned typecode;
	bool hexmode;
	bool toomany;
	int scroll_delta;
	std::set<std::string> vmas_enabled;
};

wxwindow_memorysearch::wxwindow_memorysearch()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Memory Search"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	typecode = 0;
	wxButton* tmp;
	Centre();
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwindow_memorysearch::on_close));
	msearch = new memorysearch();

	wxFlexGridSizer* toplevel = new wxFlexGridSizer(4, 1, 0, 0);
	SetSizer(toplevel);

	wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->Add(tmp = new wxButton(this, wxID_RESET, wxT("Reset")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwindow_memorysearch::on_button_click),
		NULL, this);
	buttons->Add(new wxStaticText(this, wxID_ANY, wxT("Data type:")), 0, wxGROW);
	wxString _datatypes[DATATYPES];
	for(size_t i = 0; i < DATATYPES; i++)
		_datatypes[i] = towxstring(datatypes[i]);
	buttons->Add(type = new wxComboBox(this, wxID_TYPESELECT, _datatypes[typecode], wxDefaultPosition,
		wxDefaultSize, DATATYPES, _datatypes, wxCB_READONLY), 0,
		wxGROW);
	type->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click), NULL, this);
	buttons->Add(hexmode2 = new wxCheckBox(this, wxID_HEX_SELECT, wxT("Hex display")), 0, wxGROW);
	buttons->Add(autoupdate = new wxCheckBox(this, wxID_AUTOUPDATE, wxT("Update automatically")), 0, wxGROW);
	autoupdate->SetValue(true);
	hexmode2->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click), NULL, this);
	toplevel->Add(buttons);

	wxFlexGridSizer* searches = new wxFlexGridSizer(2, 7, 0, 0);
	for(unsigned j = 0; j < BROW_SIZE; j++) {
		searches->Add(tmp = new wxButton(this, wxID_BUTTONS_BASE + j, towxstring(searchtypes[j])), 1, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
			wxCommandEventHandler(wxwindow_memorysearch::on_button_click), NULL, this);
	}
	toplevel->Add(searches);

	toplevel->Add(count = new wxStaticText(this, wxID_ANY, wxT("XXXXXX candidates")), 0, wxGROW);
	wxBoxSizer* matchesb = new wxBoxSizer(wxHORIZONTAL);
	matchesb->Add(matches = new panel(this), 1, wxGROW);
	matchesb->Add(scroll = new scroll_bar(this, wxID_ANY, true), 0, wxGROW);
	toplevel->Add(matchesb, 1, wxGROW);

	scroll->set_page_size(matches->get_characters().second);

	for(auto i : get_regions())
		if(!i.readonly && !i.iospace)
			vmas_enabled.insert(i.region_name);

	dragging = false;
	toomany = true;
	matches->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_MOTION, wxMouseEventHandler(wxwindow_memorysearch::on_mousedrag), NULL, this);
	matches->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	scroll->set_handler([this](scroll_bar& s) { this->matches->request_paint(); });

	toplevel->SetSizeHints(this);
	Fit();
	update();
	Fit();
	hexmode = false;
}

wxwindow_memorysearch::panel::panel(wxwindow_memorysearch* _parent)
	: text_framebuffer_panel(_parent, 40, 25, wxID_ANY, NULL)
{
	parent = _parent;
	first_sel = 0;
	last_sel = 0;
}

void wxwindow_memorysearch::panel::prepare_paint()
{
	uint64_t first = parent->scroll->get_position();
	auto ssize = get_characters();
	uint64_t last = first + ssize.second;
	std::vector<std::string> lines;
	lines.reserve(ssize.second);
	std::map<uint64_t, uint64_t> addrs;
	auto* ms = parent->msearch;
	uint64_t addr_count;
	bool toomany = false;
	auto _parent = parent;
	runemufn([&toomany, &first, &last, ms, &lines, &addrs, &addr_count, _parent]() {
		addr_count = ms->get_candidate_count();
		if(last > addr_count) {
			uint64_t delta = last - addr_count;
			if(first > delta) {
				first -= delta;
				last -= delta;
			} else {
				last -= first;
				first = 0;
			}
		}
		if(addr_count <= CANDIDATE_LIMIT) {
			std::list<uint64_t> addrs2 = ms->get_candidates();
			long j = 0;
			for(auto i : addrs2) {
				std::string row = hexformat_address(i) + " ";
				switch(_parent->typecode) {
				case 0:
					row += format_number_signed(memory_read_byte(i), _parent->hexmode);
					break;
				case 1:
					row += format_number_unsigned(memory_read_byte(i), _parent->hexmode);
					break;
				case 2:
					row += format_number_signed(memory_read_word(i), _parent->hexmode);
					break;
				case 3:
					row += format_number_unsigned(memory_read_word(i), _parent->hexmode);
					break;
				case 4:
					row += format_number_signed(memory_read_dword(i), _parent->hexmode);
					break;
				case 5:
					row += format_number_unsigned(memory_read_dword(i), _parent->hexmode);
					break;
				case 6:
					row +=  format_number_signed(memory_read_qword(i), _parent->hexmode);
					break;
				case 7:
					row += format_number_unsigned(memory_read_qword(i), _parent->hexmode);
					break;
				};
				if(j >= first && j < last)
					lines.push_back(row);
				addrs[j++] = i;
			}
		} else {
			lines.push_back("Too many candidates to display");
			toomany = true;
		}
	});
	std::swap(parent->addresses, addrs);

	std::ostringstream x;
	x << addr_count << " " << ((addr_count != 1) ? "candidates" : "candidate");
	parent->count->SetLabel(towxstring(x.str()));

	clear();
	for(unsigned i = 0; i < lines.size(); i++) {
		bool sel = (first + i) >= first_sel && (first + i) < last_sel;
		write(lines[i], 0, 0, i, sel ? 0xFFFFFF : 0, sel ? 0 : 0xFFFFFF);
	}

	if(last_sel > last)
		last_sel = last;
	if(first_sel > last)
		first_sel = last;

	parent->toomany = toomany;
	parent->scroll->set_range(toomany ? 0 : addr_count);
}

void wxwindow_memorysearch::panel::set_selection(uint64_t first, uint64_t last)
{
	first_sel = first;
	last_sel = last;
	request_paint();
}

void wxwindow_memorysearch::panel::get_selection(uint64_t& first, uint64_t& last)
{
	first = first_sel;
	last = last_sel;
}

wxwindow_memorysearch::~wxwindow_memorysearch()
{
	delete msearch;
	mwatch = NULL;
}

bool wxwindow_memorysearch::ShouldPreventAppExit() const
{
	return false;
}

void wxwindow_memorysearch::on_mouse(wxMouseEvent& e)
{
	if(e.RightUp() || (e.LeftUp() && e.ControlDown()))
		on_mouse2(e);
	else if(e.LeftDown())
		on_mouse0(e, true);
	else if(e.LeftUp())
		on_mouse0(e, false);
	unsigned speed = 1;
	if(e.ShiftDown())
		speed = 10;
	if(e.ShiftDown() && e.ControlDown())
		speed = 50;
	scroll->apply_wheel(e.GetWheelRotation(), e.GetWheelDelta(), speed);
}

void wxwindow_memorysearch::on_mouse0(wxMouseEvent& e, bool polarity)
{
	dragging = polarity && !toomany;
	if(dragging) {
		mpx = e.GetX();
		mpy = e.GetY();
		uint64_t first = scroll->get_position();
		drag_startline = first + e.GetY() / matches->get_cell().second;
	} else if(mpx == e.GetX() && mpy == e.GetY()) {
		matches->set_selection(0, 0);
	}
}

void wxwindow_memorysearch::on_mousedrag(wxMouseEvent& e)
{
	if(!dragging)
		return;
	uint64_t first = scroll->get_position();
	uint64_t linenow = first + e.GetY() / matches->get_cell().second;
	if(drag_startline < linenow)
		matches->set_selection(drag_startline, linenow + 1);
	else
		matches->set_selection(linenow, drag_startline + 1);
}

void wxwindow_memorysearch::on_mouse2(wxMouseEvent& e)
{
	wxMenu menu;
	bool some_selected;
	uint64_t start, end;
	matches->get_selection(start, end);
	some_selected = (start < end);
	if(!some_selected) {
		uint64_t first = scroll->get_position();
		act_line = first + e.GetY() / matches->get_cell().second;
		if(addresses.count(act_line))
			some_selected = true;
	}
	menu.Append(wxID_ADD, wxT("Add watch..."))->Enable(some_selected);
	menu.AppendSeparator();
	menu.Append(wxID_DISQUALIFY, wxT("Disqualify"))->Enable(some_selected);
	menu.AppendSeparator();
	menu.Append(wxID_UPDATE, wxT("Update"));
	menu.Append(wxID_SET_REGIONS, wxT("Enabled VMAs"));
	menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxwindow_memorysearch::on_button_click),
		NULL, this);
	PopupMenu(&menu);
}

void wxwindow_memorysearch::on_close(wxCloseEvent& e)
{
	Destroy();
	mwatch = NULL;
}

void wxwindow_memorysearch::auto_update()
{
	if(autoupdate->GetValue())
		update();
}

void wxwindow_memorysearch::update()
{
	matches->request_paint();
}

void wxwindow_memorysearch::on_button_click(wxCommandEvent& e)
{
	wxwindow_memorysearch* tmp = this;
	int id = e.GetId();
	if(id == wxID_RESET) {
		msearch->reset();
		for(auto i : get_regions())
			if(!i.readonly && !i.iospace && !vmas_enabled.count(i.region_name))
				msearch->dq_range(i.baseaddr, i.baseaddr + i.size - 1);
	} else if(id == wxID_UPDATE) {
		update();
	} else if(id == wxID_TYPESELECT) {
		wxString c = type->GetValue();
		for(unsigned i = 0; i < DATATYPES; i++)
			if(c == towxstring(datatypes[i]))
				typecode = i;
	} else if(id == wxID_HEX_SELECT) {
		hexmode = hexmode2->GetValue();
	} else if(id == wxID_ADD) {
		uint64_t start, end;
		matches->get_selection(start, end);
		if(start == end) {
			start = act_line;
			end = act_line + 1;
		}
		char wch = watchchars[typecode];
		for(long r = start; r < end; r++) {
			if(!addresses.count(r))
				return;
			uint64_t addr = addresses[r];
			try {
				std::string n = pick_text(this, "Name for watch", (stringfmt()
					<< "Enter name for watch at 0x" << std::hex << addr << ":").str());
				if(n == "")
					continue;
				std::string e = (stringfmt() << "C0x" << std::hex << addr << "z" << wch).str();
				runemufn([n, e]() { set_watchexpr_for(n, e); });
			} catch(canceled_exception& e) {
			}
		}
		matches->set_selection(0, 0);
	} else if(id == wxID_DISQUALIFY) {
		uint64_t start, end;
		matches->get_selection(start, end);
		if(start == end) {
			start = act_line;
			end = act_line + 1;
		}
		for(long r = start; r < end; r++) {
			if(!addresses.count(r))
				return;
			uint64_t addr = addresses[r];
			auto ms = msearch;
			runemufn([addr, ms]() { ms->dq_range(addr, addr); });
		}
		matches->set_selection(0, 0);
	} else if(id == wxID_SET_REGIONS) {
		wxwindow_memorysearch_vmasel* d = new wxwindow_memorysearch_vmasel(this, vmas_enabled);
		if(d->ShowModal() == wxID_OK)
			vmas_enabled = d->get_vmas();
		d->Destroy();
		for(auto i : get_regions())
			if(!i.readonly && !i.iospace && !vmas_enabled.count(i.region_name))
				msearch->dq_range(i.baseaddr, i.baseaddr + i.size - 1);
	} else if(id == wxID_BUTTONS_BASE || id == wxID_BUTTONS_BASE + 1) {
		//Value search.
		bool diff = (id == wxID_BUTTONS_BASE + 1);
		switch(typecode)
		{
		case 0:		valuesearch<int8_t>(diff);	break;
		case 1:		valuesearch<uint8_t>(diff);	break;
		case 2:		valuesearch<int16_t>(diff);	break;
		case 3:		valuesearch<uint16_t>(diff);	break;
		case 4:		valuesearch<int32_t>(diff);	break;
		case 5:		valuesearch<uint32_t>(diff);	break;
		case 6:		valuesearch<int64_t>(diff);	break;
		case 7:		valuesearch<uint64_t>(diff);	break;
		};
	} else if(id > wxID_BUTTONS_BASE + 1 && id < wxID_BUTTONS_BASE + 2 + PRIMITIVES ) {
		int button = id - wxID_BUTTONS_BASE - 2;
		(msearch->*(primitive_searches[typecode][button]))();
	}
	update();
}

template<typename T> void wxwindow_memorysearch::valuesearch(bool diff)
{
	std::string v;
	wxTextEntryDialog* d = new wxTextEntryDialog(this, wxT("Enter value to search for:"), wxT("Memory search"),
		wxT(""));
	if(d->ShowModal() == wxID_CANCEL)
		return;
	v = tostdstring(d->GetValue());
	d->Destroy();
	T val2;
	try {
		val2 = parse_value<T>(v);
	} catch(...) {
		wxMessageBox(towxstring("Bad value '" + v + "'"), _T("Error"), wxICON_WARNING | wxOK, this);
	}
	if(diff)
		valuesearch3(val2);
	else
		valuesearch2(val2);
	update();
}

template<> void wxwindow_memorysearch::valuesearch3<int8_t>(int8_t val)
{
	msearch->byte_difference(static_cast<uint8_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<uint8_t>(uint8_t val)
{
	msearch->byte_difference(static_cast<uint8_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<int16_t>(int16_t val)
{
	msearch->word_difference(static_cast<uint16_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<uint16_t>(uint16_t val)
{
	msearch->word_difference(static_cast<uint16_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<int32_t>(int32_t val)
{
	msearch->dword_difference(static_cast<uint32_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<uint32_t>(uint32_t val)
{
	msearch->dword_difference(static_cast<uint32_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<int64_t>(int64_t val)
{
	msearch->qword_difference(static_cast<uint64_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch3<uint64_t>(uint64_t val)
{
	msearch->qword_difference(static_cast<uint64_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<int8_t>(int8_t val)
{
	msearch->byte_value(static_cast<uint8_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<uint8_t>(uint8_t val)
{
	msearch->byte_value(static_cast<uint8_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<int16_t>(int16_t val)
{
	msearch->word_value(static_cast<uint16_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<uint16_t>(uint16_t val)
{
	msearch->word_value(static_cast<uint16_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<int32_t>(int32_t val)
{
	msearch->dword_value(static_cast<uint32_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<uint32_t>(uint32_t val)
{
	msearch->dword_value(static_cast<uint32_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<int64_t>(int64_t val)
{
	msearch->qword_value(static_cast<uint64_t>(val));
}

template<> void wxwindow_memorysearch::valuesearch2<uint64_t>(uint64_t val)
{
	msearch->qword_value(static_cast<uint64_t>(val));
}

void wxwindow_memorysearch_display()
{
	if(mwatch) {
		mwatch->Raise();
		return;
	}
	mwatch = new wxwindow_memorysearch();
	mwatch->Show();
}

void wxwindow_memorysearch_update()
{
	if(mwatch)
		mwatch->auto_update();
}
