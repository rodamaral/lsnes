#include "core/dispatch.hpp"
#include "core/memorymanip.hpp"

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
#define wxID_BUTTONS_BASE (wxID_HIGHEST + 128)

#define DATATYPES 8
#define BROW_SIZE 8
#define PRIMITIVES 7

#define CANDIDATE_LIMIT 200

class wxwindow_memorysearch;
namespace
{
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
		"<",
		"<=",
		"==",
		"!=",
		">=",
		">",
		"true"
	};

	typedef void (memorysearch::*primitive_search_t)();
	
	primitive_search_t primitive_searches[DATATYPES][PRIMITIVES] = {
		{ &memorysearch::byte_slt, &memorysearch::byte_sle, &memorysearch::byte_seq, &memorysearch::byte_sne,
		&memorysearch::byte_sge, &memorysearch::byte_sgt, &memorysearch::update },
		{ &memorysearch::byte_ult, &memorysearch::byte_ule, &memorysearch::byte_ueq, &memorysearch::byte_une,
		&memorysearch::byte_uge, &memorysearch::byte_ugt, &memorysearch::update },
		{ &memorysearch::word_slt, &memorysearch::word_sle, &memorysearch::word_seq, &memorysearch::word_sne,
		&memorysearch::word_sge, &memorysearch::word_sgt, &memorysearch::update },
		{ &memorysearch::word_ult, &memorysearch::word_ule, &memorysearch::word_ueq, &memorysearch::word_une,
		&memorysearch::word_uge, &memorysearch::word_ugt, &memorysearch::update },
		{ &memorysearch::dword_slt, &memorysearch::dword_sle, &memorysearch::dword_seq,
		&memorysearch::dword_sne, &memorysearch::dword_sge, &memorysearch::dword_sgt, &memorysearch::update },
		{ &memorysearch::dword_ult, &memorysearch::dword_ule, &memorysearch::dword_ueq,
		&memorysearch::dword_une, &memorysearch::dword_uge, &memorysearch::dword_ugt, &memorysearch::update },
		{ &memorysearch::qword_slt, &memorysearch::qword_sle, &memorysearch::qword_seq,
		&memorysearch::qword_sne, &memorysearch::qword_sge, &memorysearch::qword_sgt, &memorysearch::update },
		{ &memorysearch::qword_ult, &memorysearch::qword_ule, &memorysearch::qword_ueq,
		&memorysearch::qword_une, &memorysearch::qword_uge, &memorysearch::qword_ugt, &memorysearch::update }
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

class wxwindow_memorysearch : public wxFrame
{
public:
	wxwindow_memorysearch();
	~wxwindow_memorysearch();
	bool ShouldPreventAppExit() const;
	void on_close(wxCloseEvent& e);
	void on_button_click(wxCommandEvent& e);
	bool update_queued;
private:
	template<typename T> void valuesearch();
	template<typename T> void valuesearch2(T value);
	void update();
	memorysearch* msearch;
	wxStaticText* count;
	wxTextCtrl* matches;
	wxComboBox* type;
	wxCheckBox* hexmode2;
	unsigned typecode;
	bool hexmode;
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

	wxFlexGridSizer* buttons = new wxFlexGridSizer(1, 5, 0, 0);
	buttons->Add(tmp = new wxButton(this, wxID_RESET, wxT("Reset")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwindow_memorysearch::on_button_click),
		NULL, this);
	buttons->Add(tmp = new wxButton(this, wxID_UPDATE, wxT("Update")), 0, wxGROW);
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
	hexmode2->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click), NULL, this);
	toplevel->Add(buttons);

	wxFlexGridSizer* searches = new wxFlexGridSizer(1, BROW_SIZE, 0, 0);
	for(unsigned j = 0; j < BROW_SIZE; j++) {
		searches->Add(tmp = new wxButton(this, wxID_BUTTONS_BASE + j, towxstring(searchtypes[j])), 1, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
			wxCommandEventHandler(wxwindow_memorysearch::on_button_click), NULL, this);
	}
	toplevel->Add(searches);

	toplevel->Add(count = new wxStaticText(this, wxID_ANY, wxT("XXX candidates")), 0, wxGROW);
	toplevel->Add(matches = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, 300),
		wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP), 1, wxGROW);

	toplevel->SetSizeHints(this);
	Fit();
	update();
	Fit();
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

void wxwindow_memorysearch::on_close(wxCloseEvent& e)
{
	Destroy();
	mwatch = NULL;
}

void wxwindow_memorysearch::update()
{
	std::string ret;
	uint64_t addr_count;
	runemufn([msearch, &ret, &addr_count, typecode, hexmode]() {
		addr_count = msearch->get_candidate_count();
		if(addr_count <= CANDIDATE_LIMIT) {
			std::list<uint64_t> addrs = msearch->get_candidates();
			for(auto i : addrs) {
				std::ostringstream row;
				row << hexformat_address(i) << " ";
				switch(typecode) {
				case 0:		row << format_number_signed(memory_read_byte(i), hexmode);	break;
				case 1:		row << format_number_unsigned(memory_read_byte(i), hexmode);	break;
				case 2:		row << format_number_signed(memory_read_word(i), hexmode);	break;
				case 3:		row << format_number_unsigned(memory_read_word(i), hexmode);	break;
				case 4:		row << format_number_signed(memory_read_dword(i), hexmode);	break;
				case 5:		row << format_number_unsigned(memory_read_dword(i), hexmode);	break;
				case 6:		row << format_number_signed(memory_read_qword(i), hexmode);	break;
				case 7:		row << format_number_unsigned(memory_read_qword(i), hexmode);	break;
				};
				row << std::endl;
				ret = ret + row.str();
			}
		} else {
			ret = "Too many candidates to display";
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
	wxwindow_memorysearch* tmp = this;
	int id = e.GetId();
	if(id == wxID_RESET) {
		msearch->reset();
	} else if(id == wxID_UPDATE) {
		update();
	} else if(id == wxID_TYPESELECT) {
		wxString c = type->GetValue();
		for(unsigned i = 0; i < DATATYPES; i++)
			if(c == towxstring(datatypes[i]))
				typecode = i;
	} else if(id == wxID_HEX_SELECT) {
		hexmode = hexmode2->GetValue();
	} else if(id == wxID_BUTTONS_BASE) {
		//Value search.
		switch(typecode)
		{
		case 0:		valuesearch<int8_t>();		break;
		case 1:		valuesearch<uint8_t>();		break;
		case 2:		valuesearch<int16_t>();		break;
		case 3:		valuesearch<uint16_t>();	break;
		case 4:		valuesearch<int32_t>();		break;
		case 5:		valuesearch<uint32_t>();	break;
		case 6:		valuesearch<int64_t>();		break;
		case 7:		valuesearch<uint64_t>();	break;
		};
	} else if(id > wxID_BUTTONS_BASE && id < wxID_BUTTONS_BASE + 1 + PRIMITIVES ) {
		int button = id - wxID_BUTTONS_BASE - 1;
		(msearch->*(primitive_searches[typecode][button]))();
	}
	update();
}

template<typename T> void wxwindow_memorysearch::valuesearch()
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
	valuesearch2(val2);
	update();
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
