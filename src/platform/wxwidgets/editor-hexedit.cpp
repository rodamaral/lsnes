#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>

#include "core/moviedata.hpp"
#include "core/memorywatch.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/instance-map.hpp"
#include "core/project.hpp"
#include "core/memorymanip.hpp"
#include "core/ui-services.hpp"
#include "library/memorysearch.hpp"
#include "library/hex.hpp"

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include "platform/wxwidgets/scrollbar.hpp"

#include "library/string.hpp"
#include "library/json.hpp"
#include "library/zip.hpp"
#include "interface/romtype.hpp"

#include <iomanip>

class wxeditor_hexedit;

namespace
{
	const size_t maxvaluelen = 8;	//The length of longest value type.
	instance_map<wxeditor_hexedit> editor;

	struct val_type
	{
		const char* name;
		unsigned len;
		bool hard_bigendian;
		const char* format;
		int type;    //0 => Unsigned, 1 => Signed, 2 => Float
		int scale;
		std::string (*read)(const uint8_t* x);
	};

	val_type datatypes[] = {
		{"1 byte (signed)", 1, false, "", 1, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << (int)(char)x[0]).str();
		}},
		{"1 byte (unsigned)", 1, false, "", 0, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << (int)x[0]).str();
		}},
		{"1 byte (hex)", 1, false, "%02x", 0, 0, [](const uint8_t* x) -> std::string {
			return hex::to(x[0]);
		}},
		{"2 bytes (signed)", 2, false, "", 1, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int16_t*)x).str();
		}},
		{"2 bytes (unsigned)", 2, false, "", 0, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint16_t*)x).str();
		}},
		{"2 bytes (hex)", 2, false, "%04x", 0, 0, [](const uint8_t* x) -> std::string {
			return hex::to(*(uint16_t*)x);
		}},
		{"3 bytes (signed)", 3, true, "", 1, 0, [](const uint8_t* x) -> std::string {
			int32_t a = 0;
			a |= (uint32_t)x[0] << 16;
			a |= (uint32_t)x[1] << 8;
			a |= (uint32_t)x[2];
			if(a & 0x800000)
				a -= 0x1000000;
			return (stringfmt() << a).str();
		}},
		{"3 bytes (unsigned)", 3, true, "", 0, 0, [](const uint8_t* x) -> std::string {
			int32_t a = 0;
			a |= (uint32_t)x[0] << 16;
			a |= (uint32_t)x[1] << 8;
			a |= (uint32_t)x[2];
			return (stringfmt() << a).str();
		}},
		{"3 bytes (hex)", 3, true, "%06x", 0, 0, [](const uint8_t* x) -> std::string {
			int32_t a = 0;
			a |= (uint32_t)x[0] << 16;
			a |= (uint32_t)x[1] << 8;
			a |= (uint32_t)x[2];
			return hex::to24(a);
		}},
		{"4 bytes (signed)", 4, false, "", 1, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int32_t*)x).str();
		}},
		{"4 bytes (unsigned)", 4, false, "", 0, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint32_t*)x).str();
		}},
		{"4 bytes (hex)", 4, false, "%08x", 0, 0, [](const uint8_t* x) -> std::string {
			return hex::to(*(uint32_t*)x);
		}},
		{"4 bytes (float)", 4, false, "", 2, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(float*)x).str();
		}},
		{"8 bytes (signed)", 8, false, "", 1, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int64_t*)x).str();
		}},
		{"8 bytes (unsigned)", 8, false, "", 0, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint64_t*)x).str();
		}},
		{"8 bytes (hex)", 8, false, "%016x", 0, 0, [](const uint8_t* x) -> std::string {
			return hex::to(*(uint64_t*)x);
		}},
		{"8 bytes (float)", 8, false, "", 2, 0, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(double*)x).str();
		}},
		{"Q8.8 (signed)", 2, false, "", 1, 8, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int16_t*)x / 256.0).str();
		}},
		{"Q8.8 (unsigned)", 2, false, "", 0, 8, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint16_t*)x / 256.0).str();
		}},
		{"Q12.4 (signed)", 2, false, "", 1, 4, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int16_t*)x / 16.0).str();
		}},
		{"Q12.4 (unsigned)", 2, false, "", 0, 4, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint16_t*)x / 16.0).str();
		}},
		{"Q16.8 (signed)", 3, true, "", 1, 8, [](const uint8_t* x) -> std::string {
			int32_t a = 0;
			a |= (uint32_t)x[0] << 16;
			a |= (uint32_t)x[1] << 8;
			a |= (uint32_t)x[2];
			if(a & 0x800000)
				a -= 0x1000000;
			return (stringfmt() << a / 256.0).str();
		}},
		{"Q16.8 (unsigned)", 3, true, "", 0, 8, [](const uint8_t* x) -> std::string {
			int32_t a = 0;
			a |= (uint32_t)x[0] << 16;
			a |= (uint32_t)x[1] << 8;
			a |= (uint32_t)x[2];
			return (stringfmt() << a / 256.0).str();
		}},
		{"Q24.8 (signed)", 4, false, "", 1, 8, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int32_t*)x / 256.0).str();
		}},
		{"Q24.8 (unsigned)", 4, false, "", 0, 8, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint32_t*)x / 256.0).str();
		}},
		{"Q20.12 (signed)", 4, false, "", 1, 12, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int32_t*)x / 4096.0).str();
		}},
		{"Q20.12 (unsigned)", 4, false, "", 0, 12, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint32_t*)x / 4096.0).str();
		}},
		{"Q16.16 (signed)", 4, false, "", 1, 16, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(int32_t*)x / 65536.0).str();
		}},
		{"Q16.16 (unsigned)", 4, false, "", 0, 16, [](const uint8_t* x) -> std::string {
			return (stringfmt() << *(uint32_t*)x / 65536.0).str();
		}},
	};

	unsigned hexaddr = 6;
	int separators[5] = {6, 15, 24, 28, 42};
	const char32_t* sepchars[5] = {U"\u2502", U" ", U".", U" ", U"\u2502"};
	int hexcol[16] = {7, 9, 11, 13, 16, 18, 20, 22, 25, 27, 29, 31, 34, 36, 38, 40};
	int charcol[16] = {43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58};
	const char32_t* hexes[16] = {U"0", U"1", U"2", U"3", U"4", U"5", U"6", U"7", U"8", U"9", U"A", U"B", U"C",
		U"D", U"E", U"F"};

	enum {
		wxID_OPPOSITE_ENDIAN = wxID_HIGHEST + 1,
		wxID_DATATYPES_FIRST,
		wxID_DATATYPES_LAST = wxID_DATATYPES_FIRST + 255,
		wxID_REGIONS_FIRST,
		wxID_REGIONS_LAST = wxID_REGIONS_FIRST + 255,
		wxID_ADD_BOOKMARK,
		wxID_DELETE_BOOKMARK,
		wxID_LOAD_BOOKMARKS,
		wxID_SAVE_BOOKMARKS,
		wxID_BOOKMARKS_FIRST,
		wxID_BOOKMARKS_LAST = wxID_BOOKMARKS_FIRST + 255,
		wxID_SEARCH_DISQUALIFY,
		wxID_SEARCH_PREV,
		wxID_SEARCH_NEXT,
		wxID_SEARCH_WATCH,
	};
}

class wxeditor_hexedit : public wxFrame
{
public:
	wxeditor_hexedit(emulator_instance& _inst, wxWindow* parent)
		: wxFrame(parent, wxID_ANY, wxT("lsnes: Memory editor"), wxDefaultPosition, wxSize(-1, -1),
			wxCAPTION | wxMINIMIZE_BOX | wxCLOSE_BOX | wxSYSTEM_MENU), inst(_inst)
	{
		CHECK_UI_THREAD;
		Centre();
		wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);
		SetSizer(top);

		destructing = false;
		hex_input_state = -1;
		current_vma = 0;

		Connect(wxEVT_CHAR, wxKeyEventHandler(wxeditor_hexedit::on_keyboard), NULL, this);

		wxBoxSizer* parea = new wxBoxSizer(wxHORIZONTAL);
		parea->Add(hpanel = new _panel(this, inst), 1, wxGROW);
		hpanel->SetFocus();
		parea->Add(scroll = new scroll_bar(this, wxID_ANY, true), 0, wxGROW);
		top->Add(parea, 1, wxGROW);
		scroll->Connect(wxEVT_CHAR, wxKeyEventHandler(wxeditor_hexedit::on_keyboard), NULL, this);

		SetStatusBar(statusbar = new wxStatusBar(this));
		SetMenuBar(menubar = new wxMenuBar);

		valuemenu = new wxMenu();
		menubar->Append(valuemenu, wxT("Value"));
		regionmenu = new wxMenu();
		menubar->Append(regionmenu, wxT("Region"));
		typemenu = new wxMenu();
		bookmarkmenu = new wxMenu();
		bookmarkmenu->Append(wxID_ADD_BOOKMARK, wxT("Add bookmark..."));
		bookmarkmenu->Append(wxID_DELETE_BOOKMARK, wxT("Delete bookmark..."));
		bookmarkmenu->AppendSeparator();
		bookmarkmenu->Append(wxID_LOAD_BOOKMARKS, wxT("Load bookmarks..."));
		bookmarkmenu->Append(wxID_SAVE_BOOKMARKS, wxT("Save bookmarks..."));
		bookmarkmenu->AppendSeparator();
		menubar->Append(bookmarkmenu, wxT("Bookmarks"));
		valuemenu->AppendSubMenu(typemenu, wxT("Type"));
		oendian = valuemenu->AppendCheckItem(wxID_OPPOSITE_ENDIAN, wxT("Little endian"));
		for(size_t i = 0; i < sizeof(datatypes) / sizeof(datatypes[0]); i++)
			typemenu->AppendRadioItem(wxID_DATATYPES_FIRST + i, towxstring(datatypes[i].name));
		typemenu->FindItem(wxID_DATATYPES_FIRST)->Check();
		searchmenu = new wxMenu();
		menubar->Append(searchmenu, wxT("Search"));
		searchmenu->Append(wxID_SEARCH_PREV, wxT("Previous...\tCtrl+P"));
		searchmenu->Append(wxID_SEARCH_NEXT, wxT("Next...\tCtrl+N"));
		searchmenu->Append(wxID_SEARCH_WATCH, wxT("Add watch...\tCtrl+W"));
		searchmenu->AppendSeparator();
		searchmenu->Append(wxID_SEARCH_DISQUALIFY, wxT("Disqualify...\tCtrl+D"));
		set_search_status();

		littleendian = true;
		valuemenu->FindItem(wxID_OPPOSITE_ENDIAN)->Check(littleendian);
		curtype = 0;
		Connect(wxID_ADD_BOOKMARK, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_addbookmark));
		Connect(wxID_DELETE_BOOKMARK, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_deletebookmark));
		Connect(wxID_LOAD_BOOKMARKS, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_loadbookmarks));
		Connect(wxID_SAVE_BOOKMARKS, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_savebookmarks));
		Connect(wxID_OPPOSITE_ENDIAN, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_changeendian));
		Connect(wxID_DATATYPES_FIRST, wxID_DATATYPES_LAST, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_typechange));
		Connect(wxID_REGIONS_FIRST, wxID_REGIONS_LAST, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_vmasel));
		Connect(wxID_BOOKMARKS_FIRST, wxID_BOOKMARKS_LAST, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_bookmark));
		Connect(wxID_SEARCH_DISQUALIFY, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_search_discard));
		Connect(wxID_SEARCH_PREV, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_search_prevnext));
		Connect(wxID_SEARCH_NEXT, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_search_prevnext));
		Connect(wxID_SEARCH_WATCH, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_hexedit::on_search_watch));

		scroll->set_page_size(hpanel->lines);
		scroll->set_handler([this](scroll_bar& s) {
			this->hpanel->offset = s.get_position();
			this->hpanel->request_paint();
		});

		corechange.set(inst.dispatch->core_changed, [this](bool hard) {
			this->on_core_changed(hard); });
		on_core_changed(true);
		top->SetSizeHints(this);
		Fit();
	}
	~wxeditor_hexedit()
	{
		destructing = true;
		editor.remove(inst);
	}
	bool ShouldPreventAppExit() const
	{
		return false;
	}
	void set_search_status()
	{
		CHECK_UI_THREAD;
		bool e = wxwindow_memorysearch_active(inst);
		searchmenu->FindItem(wxID_SEARCH_DISQUALIFY)->Enable(e);
		searchmenu->FindItem(wxID_SEARCH_PREV)->Enable(e);
		searchmenu->FindItem(wxID_SEARCH_NEXT)->Enable(e);
	}
	void on_keyboard(wxKeyEvent& e)
	{
		CHECK_UI_THREAD;
		int c = e.GetKeyCode();
		if(c == WXK_ESCAPE) {
			hex_input_state = -1;
			hpanel->request_paint();
			return;
		}
		if(c == WXK_LEFT && hex_input_state < 0) {
			if(hpanel->seloff > 0) hpanel->seloff--;
			hpanel->request_paint();
			return;
		}
		if(c == WXK_RIGHT && hex_input_state < 0) {
			if(hpanel->seloff + 1 < hpanel->vmasize) hpanel->seloff++;
			hpanel->request_paint();
			return;
		}
		if(c == WXK_UP && hex_input_state < 0) {
			if(hpanel->seloff >= 16) hpanel->seloff -= 16;
			hpanel->request_paint();
			return;
		}
		if(c == WXK_DOWN && hex_input_state < 0) {
			if(hpanel->seloff + 16 < hpanel->vmasize) hpanel->seloff += 16;
			hpanel->request_paint();
			return;
		}
		if(c == WXK_PAGEUP && hex_input_state < 0) {
			scroll->apply_delta(-static_cast<int>(hpanel->lines));
			hpanel->offset = scroll->get_position();
			hpanel->request_paint();
			return;
		}
		if(c == WXK_PAGEDOWN && hex_input_state < 0) {
			scroll->apply_delta(static_cast<int>(hpanel->lines));
			hpanel->offset = scroll->get_position();
			hpanel->request_paint();
			return;
		}
		if(c >= '0' && c <= '9') {
			do_hex(c - '0');
			return;
		}
		if(c >= 'A' && c <= 'F') {
			do_hex(c - 'A' + 10);
			return;
		}
		if(c >= 'a' && c <= 'f') {
			do_hex(c - 'a' + 10);
			return;
		}
		e.Skip();
	}
	void on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		auto cell = hpanel->get_cell();
		if(e.LeftDown())
			hpanel->on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, true);
		if(e.LeftUp())
			hpanel->on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, false);
		unsigned speed = 1;
		if(e.ShiftDown())
			speed = 10;
		if(e.ShiftDown() && e.ControlDown())
			speed = 50;
		scroll->apply_wheel(e.GetWheelRotation(), e.GetWheelDelta(), speed);
		hpanel->offset = scroll->get_position();
	}
	void on_loadbookmarks(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		try {
			std::string filename = choose_file_load(this, "Load bookmarks from file",
				UI_get_project_otherpath(inst), filetype_hexbookmarks);
			auto _in = zip::readrel(filename, "");
			std::string in(_in.begin(), _in.end());
			JSON::node root(in);
			std::vector<bookmark_entry> newbookmarks;
			for(auto i : root) {
				bookmark_entry e;
				e.name = i["name"].as_string8();
				e.vma = i["vma"].as_string8();
				e.scroll = i["offset"].as_int();
				e.sel = i["selected"].as_uint();
				newbookmarks.push_back(e);
			}
			std::swap(bookmarks, newbookmarks);
			for(unsigned i = wxID_BOOKMARKS_FIRST; i <= wxID_BOOKMARKS_LAST; i++) {
				auto p = bookmarkmenu->FindItem(i);
				if(p)
					bookmarkmenu->Delete(p);
			}
			int idx = 0;
			for(auto i : bookmarks) {
				if(wxID_BOOKMARKS_FIRST + idx > wxID_BOOKMARKS_LAST)
					break;
				bookmarkmenu->Append(wxID_BOOKMARKS_FIRST + idx, towxstring(i.name));
				idx++;
			}
		} catch(canceled_exception& e) {
		} catch(std::exception& e) {
			show_message_ok(this, "Error", std::string("Can't load bookmarks: ") + e.what(),
				wxICON_EXCLAMATION);
			return;
		}
	}
	void on_savebookmarks(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		JSON::node root(JSON::array);
		for(auto i : bookmarks) {
			JSON::node n(JSON::object);
			n["name"] = JSON::string(i.name);
			n["vma"] = JSON::string(i.vma);
			n["offset"] = JSON::number((int64_t)i.scroll);
			n["selected"] = JSON::number(i.sel);
			root.append(n);
		}
		std::string doc = root.serialize();
		try {
			std::string filename = choose_file_save(this, "Save bookmarks to file",
				UI_get_project_otherpath(inst), filetype_hexbookmarks);
			std::ofstream out(filename.c_str());
			out << doc << std::endl;
			out.close();
		} catch(canceled_exception& e) {
		} catch(std::exception& e) {
			show_message_ok(this, "Error", std::string("Can't save bookmarks: ") + e.what(),
				wxICON_EXCLAMATION);
		}
	}
	void on_addbookmark(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(bookmarks.size() <= wxID_BOOKMARKS_LAST - wxID_BOOKMARKS_FIRST) {
			std::string name = pick_text(this, "Add bookmark", "Enter name for bookmark", "", false);
			bookmark_entry ent;
			ent.name = name;
			ent.vma = get_current_vma_name();
			ent.scroll = hpanel->offset;
			ent.sel = hpanel->seloff;
			int idx = bookmarks.size();
			bookmarks.push_back(ent);
			bookmarkmenu->Append(wxID_BOOKMARKS_FIRST + idx, towxstring(name));
		} else {
			show_message_ok(this, "Error adding bookmark", "Too many bookmarks", wxICON_EXCLAMATION);
		}
	}
	void on_deletebookmark(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(bookmarks.size() > 0) {
			std::vector<wxString> _choices;
			for(auto i : bookmarks)
				_choices.push_back(towxstring(i.name));
			wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(this, towxstring("Select bookmark "
				"to delete"), towxstring("Delete bookmark"), _choices.size(), &_choices[0]);
			d2->SetSelection(0);
			if(d2->ShowModal() == wxID_CANCEL) {
				d2->Destroy();
				return;
			}
			int sel = d2->GetSelection();
			d2->Destroy();
			if(sel >= 0)
				bookmarks.erase(bookmarks.begin() + sel);
			for(unsigned i = wxID_BOOKMARKS_FIRST; i <= wxID_BOOKMARKS_LAST; i++) {
				auto p = bookmarkmenu->FindItem(i);
				if(p)
					bookmarkmenu->Delete(p);
			}
			int idx = 0;
			for(auto i : bookmarks) {
				bookmarkmenu->Append(wxID_BOOKMARKS_FIRST + idx, towxstring(i.name));
				idx++;
			}
		}
	}
	void rescroll_panel()
	{
		CHECK_UI_THREAD;
		uint64_t vfirst = static_cast<uint64_t>(hpanel->offset) * 16;
		uint64_t vlast = static_cast<uint64_t>(hpanel->offset + hpanel->lines) * 16;
		if(hpanel->seloff < vfirst || hpanel->seloff >= vlast) {
			int l = hpanel->seloff / 16;
			int r = hpanel->lines / 4;
			hpanel->offset = (l > r) ? (l - r) : 0;
			scroll->set_position(hpanel->offset);
		}
	}
	void on_search_discard(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		auto p = wxwindow_memorysearch_active(inst);
		if(!p)
			return;
		if(hpanel->seloff < hpanel->vmasize) {
			p->dq_range(hpanel->vmabase + hpanel->seloff, hpanel->vmabase + hpanel->seloff);
			wxwindow_memorysearch_update(inst);
			hpanel->seloff = p->cycle_candidate_vma(hpanel->vmabase + hpanel->seloff, true) -
				hpanel->vmabase;
			rescroll_panel();
			hpanel->request_paint();
		}
	}
	void on_search_watch(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		try {
			if(!hpanel->vmasize)
				return;
			uint64_t addr = hpanel->vmabase + hpanel->seloff;
			std::string n = pick_text(this, "Name for watch", (stringfmt()
				<< "Enter name for watch at 0x" << std::hex << addr << ":").str());
			if(n == "")
				return;
			memwatch_item e;
			e.expr = (stringfmt() << addr).str();
			e.format = datatypes[curtype].format;
			e.bytes = datatypes[curtype].len;
			e.signed_flag = (datatypes[curtype].type == 1);
			e.float_flag = (datatypes[curtype].type == 2);
			//Handle hostendian VMAs.
			auto i = inst.memory->get_regions();
			bool hostendian = false;
			for(auto& j : i) {
				if(addr >= j->base && addr < j->base + j->size && !j->endian)
					hostendian = true;
			}
			e.endianess = hostendian ? 0 : (littleendian ? -1 : 1);
			e.scale_div = 1ULL << datatypes[curtype].scale;
			inst.iqueue->run([n, &e]() { CORE().mwatch->set(n, e); });
		} catch(canceled_exception& e) {
		}
	}
	void on_search_prevnext(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		auto p = wxwindow_memorysearch_active(inst);
		if(!p)
			return;
		if(hpanel->seloff < hpanel->vmasize) {
			hpanel->seloff = p->cycle_candidate_vma(hpanel->vmabase + hpanel->seloff, e.GetId() ==
				wxID_SEARCH_NEXT) - hpanel->vmabase;
			rescroll_panel();
			hpanel->request_paint();
		}
	}
	void on_bookmark(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		int id = e.GetId();
		if(id < wxID_BOOKMARKS_FIRST || id > wxID_BOOKMARKS_LAST)
			return;
		bookmark_entry ent = bookmarks[id - wxID_BOOKMARKS_FIRST];
		int r = vma_index_for_name(ent.vma);
		uint64_t base = 0, size = 0;
		auto i = inst.memory->get_regions();
		for(auto j : i) {
			if(j->readonly || j->special)
				continue;
			if(j->name == ent.vma) {
				base = j->base;
				size = j->size;
			}
		}
		if(ent.sel >= size || ent.scroll >= (ssize_t)((size + 15) / 16))
			goto invalid_bookmark;
		current_vma = r;
		regionmenu->FindItem(wxID_REGIONS_FIRST + current_vma)->Check();
		update_vma(base, size);
		hpanel->offset = ent.scroll;
		hpanel->seloff = ent.sel;
		scroll->set_position(hpanel->offset);
		hpanel->request_paint();
		return;
invalid_bookmark:
		show_message_ok(this, "Error jumping to bookmark", "Bookmark refers to nonexistent location",
			wxICON_EXCLAMATION);
		return;
	}
	void on_vmasel(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(destructing)
			return;
		int selected = e.GetId();
		if(selected < wxID_REGIONS_FIRST || selected > wxID_REGIONS_LAST)
			return;
		selected -= wxID_REGIONS_FIRST;
		auto i = inst.memory->get_regions();
		int index = 0;
		for(auto j : i) {
			if(j->readonly || j->special)
				continue;
			if(index == selected) {
				if(j->base != hpanel->vmabase || j->size != hpanel->vmasize)
					update_vma(j->base, j->size);
				current_vma = index;
				if(vma_endians.count(index)) {
					littleendian = vma_endians[index];
					valuemenu->FindItem(wxID_OPPOSITE_ENDIAN)->Check(littleendian);
				}
				return;
			}
			index++;
		}
		current_vma = index;
		update_vma(0, 0);
	}
	bool is_endian_little(int endian)
	{
		if(endian < 0) return true;
		if(endian > 0) return false;
		uint16_t magic = 1;
		return (*reinterpret_cast<uint8_t*>(&magic) == 1);
	}
	void update_vma(uint64_t base, uint64_t size)
	{
		CHECK_UI_THREAD;
		hpanel->vmabase = base;
		hpanel->vmasize = size;
		hpanel->offset = 0;
		hpanel->seloff = 0;
		scroll->set_range((size + 15) / 16);
		scroll->set_position(0);
		hpanel->request_paint();
	}
	void on_typechange(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(destructing)
			return;
		int id = e.GetId();
		if(id < wxID_DATATYPES_FIRST || id > wxID_DATATYPES_LAST)
			return;
		curtype = id - wxID_DATATYPES_FIRST;
		hpanel->request_paint();
	}
	void on_changeendian(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(destructing)
			return;
		littleendian = valuemenu->FindItem(wxID_OPPOSITE_ENDIAN)->IsChecked();
		if(vma_endians.count(current_vma))
			vma_endians[current_vma] = littleendian;
		hpanel->request_paint();
	}
	void updated()
	{
		CHECK_UI_THREAD;
		if(destructing)
			return;
		hpanel->request_paint();
	}
	void jumpto(uint64_t addr)
	{
		CHECK_UI_THREAD;
		if(destructing)
			return;
		//Switch to correct VMA.
		auto i = inst.memory->get_regions();
		int index = 0;
		for(auto j : i) {
			if(j->readonly || j->special)
				continue;
			if(addr >= j->base && addr < j->base + j->size) {
				if(j->base != hpanel->vmabase || j->size != hpanel->vmasize)
					update_vma(j->base, j->size);
				current_vma = index;
				if(vma_endians.count(index)) {
					littleendian = vma_endians[index];
					valuemenu->FindItem(wxID_OPPOSITE_ENDIAN)->Check(littleendian);
				}
				break;
			}
			index++;
		}
		if(addr < hpanel->vmabase || addr >= hpanel->vmabase + hpanel->vmasize)
			return;
		hpanel->seloff = addr - hpanel->vmabase;
		rescroll_panel();
		hpanel->request_paint();
	}
	void refresh_curvalue()
	{
		CHECK_UI_THREAD;
		uint8_t buf[maxvaluelen];
		memcpy(buf, hpanel->value, maxvaluelen);
		val_type vt = datatypes[curtype];
		if(littleendian != is_endian_little(vt.hard_bigendian ? 1 : 0))
			for(unsigned i = 0; i < vt.len / 2; i++)
				std::swap(buf[i], buf[vt.len - i - 1]);
		wxMenuItem* it = regionmenu->FindItem(wxID_REGIONS_FIRST + current_vma);
		std::string vma = "(none)";
		if(it) vma = tostdstring(it->GetItemLabelText());
		unsigned addrlen = 1;
		while(hpanel->vmasize > (1ULL << (4 * addrlen)))
			addrlen++;
		std::string addr = (stringfmt() << std::hex << std::setw(addrlen) << std::setfill('0') <<
			hpanel->seloff).str();
		std::string vtext = vt.read(buf);
		statusbar->SetStatusText(towxstring("Region: " + vma + " Address: " + addr + " Value: " + vtext));
	}
	int vma_index_for_name(const std::string& x)
	{
		for(size_t i = 0; i < vma_names.size(); i++)
			if(vma_names[i] == x)
				return i;
		return -1;
	}
	std::string get_current_vma_name()
	{
		if(current_vma >= vma_names.size())
			return "";
		return vma_names[current_vma];
	}
	void on_core_changed(bool _hard)
	{
		if(destructing)
			return;
		bool hard = _hard;
		runuifun([this, hard]() {
			for(unsigned i = wxID_REGIONS_FIRST; i <= wxID_REGIONS_LAST; i++) {
				auto p = regionmenu->FindItem(i);
				if(p)
					regionmenu->Delete(p);
			}
			std::string current_reg = get_current_vma_name();
			uint64_t nsbase = 0, nssize = 0;
			auto i = inst.memory->get_regions();
			vma_names.clear();
			if(hard)
				vma_endians.clear();
			int index = 0;
			int curreg_index = 0;
			for(auto j : i) {
				if(j->readonly || j->special)
					continue;
				regionmenu->AppendRadioItem(wxID_REGIONS_FIRST + index, towxstring(j->name));
				vma_names.push_back(j->name);
				if(j->name == current_reg || index == 0) {
					curreg_index = index;
					nsbase = j->base;
					nssize = j->size;
				}
				if(!vma_endians.count(index))
					vma_endians[index] = is_endian_little(j->endian);
				index++;
			}
			if(!index) {
				update_vma(0, 0);
				return;
			}
			regionmenu->FindItem(wxID_REGIONS_FIRST + curreg_index)->Check();
			current_vma = curreg_index;
			if(vma_endians.count(current_vma)) {
				littleendian = vma_endians[current_vma];
				typemenu->FindItem(wxID_DATATYPES_FIRST)->Check(littleendian);
			}
			if(nsbase != hpanel->vmabase || nssize != hpanel->vmasize)
				update_vma(nsbase, nssize);
			hpanel->request_paint();
		});
	}
	void do_hex(int hex)
	{
		if(hpanel->seloff > hpanel->vmasize)
			return;
		if(hex_input_state < 0)
			hex_input_state = hex;
		else {
			uint8_t byte = hex_input_state * 16 + hex;
			uint64_t addr = hpanel->vmabase + hpanel->seloff;
			hex_input_state = -1;
			if(hpanel->seloff + 1 < hpanel->vmasize)
				hpanel->seloff++;
			inst.iqueue->run([addr, byte]() {
				CORE().memory->write<uint8_t>(addr, byte);
			});
		}
		hpanel->request_paint();
	}
	class _panel : public text_framebuffer_panel
	{
	public:
		_panel(wxeditor_hexedit* parent, emulator_instance& _inst)
			: text_framebuffer_panel(parent, 59, lines = 28, wxID_ANY, NULL), inst(_inst)
		{
			CHECK_UI_THREAD;
			rparent = parent;
			vmabase = 0;
			vmasize = 0;
			offset = 0;
			seloff = 0;
			memset(value, 0, maxvaluelen);
			clear();
			Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxeditor_hexedit::on_mouse), NULL, parent);
			Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_hexedit::on_mouse), NULL, parent);
			Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(wxeditor_hexedit::on_mouse), NULL, parent);
			Connect(wxEVT_CHAR, wxKeyEventHandler(wxeditor_hexedit::on_keyboard), NULL, parent);
			request_paint();
		}
		void prepare_paint()
		{
			CHECK_UI_THREAD;
			uint64_t paint_offset = static_cast<uint64_t>(offset) * 16;
			uint64_t _vmabase = vmabase;
			uint64_t _vmasize = vmasize;
			uint64_t _seloff = seloff;
			int _lines = lines;
			uint8_t* _value = value;
			inst.iqueue->run([_vmabase, _vmasize, paint_offset, _seloff, _value, _lines,
				this]() {
				memory_search* memsearch = wxwindow_memorysearch_active(inst);
				//Paint the stuff
				for(ssize_t j = 0; j < _lines; j++) {
					uint64_t addr = paint_offset + j * 16;
					if(addr >= _vmasize) {
						//Past-the-end.
						for(size_t i = 0; i < get_characters().first; i++)
							write(" ", 1, i, j, 0, 0xFFFFFF);
						continue;
					}
					for(size_t i = 0; i < sizeof(separators)/sizeof(separators[0]); i++) {
						write(sepchars[i], 1, separators[i], j, 0, 0xFFFFFF);
					}
					for(size_t i = 0; i < hexaddr; i++) {
						write(hexes[(addr >> 4 * (hexaddr - i - 1)) & 15], 1, i, j, 0,
							0xFFFFFF);
					}
					size_t bytes = 16;
					if(_vmasize - addr < 16)
						bytes = _vmasize - addr;
					uint64_t laddr = addr + _vmabase;
					for(size_t i = 0; i < bytes; i++) {
						uint32_t fg = 0;
						uint32_t bg = 0xFFFFFF;
						bool candidate = (memsearch && memsearch->is_candidate(laddr + i));
						if(candidate) bg = (bg & 0xC0C0C0) | 0x3F0000;
						if(addr + i == _seloff)
							std::swap(fg, bg);
						uint8_t b = inst.memory->read<uint8_t>(laddr + i);
						if(rparent->hex_input_state < 0 || addr + i != seloff
						)
							write(hexes[(b >> 4) & 15], 1, hexcol[i], j, fg, bg);
						else
							write(hexes[rparent->hex_input_state], 1, hexcol[i], j, 0xFF,
								0);
						write(hexes[b & 15], 1, hexcol[i] + 1, j, fg, bg);
						char32_t buf[2] = {0, 0};
						buf[0] = byte_to_char(b);
						write(buf, 1, charcol[i], j, fg, bg);
					}
					for(size_t i = bytes; i < 16; i++) {
						write("  ", 2, hexcol[i], j, 0, 0xFFFFFF);
						write(" ", 1, charcol[i] + 1, j, 0, 0xFFFFFF);
					}
				}
				memset(_value, 0, maxvaluelen);
				inst.memory->read_range(_vmabase + _seloff, _value, maxvaluelen);
			});
			rparent->refresh_curvalue();
			rparent->set_search_status();
		}
		char32_t byte_to_char(uint8_t ch)
		{
			if(ch == 160)
				return U' ';
			if((ch & 0x60) == 0 || ch == 127 || ch == 0xad)
				return U'.';
			return ch;
		}
		void on_mouse0(int x, int y, bool polarity)
		{
			CHECK_UI_THREAD;
			if(!polarity)
				return;
			uint64_t rowaddr = 16 * (static_cast<uint64_t>(offset) + y);
			int coladdr = 16;
			for(unsigned i = 0; i < 16; i++)
				if(x == hexcol[i] || x == hexcol[i] + 1 || x == charcol[i])
					coladdr = i;
			if(rowaddr + coladdr >= vmasize || coladdr > 15)
				return;
			seloff = rowaddr + coladdr;
			request_paint();
		}
		emulator_instance& inst;
		wxeditor_hexedit* rparent;
		int offset;
		uint64_t vmabase;
		uint64_t vmasize;
		uint64_t seloff;
		uint8_t value[maxvaluelen];
		int lines;
	};
private:
	struct bookmark_entry
	{
		std::string name;
		std::string vma;
		int scroll;
		uint64_t sel;
	};
	emulator_instance& inst;
	wxMenu* regionmenu;
	wxMenu* bookmarkmenu;
	wxMenu* searchmenu;
	wxComboBox* datatype;
	wxMenuItem* oendian;
	wxStatusBar* statusbar;
	wxMenuBar* menubar;
	scroll_bar* scroll;
	_panel* hpanel;
	wxMenu* valuemenu;
	wxMenu* typemenu;
	struct dispatch::target<bool> corechange;
	unsigned current_vma;
	std::vector<std::string> vma_names;
	std::map<unsigned, bool> vma_endians;
	std::vector<bookmark_entry> bookmarks;
	bool destructing;
	unsigned curtype;
	bool littleendian;
	int hex_input_state;
};

void wxeditor_hexedit_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	auto e = editor.lookup(inst);
	if(e) {
		e->Raise();
		return;
	}
	try {
		editor.create(inst, parent)->Show();
	} catch(...) {
	}
}

void wxeditor_hexeditor_update(emulator_instance& inst)
{
	CHECK_UI_THREAD;
	auto e = editor.lookup(inst);
	if(e) e->updated();
}

bool wxeditor_hexeditor_available(emulator_instance& inst)
{
	return editor.exists(inst);
}

bool wxeditor_hexeditor_jumpto(emulator_instance& inst, uint64_t addr)
{
	CHECK_UI_THREAD;
	auto e = editor.lookup(inst);
	if(e) {
		e->jumpto(addr);
		return true;
	} else
		return false;
}
