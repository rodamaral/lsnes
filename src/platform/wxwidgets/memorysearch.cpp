#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/instance-map.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/project.hpp"
#include "core/ui-services.hpp"
#include "library/hex.hpp"
#include "library/string.hpp"
#include "library/memorysearch.hpp"
#include "library/int24.hpp"
#include "library/zip.hpp"

#include "platform/wxwidgets/loadsave.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/scrollbar.hpp"
#include "platform/wxwidgets/textrender.hpp"

#include <sstream>
#include <fstream>
#include <iomanip>

#define wxID_RESET (wxID_HIGHEST + 1)
#define wxID_UPDATE (wxID_HIGHEST + 2)
#define wxID_TYPESELECT (wxID_HIGHEST + 3)
#define wxID_HEX_SELECT (wxID_HIGHEST + 4)
#define wxID_ADD (wxID_HIGHEST + 5)
#define wxID_SET_REGIONS (wxID_HIGHEST + 6)
#define wxID_AUTOUPDATE (wxID_HIGHEST + 7)
#define wxID_DISQUALIFY (wxID_HIGHEST + 8)
#define wxID_POKE (wxID_HIGHEST + 9)
#define wxID_SHOW_HEXEDITOR (wxID_HIGHEST + 10)
#define wxID_MENU_SAVE_PREVMEM (wxID_HIGHEST + 11)
#define wxID_MENU_SAVE_SET (wxID_HIGHEST + 12)
#define wxID_MENU_SAVE_ALL (wxID_HIGHEST + 13)
#define wxID_MENU_LOAD (wxID_HIGHEST + 14)
#define wxID_MENU_UNDO (wxID_HIGHEST + 15)
#define wxID_MENU_REDO (wxID_HIGHEST + 16)
#define wxID_MENU_DUMP_CANDIDATES (wxID_HIGHEST + 17)
#define wxID_BUTTONS_BASE (wxID_HIGHEST + 128)

#define DATATYPES 12
#define CANDIDATE_LIMIT 512

class wxwindow_memorysearch;
memory_search* wxwindow_memorysearch_active();

namespace
{
	unsigned UNDOHISTORY_MAXSIZE = 48;
	struct _watch_properties {
		unsigned len;
		int type;  //0 => Unsigned, 1 => Signed, 2 => Float.
		const char* hformat;
	} watch_properties[] = {
		{1, 1, "%02x"},
		{1, 0, "%02x"},
		{2, 1, "%04x"},
		{2, 0, "%04x"},
		{3, 1, "%06x"},
		{3, 0, "%06x"},
		{4, 1, "%08x"},
		{4, 0, "%08x"},
		{8, 1, "%016x"},
		{8, 0, "%016x"},
		{4, 2, ""},
		{8, 2, ""},
	};

	instance_map<wxwindow_memorysearch> mwatch;

	const char* datatypes[] = {
		"signed byte",
		"unsigned byte",
		"signed word",
		"unsigned word",
		"signed hword",
		"unsigned hword",
		"signed dword",
		"unsigned dword",
		"signed qword",
		"unsigned qword",
		"float",
		"double"
	};

	typedef void (wxwindow_memorysearch::*search_fn_t)();

	struct searchtype
	{
		const char* name;
		search_fn_t searches[DATATYPES];
	};

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

	template<> std::string format_number_signed<ss_uint24_t>(ss_uint24_t val, bool hex)
	{
		return format_number_signedh((int32_t)(uint32_t)(val), 6, hex);
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

	template<> std::string format_number_unsigned<ss_uint24_t>(ss_uint24_t val, bool hex)
	{
		return format_number_unsignedh(val, 6, hex);
	}

	template<> std::string format_number_unsigned<uint32_t>(uint32_t val, bool hex)
	{
		return format_number_unsignedh(val, 8, hex);
	}

	template<> std::string format_number_unsigned<uint64_t>(uint64_t val, bool hex)
	{
		return format_number_unsignedh(val, 16, hex);
	}

	std::string format_number_float(double val)
	{
		return (stringfmt() << val).str();
	}
}

class wxwindow_memorysearch_vmasel : public wxDialog
{
public:
	wxwindow_memorysearch_vmasel(wxWindow* p, emulator_instance& _inst, const std::set<std::string>& enabled);
	bool ShouldPreventAppExit() const;
	std::set<std::string> get_vmas();
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
private:
	emulator_instance& inst;
	std::set<std::string> vmas;
	std::vector<wxCheckBox*> checkboxes;
	wxButton* ok;
	wxButton* cancel;
};

wxwindow_memorysearch_vmasel::wxwindow_memorysearch_vmasel(wxWindow* p, emulator_instance& _inst,
	const std::set<std::string>& enabled)
	: wxDialog(p, wxID_ANY, towxstring("lsnes: Select enabled regions"), wxDefaultPosition, wxSize(300, -1)),
	inst(_inst)
{
	CHECK_UI_THREAD;
	auto i = inst.memory->get_regions();
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
	CHECK_UI_THREAD;
	for(auto i : checkboxes)
		if(i->GetValue())
			vmas.insert(tostdstring(i->GetLabel()));
	EndModal(wxID_OK);
}

void wxwindow_memorysearch_vmasel::on_cancel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}


class wxwindow_memorysearch : public wxFrame
{
public:
	class panel : public text_framebuffer_panel
	{
	public:
		panel(wxwindow_memorysearch* parent, emulator_instance& _inst);
		void set_selection(uint64_t first, uint64_t last);
		void get_selection(uint64_t& first, uint64_t& last);
	protected:
		void prepare_paint();
	private:
		emulator_instance& inst;
		wxwindow_memorysearch* parent;
		uint64_t first_sel;
		uint64_t last_sel;
	};
	wxwindow_memorysearch(emulator_instance& _inst);
	~wxwindow_memorysearch();
	bool ShouldPreventAppExit() const;
	void on_close(wxCloseEvent& e);
	void on_button_click(wxCommandEvent& e);
	void auto_update();
	void on_mousedrag(wxMouseEvent& e);
	void on_mouse(wxMouseEvent& e);
	emulator_instance& inst;
	bool update_queued;
	template<void(memory_search::*sfn)()> void search_0();
	template<typename T, typename T2, void(memory_search::*sfn)(T2 val)> void search_1();
	template<typename T> void _do_poke_addr(uint64_t addr);
	template<typename T> std::string _do_format_signed(uint64_t addr, bool hex, bool old)
	{
		if(old)
			return format_number_signed<T>(msearch->v_readold<T>(addr), hex);
		else
			return format_number_signed<T>(inst.memory->read<T>(addr), hex);
	}
	template<typename T> std::string _do_format_unsigned(uint64_t addr, bool hex, bool old)
	{
		if(old)
			return format_number_unsigned<T>(msearch->v_readold<T>(addr), hex);
		else
			return format_number_unsigned<T>(inst.memory->read<T>(addr), hex);
	}
	template<typename T> std::string _do_format_float(uint64_t addr, bool hex, bool old)
	{
		if(old)
			return format_number_float(msearch->v_readold<T>(addr));
		else
			return format_number_float(inst.memory->read<T>(addr));
	}
	void dump_candidates_text();
private:
	friend memory_search* wxwindow_memorysearch_active(emulator_instance& inst);
	friend class panel;
	template<typename T> T promptvalue(bool& bad);
	void update();
	memory_search* msearch;
	void on_mouse0(wxMouseEvent& e, bool polarity);
	void on_mousedrag();
	void on_mouse2(wxMouseEvent& e);
	void handle_undo_redo(bool redo);
	void push_undo();
	void handle_save(memory_search::savestate_type type);
	void handle_load();
	std::string format_address(uint64_t addr);
	wxStaticText* count;
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
	std::map<std::string, std::pair<uint64_t, uint64_t>> vma_info;
	wxMenuItem* undoitem;
	wxMenuItem* redoitem;
	std::list<std::vector<char>> undohistory;
	std::list<std::vector<char>> redohistory;
};

std::string wxwindow_memorysearch::format_address(uint64_t addr)
{
	for(auto i : vma_info) {
		if(i.second.first <= addr && i.second.first + i.second.second > addr) {
			//Hit.
			unsigned hcount = 1;
			uint64_t t = i.second.second;
			while(t > 0x10) { hcount++; t >>= 4; }
			return (stringfmt() << i.first << "+" << std::hex << std::setw(hcount) << std::setfill('0')
				<< (addr - i.second.first)).str();
		}
	}
	//Fallback.
	return hex::to(addr);
}

namespace
{
	typedef void (wxwindow_memorysearch::*pokefn_t)(uint64_t);
	pokefn_t pokes[] = {
		&wxwindow_memorysearch::_do_poke_addr<int8_t>,
		&wxwindow_memorysearch::_do_poke_addr<uint8_t>,
		&wxwindow_memorysearch::_do_poke_addr<int16_t>,
		&wxwindow_memorysearch::_do_poke_addr<uint16_t>,
		&wxwindow_memorysearch::_do_poke_addr<ss_int24_t>,
		&wxwindow_memorysearch::_do_poke_addr<ss_uint24_t>,
		&wxwindow_memorysearch::_do_poke_addr<int32_t>,
		&wxwindow_memorysearch::_do_poke_addr<uint32_t>,
		&wxwindow_memorysearch::_do_poke_addr<int64_t>,
		&wxwindow_memorysearch::_do_poke_addr<uint64_t>,
		&wxwindow_memorysearch::_do_poke_addr<float>,
		&wxwindow_memorysearch::_do_poke_addr<double>,
	};

	typedef std::string (wxwindow_memorysearch::*displayfn_t)(uint64_t, bool hexmode, bool old);
	displayfn_t displays[] = {
		&wxwindow_memorysearch::_do_format_signed<uint8_t>,
		&wxwindow_memorysearch::_do_format_unsigned<uint8_t>,
		&wxwindow_memorysearch::_do_format_signed<uint16_t>,
		&wxwindow_memorysearch::_do_format_unsigned<uint16_t>,
		&wxwindow_memorysearch::_do_format_signed<ss_uint24_t>,
		&wxwindow_memorysearch::_do_format_unsigned<ss_uint24_t>,
		&wxwindow_memorysearch::_do_format_signed<uint32_t>,
		&wxwindow_memorysearch::_do_format_unsigned<uint32_t>,
		&wxwindow_memorysearch::_do_format_signed<uint64_t>,
		&wxwindow_memorysearch::_do_format_unsigned<uint64_t>,
		&wxwindow_memorysearch::_do_format_float<float>,
		&wxwindow_memorysearch::_do_format_float<double>,
	};

	struct searchtype searchtbl[] = {
		{
			"value", {
				&wxwindow_memorysearch::search_1<int8_t, uint8_t,
					&memory_search::s_value<uint8_t>>,
				&wxwindow_memorysearch::search_1<uint8_t, uint8_t,
					&memory_search::s_value<uint8_t>>,
				&wxwindow_memorysearch::search_1<int16_t, uint16_t,
					&memory_search::s_value<uint16_t>>,
				&wxwindow_memorysearch::search_1<uint16_t, uint16_t,
					&memory_search::s_value<uint16_t>>,
				&wxwindow_memorysearch::search_1<ss_int24_t, ss_uint24_t,
					&memory_search::s_value<ss_uint24_t>>,
				&wxwindow_memorysearch::search_1<ss_uint24_t, ss_uint24_t,
					&memory_search::s_value<ss_uint24_t>>,
				&wxwindow_memorysearch::search_1<int32_t, uint32_t,
					&memory_search::s_value<uint32_t>>,
				&wxwindow_memorysearch::search_1<uint32_t, uint32_t,
					&memory_search::s_value<uint32_t>>,
				&wxwindow_memorysearch::search_1<int64_t, uint64_t,
					&memory_search::s_value<uint64_t>>,
				&wxwindow_memorysearch::search_1<uint64_t, uint64_t,
					&memory_search::s_value<uint64_t>>,
				&wxwindow_memorysearch::search_1<float, float,
					&memory_search::s_value<float>>,
				&wxwindow_memorysearch::search_1<double, double,
					&memory_search::s_value<double>>
			}
		},{
			"diff.", {
				&wxwindow_memorysearch::search_1<int8_t, uint8_t,
					&memory_search::s_difference<uint8_t>>,
				&wxwindow_memorysearch::search_1<uint8_t, uint8_t,
					&memory_search::s_difference<uint8_t>>,
				&wxwindow_memorysearch::search_1<int16_t, uint16_t,
					&memory_search::s_difference<uint16_t>>,
				&wxwindow_memorysearch::search_1<uint16_t, uint16_t,
					&memory_search::s_difference<uint16_t>>,
				&wxwindow_memorysearch::search_1<ss_int24_t, ss_uint24_t,
					&memory_search::s_difference<ss_uint24_t>>,
				&wxwindow_memorysearch::search_1<ss_uint24_t, ss_uint24_t,
					&memory_search::s_difference<ss_uint24_t>>,
				&wxwindow_memorysearch::search_1<int32_t, uint32_t,
					&memory_search::s_difference<uint32_t>>,
				&wxwindow_memorysearch::search_1<uint32_t, uint32_t,
					&memory_search::s_difference<uint32_t>>,
				&wxwindow_memorysearch::search_1<int64_t, uint64_t,
					&memory_search::s_difference<uint64_t>>,
				&wxwindow_memorysearch::search_1<uint64_t, uint64_t,
					&memory_search::s_difference<uint64_t>>,
				&wxwindow_memorysearch::search_1<float, float,
					&memory_search::s_difference<float>>,
				&wxwindow_memorysearch::search_1<double, double,
					&memory_search::s_difference<double>>
			}
		},{
			"<", {
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<int8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<int16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<int32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<int64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<double>>
			}
		},{
			"<=", {
				&wxwindow_memorysearch::search_0<&memory_search::s_le<int8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<int16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<int32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<int64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<double>>
			}
		},{
			"==", {
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<int8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<int16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<int32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<int64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_eq<double>>
			}
		},{
			"!=", {
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<int8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<int16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<int32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<int64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ne<double>>
			}
		},{
			">=", {
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<int8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<int16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<int32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<int64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<double>>
			}
		},{
			">", {
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<int8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<int16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<int32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<int64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<double>>
			}
		},{
			"seq<", {
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqlt<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_lt<double>>
			}
		},{
			"seq<=", {
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqle<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_le<double>>
			}
		},{
			"seq>=", {
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqge<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_ge<double>>
			}
		},{
			"seq>", {
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint8_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint16_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<ss_int24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<ss_uint24_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint32_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_seqgt<uint64_t>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<float>>,
				&wxwindow_memorysearch::search_0<&memory_search::s_gt<double>>
			}
		},{
			"true", {
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>,
				&wxwindow_memorysearch::search_0<&memory_search::update>
			}
		}
	};
}

wxwindow_memorysearch::wxwindow_memorysearch(emulator_instance& _inst)
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Memory Search"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX), inst(_inst)
{
	CHECK_UI_THREAD;
	typecode = 0;
	wxButton* tmp;
	Centre();
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwindow_memorysearch::on_close));
	msearch = new memory_search(*inst.memory);

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

	wxFlexGridSizer* searches = new wxFlexGridSizer(0, 6, 0, 0);
	for(unsigned j = 0; j < sizeof(searchtbl)/sizeof(searchtbl[0]); j++) {
		std::string name = searchtbl[j].name;
		searches->Add(tmp = new wxButton(this, wxID_BUTTONS_BASE + j, towxstring(name)), 1, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwindow_memorysearch::on_button_click), NULL, this);
	}
	toplevel->Add(searches);

	toplevel->Add(count = new wxStaticText(this, wxID_ANY, wxT("XXXXXX candidates")), 0, wxGROW);
	wxBoxSizer* matchesb = new wxBoxSizer(wxHORIZONTAL);
	matchesb->Add(matches = new panel(this, inst), 1, wxGROW);
	matchesb->Add(scroll = new scroll_bar(this, wxID_ANY, true), 0, wxGROW);
	toplevel->Add(matchesb, 1, wxGROW);

	scroll->set_page_size(matches->get_characters().second);

	for(auto i : inst.memory->get_regions()) {
		if(memory_search::searchable_region(i))
			vmas_enabled.insert(i->name);
		vma_info[i->name] = std::make_pair(i->base, i->size);
	}

	wxMenuBar* menubar = new wxMenuBar();
	SetMenuBar(menubar);
	wxMenu* filemenu = new wxMenu();
	filemenu->Append(wxID_MENU_DUMP_CANDIDATES, wxT("Dump candidates..."));
	filemenu->AppendSeparator();
	filemenu->Append(wxID_MENU_SAVE_PREVMEM, wxT("Save previous memory..."));
	filemenu->Append(wxID_MENU_SAVE_SET, wxT("Save set of addresses..."));
	filemenu->Append(wxID_MENU_SAVE_ALL, wxT("Save previous memory and set of addresses..."));
	filemenu->AppendSeparator();
	filemenu->Append(wxID_MENU_LOAD, wxT("Load save..."));
	menubar->Append(filemenu, wxT("File"));
	wxMenu* editmenu = new wxMenu();
	undoitem = editmenu->Append(wxID_UNDO, wxT("Undo"));
	redoitem = editmenu->Append(wxID_REDO, wxT("Redo"));
	undoitem->Enable(false);
	redoitem->Enable(false);
	menubar->Append(editmenu, wxT("Edit"));
	Connect(wxID_MENU_DUMP_CANDIDATES, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));
	Connect(wxID_MENU_SAVE_PREVMEM, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));
	Connect(wxID_MENU_SAVE_SET, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));
	Connect(wxID_MENU_SAVE_ALL, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));
	Connect(wxID_MENU_LOAD, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));
	Connect(wxID_UNDO, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));
	Connect(wxID_REDO, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxwindow_memorysearch::on_button_click));

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

wxwindow_memorysearch::panel::panel(wxwindow_memorysearch* _parent, emulator_instance& _inst)
	: text_framebuffer_panel(_parent, 40, 25, wxID_ANY, NULL), inst(_inst)
{
	parent = _parent;
	first_sel = 0;
	last_sel = 0;
}

void wxwindow_memorysearch::panel::prepare_paint()
{
	CHECK_UI_THREAD;
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
	inst.iqueue->run([&toomany, &first, &last, ms, &lines, &addrs, &addr_count, _parent]() {
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
			unsigned long j = 0;
			for(auto i : addrs2) {
				std::string row = _parent->format_address(i) + " ";
				row += (_parent->*displays[_parent->typecode])(i, _parent->hexmode, false);
				row += " (Was: ";
				row += (_parent->*displays[_parent->typecode])(i, _parent->hexmode, true);
				row += ')';
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
	mwatch.remove(inst);
}

bool wxwindow_memorysearch::ShouldPreventAppExit() const
{
	return false;
}

void wxwindow_memorysearch::dump_candidates_text()
{
	try {
		std::string filename = choose_file_save(this, "Dump memory search",
			UI_get_project_otherpath(inst), filetype_textfile);
		std::ofstream out(filename);
		auto ms = msearch;
		inst.iqueue->run([ms, this, &out]() {
			std::list<uint64_t> addrs2 = ms->get_candidates();
			for(auto i : addrs2) {
				std::string row = format_address(i) + " ";
				row += (this->*displays[this->typecode])(i, this->hexmode, false);
				row += " (Was: ";
				row += (this->*displays[this->typecode])(i, this->hexmode, true);
				row += ')';
				out << row << std::endl;
			}
		});
		if(!out)
			throw std::runtime_error("Can't write save file");
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Save error", std::string(e.what()), wxICON_WARNING);
		return;
	}
}

void wxwindow_memorysearch::handle_save(memory_search::savestate_type type)
{
	try {
		std::vector<char> state;
		msearch->savestate(state, type);
		std::string filename = choose_file_save(this, "Save memory search",
			UI_get_project_otherpath(inst), filetype_memorysearch);
		std::ofstream out(filename, std::ios::binary);
		out.write(&state[0], state.size());
		if(!out)
			throw std::runtime_error("Can't write save file");
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Save error", std::string(e.what()), wxICON_WARNING);
		return;
	}
}

void wxwindow_memorysearch::handle_load()
{
	try {
		std::string filename = choose_file_load(this, "Load memory search",
			UI_get_project_otherpath(inst), filetype_memorysearch);
		std::vector<char> state = zip::readrel(filename, "");
		push_undo();
		msearch->loadstate(state);
		update();
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Load error", std::string(e.what()), wxICON_WARNING);
		return;
	}
}

void wxwindow_memorysearch::handle_undo_redo(bool redo)
{
	std::list<std::vector<char>>& a = *(redo ? &redohistory : &undohistory);
	std::list<std::vector<char>>& b = *(redo ? &undohistory : &redohistory);
	if(!a.size()) {
		show_message_ok(this, "Undo/Redo error", "Can't find state to undo/redo to", wxICON_WARNING);
		return;
	}
	bool pushed = false;
	try {
		std::vector<char> state;
		msearch->savestate(state, memory_search::ST_SET);
		b.push_back(state);
		pushed = true;
		msearch->loadstate(a.back());
		a.pop_back();
	} catch(std::exception& e) {
		if(pushed)
			b.pop_back();
		show_message_ok(this, "Undo/Redo error", std::string(e.what()), wxICON_WARNING);
		return;
	}
	undoitem->Enable(undohistory.size());
	redoitem->Enable(redohistory.size());
	update();
}

void wxwindow_memorysearch::push_undo()
{
	try {
		std::vector<char> state;
		msearch->savestate(state, memory_search::ST_SET);
		undohistory.push_back(state);
		if(undohistory.size() > UNDOHISTORY_MAXSIZE)
			undohistory.pop_front();
		redohistory.clear();
		undoitem->Enable(undohistory.size());
		redoitem->Enable(redohistory.size());
	} catch(...) {
	}
}

void wxwindow_memorysearch::on_mouse(wxMouseEvent& e)
{
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
	wxMenu menu;
	bool some_selected;
	uint64_t selcount = 0;
	uint64_t start, end;
	matches->get_selection(start, end);
	some_selected = (start < end);
	if(!some_selected) {
		uint64_t first = scroll->get_position();
		act_line = first + e.GetY() / matches->get_cell().second;
		if(addresses.count(act_line)) {
			some_selected = true;
			selcount++;
		}
	}
	menu.Append(wxID_ADD, wxT("Add watch..."))->Enable(some_selected);
	menu.Append(wxID_SHOW_HEXEDITOR, wxT("Select in hex editor"))->Enable(selcount == 1 &&
		wxeditor_hexeditor_available(inst));
	menu.Append(wxID_POKE, wxT("Poke..."))->Enable(selcount == 1);
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
	CHECK_UI_THREAD;
	Destroy();
}

void wxwindow_memorysearch::auto_update()
{
	CHECK_UI_THREAD;
	if(autoupdate->GetValue())
		update();
}

void wxwindow_memorysearch::update()
{
	matches->request_paint();
}

void wxwindow_memorysearch::on_button_click(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId();
	if(id == wxID_RESET) {
		push_undo();
		msearch->reset();
		//Update all VMA info too.
		for(auto i : inst.memory->get_regions()) {
			if(memory_search::searchable_region(i) && !vmas_enabled.count(i->name))
				msearch->dq_range(i->base, i->last_address());
			vma_info[i->name] = std::make_pair(i->base, i->size);
		}
		wxeditor_hexeditor_update(inst);
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
		for(uint64_t r = start; r < end; r++) {
			if(!addresses.count(r))
				continue;
			uint64_t addr = addresses[r];
			try {
				std::string n = pick_text(this, "Name for watch", (stringfmt()
					<< "Enter name for watch at 0x" << std::hex << addr << ":").str());
				if(n == "")
					continue;
				memwatch_item e;
				e.expr = (stringfmt() << addr).str();
				bool is_hex = hexmode2->GetValue();
				e.bytes = watch_properties[typecode].len;
				e.signed_flag = !is_hex && (watch_properties[typecode].type == 1);
				e.float_flag = (watch_properties[typecode].type == 2);
				if(e.float_flag) is_hex = false;
				e.format = is_hex ? watch_properties[typecode].hformat : "";
				auto i = inst.memory->get_regions();
				int endianess = 0;
				for(auto& j : i) {
					if(addr >= j->base && addr < j->base + j->size)
						endianess = j->endian;
				}
				e.endianess = endianess;
				inst.iqueue->run([n, &e]() { CORE().mwatch->set(n, e); });
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
		push_undo();
		for(uint64_t r = start; r < end; r++) {
			if(!addresses.count(r))
				return;
			uint64_t addr = addresses[r];
			auto ms = msearch;
			inst.iqueue->run([addr, ms]() { ms->dq_range(addr, addr); });
		}
		matches->set_selection(0, 0);
		wxeditor_hexeditor_update(inst);
	} else if(id == wxID_SET_REGIONS) {
		wxwindow_memorysearch_vmasel* d = new wxwindow_memorysearch_vmasel(this, inst, vmas_enabled);
		if(d->ShowModal() == wxID_OK)
			vmas_enabled = d->get_vmas();
		else {
			d->Destroy();
			return;
		}
		d->Destroy();
		push_undo();
		for(auto i : inst.memory->get_regions())
			if(memory_search::searchable_region(i) && !vmas_enabled.count(i->name))
				msearch->dq_range(i->base, i->last_address());
		wxeditor_hexeditor_update(inst);
	} else if(id == wxID_POKE) {
		uint64_t start, end;
		matches->get_selection(start, end);
		if(start == end) {
			start = act_line;
			end = act_line + 1;
		}
		for(uint64_t r = start; r < end; r++) {
			if(!addresses.count(r))
				continue;
			uint64_t addr = addresses[r];
			try {
				(this->*(pokes[typecode]))(addr);
			} catch(canceled_exception& e) {
			}
			return;
		}
	} else if(id == wxID_SHOW_HEXEDITOR) {
		uint64_t start, end;
		matches->get_selection(start, end);
		if(start == end) {
			start = act_line;
			end = act_line + 1;
		}
		for(uint64_t r = start; r < end; r++) {
			if(!addresses.count(r))
				continue;
			wxeditor_hexeditor_jumpto(inst, addresses[r]);
			return;
		}
	} else if(id >= wxID_BUTTONS_BASE && id < wxID_BUTTONS_BASE +
			(ssize_t)(sizeof(searchtbl)/sizeof(searchtbl[0]))) {
		int button = id - wxID_BUTTONS_BASE;
		push_undo();
		uint64_t old_count = msearch->get_candidate_count();
		(this->*(searchtbl[button].searches[typecode]))();
		uint64_t new_count = msearch->get_candidate_count();
		if(old_count == new_count) {
			undohistory.pop_back();  //Shouldn't be undoable.
			undoitem->Enable(undohistory.size());
		}
		wxeditor_hexeditor_update(inst);
	} else if(id == wxID_MENU_DUMP_CANDIDATES) {
		dump_candidates_text();
	} else if(id == wxID_MENU_SAVE_PREVMEM) {
		handle_save(memory_search::ST_PREVMEM);
	} else if(id == wxID_MENU_SAVE_SET) {
		handle_save(memory_search::ST_SET);
	} else if(id == wxID_MENU_SAVE_ALL) {
		handle_save(memory_search::ST_ALL);
	} else if(id == wxID_MENU_LOAD) {
		handle_load();
	} else if(id == wxID_UNDO) {
		handle_undo_redo(false);
	} else if(id == wxID_REDO) {
		handle_undo_redo(true);
	}
	update();
}

template<typename T> void wxwindow_memorysearch::_do_poke_addr(uint64_t addr)
{
	CHECK_UI_THREAD;
	T val = msearch->v_read<T>(addr);
	std::string v;
	try {
		v = pick_text(this, "Poke value", (stringfmt() << "Enter value to poke to " << std::hex << "0x"
			<< addr).str(), (stringfmt() << val).str(), false);
		val = parse_value<T>(v);
	} catch(canceled_exception& e) {
		return;
	} catch(...) {
		wxMessageBox(towxstring("Bad value '" + v + "'"), _T("Error"), wxICON_WARNING | wxOK, this);
		return;
	}
	msearch->v_write<T>(addr, val);
}

template<void(memory_search::*sfn)()> void wxwindow_memorysearch::search_0()
{
	(msearch->*sfn)();
}

template<typename T, typename T2, void(memory_search::*sfn)(T2 val)> void wxwindow_memorysearch::search_1()
{
	CHECK_UI_THREAD;
	bool bad = false;
	T val = promptvalue<T>(bad);
	if(bad)
		return;
	(msearch->*sfn)(static_cast<T2>(val));
}

template<typename T> T wxwindow_memorysearch::promptvalue(bool& bad)
{
	CHECK_UI_THREAD;
	std::string v;
	wxTextEntryDialog* d = new wxTextEntryDialog(this, wxT("Enter value to search for:"), wxT("Memory search"),
		wxT(""));
	if(d->ShowModal() == wxID_CANCEL) {
		bad = true;
		return 0;
	}
	v = tostdstring(d->GetValue());
	d->Destroy();
	T val2;
	try {
		val2 = parse_value<T>(v);
	} catch(...) {
		wxMessageBox(towxstring("Bad value '" + v + "'"), _T("Error"), wxICON_WARNING | wxOK, this);
		bad = true;
		return 0;
	}
	return val2;
}

void wxwindow_memorysearch_display(emulator_instance& inst)
{
	CHECK_UI_THREAD;
	auto e = mwatch.lookup(inst);
	if(e) {
		e->Raise();
		return;
	}
	mwatch.create(inst)->Show();
}

void wxwindow_memorysearch_update(emulator_instance& inst)
{
	auto e = mwatch.lookup(inst);
	if(e) e->auto_update();
}

memory_search* wxwindow_memorysearch_active(emulator_instance& inst)
{
	auto e = mwatch.lookup(inst);
	return e ? e->msearch : NULL;
}
