#include "core/dispatch.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "library/string.hpp"
#include "library/memorysearch.hpp"

#include "platform/wxwidgets/platform.hpp"

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

#define CANDIDATE_LIMIT 200

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

	typedef void (memory_search::*primitive_search_t)();
	
	primitive_search_t primitive_searches[DATATYPES][PRIMITIVES] = {
		{ &memory_search::byte_slt, &memory_search::byte_sle, &memory_search::byte_seq,
		&memory_search::byte_sne, &memory_search::byte_sge, &memory_search::byte_sgt,
		&memory_search::byte_seqlt, &memory_search::byte_seqle, &memory_search::byte_seqge,
		&memory_search::byte_seqgt, &memory_search::update },
		{ &memory_search::byte_ult, &memory_search::byte_ule, &memory_search::byte_ueq,
		&memory_search::byte_une, &memory_search::byte_uge, &memory_search::byte_ugt,
		&memory_search::byte_seqlt, &memory_search::byte_seqle, &memory_search::byte_seqge,
		&memory_search::byte_seqgt, &memory_search::update },
		{ &memory_search::word_slt, &memory_search::word_sle, &memory_search::word_seq,
		&memory_search::word_sne, &memory_search::word_sge, &memory_search::word_sgt,
		&memory_search::word_seqlt, &memory_search::word_seqle, &memory_search::word_seqge,
		&memory_search::word_seqgt, &memory_search::update },
		{ &memory_search::word_ult, &memory_search::word_ule, &memory_search::word_ueq,
		&memory_search::word_une, &memory_search::word_uge, &memory_search::word_ugt,
		&memory_search::word_seqlt, &memory_search::word_seqle, &memory_search::word_seqge,
		&memory_search::word_seqgt, &memory_search::update },
		{ &memory_search::dword_slt, &memory_search::dword_sle, &memory_search::dword_seq,
		&memory_search::dword_sne, &memory_search::dword_sge, &memory_search::dword_sgt,
		&memory_search::dword_seqlt, &memory_search::dword_seqle, &memory_search::dword_seqge,
		&memory_search::dword_seqgt, &memory_search::update },
		{ &memory_search::dword_ult, &memory_search::dword_ule, &memory_search::dword_ueq,
		&memory_search::dword_une, &memory_search::dword_uge, &memory_search::dword_ugt,
		&memory_search::dword_seqlt, &memory_search::dword_seqle, &memory_search::dword_seqge,
		&memory_search::dword_seqgt, &memory_search::update },
		{ &memory_search::qword_slt, &memory_search::qword_sle, &memory_search::qword_seq,
		&memory_search::qword_sne, &memory_search::qword_sge, &memory_search::qword_sgt,
		&memory_search::qword_seqlt, &memory_search::qword_seqle, &memory_search::qword_seqge,
		&memory_search::qword_seqgt, &memory_search::update },
		{ &memory_search::qword_ult, &memory_search::qword_ule, &memory_search::qword_ueq,
		&memory_search::qword_une, &memory_search::qword_uge, &memory_search::qword_ugt,
		&memory_search::qword_seqlt, &memory_search::qword_seqle, &memory_search::qword_seqge,
		&memory_search::qword_seqgt, &memory_search::update }
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
	auto i = lsnes_memory.get_regions();
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(i.size() + 1, 1, 0, 0);
	SetSizer(top_s);
	for(auto j : i) {
		if(j->readonly || j->special)
			continue;
		wxCheckBox* t;
		top_s->Add(t = new wxCheckBox(this, wxID_ANY, towxstring(j->name)), 0, wxGROW);
		if(enabled.count(j->name))
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
	wxwindow_memorysearch();
	~wxwindow_memorysearch();
	bool ShouldPreventAppExit() const;
	void on_close(wxCloseEvent& e);
	void on_button_click(wxCommandEvent& e);
	void auto_update();
	void on_mouse(wxMouseEvent& e);
	bool update_queued;
private:
	template<typename T> void valuesearch(bool diff);
	template<typename T> void valuesearch2(T value);
	template<typename T> void valuesearch3(T value);
	void update();
	memory_search* msearch;
	void on_mouse2();
	wxStaticText* count;
	wxTextCtrl* matches;
	wxComboBox* type;
	wxCheckBox* hexmode2;
	wxCheckBox* autoupdate;
	std::map<long, uint64_t> addresses;
	unsigned typecode;
	bool hexmode;
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
	msearch = new memory_search(lsnes_memory);

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

	toplevel->Add(count = new wxStaticText(this, wxID_ANY, wxT("XXX candidates")), 0, wxGROW);
	toplevel->Add(matches = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, 300),
		wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxTE_NOHIDESEL), 1, wxGROW);

	for(auto i : lsnes_memory.get_regions())
		if(memory_search::searchable_region(i))
			vmas_enabled.insert(i->name);

	//matches->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	//matches->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	//matches->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	//matches->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	matches->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);
	//matches->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(wxwindow_memorysearch::on_mouse), NULL, this);

	toplevel->SetSizeHints(this);
	Fit();
	update();
	Fit();
	hexmode = false;
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
		on_mouse2();
}

void wxwindow_memorysearch::on_mouse2()
{
	wxMenu menu;
	bool some_selected;
	long start, end;
	matches->GetSelection(&start, &end);
	some_selected = (start < end);
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
	std::string ret;
	uint64_t addr_count;
	runemufn([this, &ret, &addr_count]() {
		addr_count = this->msearch->get_candidate_count();
		if(addr_count <= CANDIDATE_LIMIT) {
			this->addresses.clear();
			std::list<uint64_t> addrs = this->msearch->get_candidates();
			long j = 0;
			for(auto i : addrs) {
				std::ostringstream row;
				row << hexformat_address(i) << " ";
				switch(this->typecode) {
				case 0:
					row << format_number_signed(lsnes_memory.read<uint8_t>(i), this->hexmode);
		 			break;
				case 1:
					row << format_number_unsigned(lsnes_memory.read<uint8_t>(i), this->hexmode);
					break;
				case 2:
					row << format_number_signed(lsnes_memory.read<uint16_t>(i), this->hexmode);
					break;
				case 3:
					row << format_number_unsigned(lsnes_memory.read<uint16_t>(i), this->hexmode);
					break;
				case 4:
					row << format_number_signed(lsnes_memory.read<uint32_t>(i), this->hexmode);
					break;
				case 5:
					row << format_number_unsigned(lsnes_memory.read<uint32_t>(i), this->hexmode);
					break;
				case 6:
					row << format_number_signed(lsnes_memory.read<uint64_t>(i), this->hexmode);
					break;
				case 7:
					row << format_number_unsigned(lsnes_memory.read<uint64_t>(i), this->hexmode);
					break;
				};
				row << std::endl;
				ret = ret + row.str();
				this->addresses[j++] = i;
			}
		} else {
			ret = "Too many candidates to display";
			this->addresses.clear();
		}
	});
	std::ostringstream x;
	x << addr_count << " " << ((addr_count != 1) ? "candidates" : "candidate");
	count->SetLabel(towxstring(x.str()));
	matches->SetValue(towxstring(ret));
	Fit();
	update_queued = false;
}

void wxwindow_memorysearch::on_button_click(wxCommandEvent& e)
{
	int id = e.GetId();
	if(id == wxID_RESET) {
		msearch->reset();
		for(auto i : lsnes_memory.get_regions())
			if(memory_search::searchable_region(i) && !vmas_enabled.count(i->name))
				msearch->dq_range(i->base, i->last_address());
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
		long start, end;
		long startx, starty, endx, endy;
		matches->GetSelection(&start, &end);
		if(start == end)
			return;
		if(!matches->PositionToXY(start, &startx, &starty))
			return;
		if(!matches->PositionToXY(end, &endx, &endy))
			return;
		if(endx == 0 && endy != 0)
			endy--;
		char wch = watchchars[typecode];
		for(long r = starty; r <= endy; r++) {
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
	} else if(id == wxID_DISQUALIFY) {
		long start, end;
		long startx, starty, endx, endy;
		matches->GetSelection(&start, &end);
		if(start == end)
			return;
		if(!matches->PositionToXY(start, &startx, &starty))
			return;
		if(!matches->PositionToXY(end, &endx, &endy))
			return;
		if(endx == 0 && endy != 0)
			endy--;
		for(long r = starty; r <= endy; r++) {
			if(!addresses.count(r))
				return;
			uint64_t addr = addresses[r];
			auto ms = msearch;
			runemufn([addr, ms]() { ms->dq_range(addr, addr); });
		}
	} else if(id == wxID_SET_REGIONS) {
		wxwindow_memorysearch_vmasel* d = new wxwindow_memorysearch_vmasel(this, vmas_enabled);
		if(d->ShowModal() == wxID_OK)
			vmas_enabled = d->get_vmas();
		d->Destroy();
		for(auto i : lsnes_memory.get_regions())
			if(memory_search::searchable_region(i) && !vmas_enabled.count(i->name))
				msearch->dq_range(i->base, i->last_address());
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
