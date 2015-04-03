#include <wx/wx.h>
#include <wx/statline.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>

#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/memorywatch.hpp"
#include "core/memorymanip.hpp"
#include "core/ui-services.hpp"
#include "library/memoryspace.hpp"
#include "core/project.hpp"

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"

#include "library/string.hpp"
#include "library/hex.hpp"
#include "interface/romtype.hpp"

/*

	std::string main_expr;
	std::string format;
	unsigned memread_bytes;			//0 if not memory read.
	bool memread_signed_flag;
	bool memread_float_flag;
	int memread_endianess;
	uint64_t memread_scale_div;
	uint64_t memread_addr_base;
	uint64_t memread_addr_size;
	enum position_category {
		PC_DISABLED,
		PC_MEMORYWATCH,
		PC_ONSCREEN
	} position;
	std::string enabled;			//Ignored for disabled.
	std::string onscreen_xpos;
	std::string onscreen_ypos;
	bool onscreen_alt_origin_x;
	bool onscreen_alt_origin_y;
	bool onscreen_cliprange_x;
	bool onscreen_cliprange_y;
	std::string onscreen_font;		//"" is system default.
	int64_t onscreen_fg_color;
	int64_t onscreen_bg_color;
	int64_t onscreen_halo_color;

*/

namespace
{
	int log2i(uint64_t v)
	{
		unsigned l = 0;
		while(v > 1) {
			v >>= 1;
			l++;
		}
		return l;
	}

	template<class T>
	class label_control
	{
	public:
		label_control()
		{
			lbl = NULL;
			ctrl = NULL;
		}
		template<typename... U> label_control(wxWindow* parent, const std::string& label, U... args)
		{
			CHECK_UI_THREAD;
			lbl = new wxStaticText(parent, wxID_ANY, towxstring(label));
			ctrl = new T(parent, args...);
		}
		void show(bool state)
		{
			CHECK_UI_THREAD;
			lbl->Show(state);
			ctrl->Show(state);
		}
		void enable(bool state)
		{
			CHECK_UI_THREAD;
			lbl->Enable(state);
			ctrl->Enable(state);
		}
		void add(wxSizer* s, bool prop = false)
		{
			CHECK_UI_THREAD;
			s->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);
			s->Add(ctrl, prop ? 1 : 0, wxGROW);
		}
		void add_cb(std::function<void(wxStaticText* l, T* c)> cb)
		{
			cb(lbl, ctrl);
		}
		wxStaticText* label() { return lbl; }
		T* operator->() { return ctrl; }
	private:
		wxStaticText* lbl;
		T* ctrl;
	};

	std::string format_color(int64_t x)
	{
		return framebuffer::color::stringify(x);
	}

	int64_t get_color(std::string x)
	{
		return framebuffer::color(x).asnumber();
	}
}

class wxeditor_memorywatch : public wxDialog
{
public:
	wxeditor_memorywatch(wxWindow* parent, emulator_instance& _inst, const std::string& name);
	bool ShouldPreventAppExit() const;
	void on_position_change(wxCommandEvent& e);
	void on_fontsel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void enable_condenable2(wxCommandEvent& e);
private:
	void enable_for_pos(memwatch_printer::position_category p);
	void enable_for_addr(bool is_addr);
	void enable_for_vma(bool free, uint64_t _base, uint64_t _size);
	void enable_condenable();
	memwatch_printer::position_category get_poscategory();
	emulator_instance& inst;
	label_control<wxComboBox> type;
	label_control<wxTextCtrl> expr;
	label_control<wxTextCtrl> format;
	label_control<wxComboBox> endianess;
	label_control<wxSpinCtrl> scale;
	label_control<wxComboBox> vma;
	label_control<wxTextCtrl> addrbase;
	label_control<wxTextCtrl> addrsize;
	label_control<wxComboBox> position;
	wxCheckBox* cond_enable;
	wxTextCtrl* enabled;
	label_control<wxTextCtrl> xpos;
	label_control<wxTextCtrl> ypos;
	wxCheckBox* alt_origin_x;
	wxCheckBox* alt_origin_y;
	wxCheckBox* cliprange_x;
	wxCheckBox* cliprange_y;
	label_control<wxTextCtrl> font;
	wxButton* font_sel;
	label_control<wxTextCtrl> fg_color;
	label_control<wxTextCtrl> bg_color;
	label_control<wxTextCtrl> halo_color;
	wxButton* ok;
	wxButton* cancel;
	std::string name;
	std::string old_addrbase;
	std::string old_addrsize;
	bool was_free;
	std::map<int, std::pair<uint64_t, uint64_t>> vmas_available;
};

wxeditor_memorywatch::wxeditor_memorywatch(wxWindow* parent, emulator_instance& _inst, const std::string& _name)
	: wxDialog(parent, wxID_ANY, towxstring("Edit memory watch '" + _name + "'")), inst(_inst), name(_name)
{
	CHECK_UI_THREAD;
	Center();
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	//Type.
	wxSizer* s1 = new wxBoxSizer(wxHORIZONTAL);
	type = label_control<wxComboBox>(this, "Data type", wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0,
		nullptr, wxCB_READONLY);
	type.add(s1);
	type->Append(wxT("(Expression)"));
	type->Append(wxT("Signed byte"));
	type->Append(wxT("Unsigned byte"));
	type->Append(wxT("Signed word"));
	type->Append(wxT("Unsigned word"));
	type->Append(wxT("Signed 3-byte"));
	type->Append(wxT("Unsigned 3-byte"));
	type->Append(wxT("Signed dword"));
	type->Append(wxT("Unsigned dword"));
	type->Append(wxT("Float"));
	type->Append(wxT("Signed qword"));
	type->Append(wxT("Unsigned qword"));
	type->Append(wxT("Double"));
	type->SetSelection(3);
	type->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxeditor_memorywatch::on_position_change), NULL, this);
	endianess = label_control<wxComboBox>(this, "Endian:", wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		0, nullptr, wxCB_READONLY);
	endianess.add(s1, true);
	endianess->Append(wxT("Little"));
	endianess->Append(wxT("Host"));
	endianess->Append(wxT("Big"));
	endianess->SetSelection(0);
	scale = label_control<wxSpinCtrl>(this, "Scale bits:", wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxSP_ARROW_KEYS, 0, 63, 0);
	scale.add(s1);
	top_s->Add(s1, 1, wxGROW);

	//Memory range.
	wxSizer* s5 = new wxBoxSizer(wxHORIZONTAL);
	vma = label_control<wxComboBox>(this, "Memory:", wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, 0,
		nullptr, wxCB_READONLY);
	vma.add(s5, true);
	vma->Append(wxT("(All)"));
	auto i = inst.memory->get_regions();
	for(auto j : i) {
		int id = vma->GetCount();
		vma->Append(towxstring(j->name));
		vmas_available[id] = std::make_pair(j->base, j->size);
	}
	//Special registers "VMA".
	{
		int id = vma->GetCount();
		vma->Append(towxstring("(registers)"));
		vmas_available[id] = std::make_pair(0xFFFFFFFFFFFFFFFFULL, 0);
	}
	vma->SetSelection(0);
	vma->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxeditor_memorywatch::on_position_change), NULL, this);
	addrbase = label_control<wxTextCtrl>(this, "Base:", wxID_ANY, wxT("0"), wxDefaultPosition, wxSize(100, -1));
	addrbase.add(s5, true);
	addrsize = label_control<wxTextCtrl>(this, "Size:", wxID_ANY, wxT("0"), wxDefaultPosition, wxSize(100, -1));
	addrsize.add(s5, true);
	top_s->Add(s5, 1, wxGROW);

	//Expression.
	wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
	expr = label_control<wxTextCtrl>(this, "Address:", wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1));
	expr.add(s2, true);
	top_s->Add(s2, 1, wxGROW);

	//Format:
	wxSizer* s3 = new wxBoxSizer(wxHORIZONTAL);
	format = label_control<wxTextCtrl>(this, "Format:", wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1));
	format.add(s3, true);
	top_s->Add(s3, 1, wxGROW);

	wxSizer* sx = new wxBoxSizer(wxVERTICAL);
	sx->Add(1, 11);
	top_s->Add(sx, 0, wxGROW);

	wxSizer* s6 = new wxBoxSizer(wxHORIZONTAL);
	position = label_control<wxComboBox>(this, "Position: ", wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		0, nullptr, wxCB_READONLY);
	position.add(s6, true);
	position->Append(wxT("Disabled"));
	position->Append(wxT("Memory watch"));
	position->Append(wxT("On screen"));
	position->SetSelection(1);
	position->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxeditor_memorywatch::on_position_change), NULL, this);
	top_s->Add(s6, 0, wxGROW);

	wxSizer* s7 = new wxBoxSizer(wxHORIZONTAL);
	s7->Add(cond_enable = new wxCheckBox(this, wxID_ANY, wxT("Conditional on: ")), 0, wxGROW);
	s7->Add(enabled = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1, wxGROW);
	cond_enable->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::enable_condenable2), NULL, this);
	top_s->Add(s7, 0, wxGROW);

	wxSizer* s8 = new wxBoxSizer(wxHORIZONTAL);

	xpos = label_control<wxTextCtrl>(this, "X:", wxID_ANY, wxT(""), wxDefaultPosition, wxSize(300, -1));
	xpos.add(s8, true);
	s8->Add(alt_origin_x = new wxCheckBox(this, wxID_ANY, wxT("Alt. origin")), 0, wxGROW);
	s8->Add(cliprange_x = new wxCheckBox(this, wxID_ANY, wxT("Clip range")), 0, wxGROW);
	top_s->Add(s8, 0, wxGROW);

	wxSizer* s9 = new wxBoxSizer(wxHORIZONTAL);
	ypos = label_control<wxTextCtrl>(this, "Y:", wxID_ANY, wxT(""), wxDefaultPosition, wxSize(300, -1));
	ypos.add(s9, true);
	s9->Add(alt_origin_y = new wxCheckBox(this, wxID_ANY, wxT("Alt. origin")), 0, wxGROW);
	s9->Add(cliprange_y = new wxCheckBox(this, wxID_ANY, wxT("Clip range")), 0, wxGROW);
	top_s->Add(s9, 0, wxGROW);

	wxSizer* s10 = new wxBoxSizer(wxHORIZONTAL);
	font = label_control<wxTextCtrl>(this, "Font:", wxID_ANY, wxT(""), wxDefaultPosition, wxSize(300, -1));
	font.add(s10, true);
	s10->Add(font_sel = new wxButton(this, wxID_ANY, wxT("...")), 0, wxGROW);
	font_sel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::on_fontsel), NULL, this);
	top_s->Add(s10, 0, wxGROW);

	wxSizer* s11 = new wxBoxSizer(wxHORIZONTAL);
	fg_color = label_control<wxTextCtrl>(this, "Foreground:", wxID_ANY, wxT("#FFFFFF"),
		wxDefaultPosition, wxSize(100, -1));
	fg_color.add(s11, true);
	bg_color = label_control<wxTextCtrl>(this, "Background:", wxID_ANY, wxT("transparent"), wxDefaultPosition,
		wxSize(100, -1));
	bg_color.add(s11, true);
	halo_color = label_control<wxTextCtrl>(this, "Halo:", wxID_ANY, wxT("transparent"), wxDefaultPosition,
		wxSize(100, -1));
	halo_color.add(s11, true);
	top_s->Add(s11, 0, wxGROW);

	wxSizer* s12 = new wxBoxSizer(wxHORIZONTAL);
	s12->AddStretchSpacer();
	s12->Add(ok = new wxButton(this, wxID_ANY, wxT("Ok")), 0, wxGROW);
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_memorywatch::on_ok), NULL, this);
	s12->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")), 0, wxGROW);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_memorywatch::on_cancel), NULL,
		this);
	top_s->Add(s12, 0, wxGROW);

	memwatch_item it;
	bool had_it = false;
	try {
		it = inst.mwatch->get(name);
		had_it = true;
	} catch(...) {
	}
	if(had_it) {
		expr->SetValue(towxstring(it.expr));
		format->SetValue(towxstring(it.format));
		cond_enable->SetValue(it.printer.cond_enable);
		enabled->SetValue(towxstring(it.printer.enabled));
		xpos->SetValue(towxstring(it.printer.onscreen_xpos));
		ypos->SetValue(towxstring(it.printer.onscreen_ypos));
		alt_origin_x->SetValue(it.printer.onscreen_alt_origin_x);
		alt_origin_y->SetValue(it.printer.onscreen_alt_origin_y);
		cliprange_x->SetValue(it.printer.onscreen_cliprange_x);
		cliprange_y->SetValue(it.printer.onscreen_cliprange_y);
		endianess->SetSelection(it.endianess + 1);
		addrbase->SetValue(towxstring(hex::to<uint64_t>(it.addr_base)));
		addrsize->SetValue(towxstring(hex::to<uint64_t>(it.addr_size)));
		if(it.printer.position == memwatch_printer::PC_DISABLED) position->SetSelection(0);
		if(it.printer.position == memwatch_printer::PC_MEMORYWATCH) position->SetSelection(1);
		if(it.printer.position == memwatch_printer::PC_ONSCREEN) position->SetSelection(2);
		switch(it.bytes) {
		case 0: type->SetSelection(0); break;
		case 1: type->SetSelection(it.signed_flag ? 1 : 2); break;
		case 2: type->SetSelection(it.signed_flag ? 3 : 4); break;
		case 3: type->SetSelection(it.signed_flag ? 5 : 6); break;
		case 4: type->SetSelection(it.float_flag ? 9 : (it.signed_flag ? 7 : 8)); break;
		case 8: type->SetSelection(it.float_flag ? 12 : (it.signed_flag ? 11 : 10)); break;
		}
		scale->SetValue(log2i(it.scale_div));
		for(auto j : vmas_available) {
			if(j.second.first == it.addr_base && j.second.second == it.addr_size)
				vma->SetSelection(j.first);
		}
		font->SetValue(towxstring(it.printer.onscreen_font));
		fg_color->SetValue(towxstring(format_color(it.printer.onscreen_fg_color)));
		bg_color->SetValue(towxstring(format_color(it.printer.onscreen_bg_color)));
		halo_color->SetValue(towxstring(format_color(it.printer.onscreen_halo_color)));
	}

	wxCommandEvent e;
	on_position_change(e);
	top_s->SetSizeHints(this);
	Fit();
}

bool wxeditor_memorywatch::ShouldPreventAppExit() const
{
	return false;
}

memwatch_printer::position_category wxeditor_memorywatch::get_poscategory()
{
	if(position->GetSelection() == 0) return memwatch_printer::PC_DISABLED;
	if(position->GetSelection() == 1) return memwatch_printer::PC_MEMORYWATCH;
	if(position->GetSelection() == 2) return memwatch_printer::PC_ONSCREEN;
	return memwatch_printer::PC_DISABLED; //NOTREACHED.
}

void wxeditor_memorywatch::enable_for_pos(memwatch_printer::position_category p)
{
	CHECK_UI_THREAD;
	bool full_disable = (p == memwatch_printer::PC_DISABLED);
	cond_enable->Enable(!full_disable);
	enabled->Enable(cond_enable->GetValue() && !full_disable);
	xpos.enable(p == memwatch_printer::PC_ONSCREEN);
	ypos.enable(p == memwatch_printer::PC_ONSCREEN);
	alt_origin_x->Enable(p == memwatch_printer::PC_ONSCREEN);
	alt_origin_y->Enable(p == memwatch_printer::PC_ONSCREEN);
	cliprange_x->Enable(p == memwatch_printer::PC_ONSCREEN);
	cliprange_y->Enable(p == memwatch_printer::PC_ONSCREEN);
	font.enable(p == memwatch_printer::PC_ONSCREEN);
	font_sel->Enable(p == memwatch_printer::PC_ONSCREEN);
	fg_color.enable(p == memwatch_printer::PC_ONSCREEN);
	bg_color.enable(p == memwatch_printer::PC_ONSCREEN);
	halo_color.enable(p == memwatch_printer::PC_ONSCREEN);
}

void wxeditor_memorywatch::enable_condenable()
{
	CHECK_UI_THREAD;
	memwatch_printer::position_category p = get_poscategory();
	bool full_disable = (p == memwatch_printer::PC_DISABLED);
	enabled->Enable(cond_enable->GetValue() && !full_disable);
}

void wxeditor_memorywatch::enable_condenable2(wxCommandEvent& e)
{
	enable_condenable();
}

void wxeditor_memorywatch::enable_for_addr(bool is_addr)
{
	CHECK_UI_THREAD;
	expr.label()->SetLabel(towxstring(is_addr ? "Address:" : "Expr:"));
	endianess.enable(is_addr);
	scale.enable(is_addr);
	vma.enable(is_addr);
	addrbase.enable(is_addr && was_free);
	addrsize.enable(is_addr && was_free);
	Fit();
}

void wxeditor_memorywatch::enable_for_vma(bool free, uint64_t _base, uint64_t _size)
{
	CHECK_UI_THREAD;
	//TODO: Set default endian.
	if(!free && !was_free) {
		addrbase->SetValue(towxstring((stringfmt() << std::hex << _base).str()));
		addrsize->SetValue(towxstring((stringfmt() << std::hex << _size).str()));
	} else if(free && !was_free) {
		addrbase->SetValue(towxstring(old_addrbase));
		addrsize->SetValue(towxstring(old_addrsize));
		addrbase.enable(true);
		addrsize.enable(true);
	} else if(!free && was_free) {
		old_addrbase = tostdstring(addrbase->GetValue());
		old_addrsize = tostdstring(addrsize->GetValue());
		addrbase->SetValue(towxstring((stringfmt() << std::hex << _base).str()));
		addrsize->SetValue(towxstring((stringfmt() << std::hex << _size).str()));
		addrbase.enable(false);
		addrsize.enable(false);
	}
	was_free = free;
}

void wxeditor_memorywatch::on_position_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	enable_for_pos(get_poscategory());
	int vmasel = vma->GetSelection();
	if(vmasel == 0)
		enable_for_vma(true, 0, 0);
	else if(vmas_available.count(vmasel)) {
		enable_for_vma(false, vmas_available[vmasel].first, vmas_available[vmasel].second);
	} else {
		vma->SetSelection(0);
		enable_for_vma(true, 0, 0);
	}
	enable_for_addr(type->GetSelection() != 0);
}

void wxeditor_memorywatch::on_fontsel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string filename = choose_file_load(this, "Choose font file", UI_get_project_otherpath(inst),
			filetype_font);
		font->SetValue(towxstring(filename));
	} catch(canceled_exception& e) {
	}
}

void wxeditor_memorywatch::on_ok(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	memwatch_item it;
	it.expr = tostdstring(expr->GetValue());
	it.format = tostdstring(format->GetValue());
	it.printer.cond_enable = cond_enable->GetValue();
	it.printer.enabled = tostdstring(enabled->GetValue());
	it.printer.onscreen_xpos = tostdstring(xpos->GetValue());
	it.printer.onscreen_ypos = tostdstring(ypos->GetValue());
	it.printer.onscreen_alt_origin_x = alt_origin_x->GetValue();
	it.printer.onscreen_alt_origin_y = alt_origin_y->GetValue();
	it.printer.onscreen_cliprange_x = cliprange_x->GetValue();
	it.printer.onscreen_cliprange_y = cliprange_y->GetValue();
	it.printer.onscreen_font = tostdstring(font->GetValue());
	it.endianess = endianess->GetSelection() - 1;
	try {
		it.addr_base = hex::from<uint64_t>(tostdstring(addrbase->GetValue()));
		it.addr_size = hex::from<uint64_t>(tostdstring(addrsize->GetValue()));
	} catch(std::exception& e) {
		show_message_ok(NULL, "Bad memory range", std::string("Error parsing memory range: ") + e.what(),
			wxICON_EXCLAMATION);
		return;
	}
	if(position->GetSelection() == 0) it.printer.position = memwatch_printer::PC_DISABLED;
	else if(position->GetSelection() == 1) it.printer.position = memwatch_printer::PC_MEMORYWATCH;
	else if(position->GetSelection() == 2) it.printer.position = memwatch_printer::PC_ONSCREEN;
	else it.printer.position = memwatch_printer::PC_MEMORYWATCH;
	it.signed_flag = false;
	it.float_flag = false;
	switch(type->GetSelection()) {
	case 0: it.bytes = 0; break;
	case 1: it.bytes = 1; it.signed_flag = !false; break;
	case 2: it.bytes = 1; it.signed_flag = !true; break;
	case 3: it.bytes = 2; it.signed_flag = !false; break;
	case 4: it.bytes = 2; it.signed_flag = !true; break;
	case 5: it.bytes = 3; it.signed_flag = !false; break;
	case 6: it.bytes = 3; it.signed_flag = !true; break;
	case 7: it.bytes = 4; it.signed_flag = !false; break;
	case 8: it.bytes = 4; it.signed_flag = !true; break;
	case 9: it.bytes = 4; it.float_flag = true; break;
	case 10: it.bytes = 8; it.signed_flag = !false; break;
	case 11: it.bytes = 8; it.signed_flag = !true; break;
	case 12: it.bytes = 8; it.float_flag = true; break;
	};
	it.scale_div = 1ULL << scale->GetValue();
	try {
		it.printer.onscreen_fg_color = get_color(tostdstring(fg_color->GetValue()));
		it.printer.onscreen_bg_color = get_color(tostdstring(bg_color->GetValue()));
		it.printer.onscreen_halo_color = get_color(tostdstring(halo_color->GetValue()));
	} catch(std::exception& e) {
		show_message_ok(NULL, "Bad colors", std::string("Error parsing colors: ") + e.what(),
			wxICON_EXCLAMATION);
		return;
	}
	try {
		inst.iqueue->run([this, &it]() {
			CORE().mwatch->set(name, it);
		});
	} catch(std::exception& e) {
		show_exception(NULL, "Bad values", "Error setting memory watch", e);
		return;
	}
	EndModal(wxID_OK);
}

void wxeditor_memorywatch::on_cancel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}

class wxeditor_memorywatches : public wxDialog
{
public:
	wxeditor_memorywatches(wxWindow* parent, emulator_instance& _inst);
	bool ShouldPreventAppExit() const;
	void on_memorywatch_change(wxCommandEvent& e);
	void on_new(wxCommandEvent& e);
	void on_rename(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
private:
	void refresh();
	emulator_instance& inst;
	//TODO: Make this a wxGrid.
	wxListBox* watches;
	wxButton* newbutton;
	wxButton* renamebutton;
	wxButton* deletebutton;
	wxButton* editbutton;
	wxButton* closebutton;
};


wxeditor_memorywatches::wxeditor_memorywatches(wxWindow* parent, emulator_instance& _inst)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit memory watches"), wxDefaultPosition, wxSize(-1, -1)),
	inst(_inst)
{
	CHECK_UI_THREAD;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(watches = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 300)), 1, wxGROW);
	watches->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_memorywatches::on_memorywatch_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(newbutton = new wxButton(this, wxID_ANY, wxT("New")), 0, wxGROW);
	pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
	pbutton_s->Add(renamebutton = new wxButton(this, wxID_ANY, wxT("Rename")), 0, wxGROW);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	pbutton_s->Add(closebutton = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
	newbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatches::on_new), NULL, this);
	editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatches::on_edit), NULL, this);
	renamebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatches::on_rename), NULL, this);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatches::on_delete), NULL, this);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatches::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	refresh();
}

bool wxeditor_memorywatches::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_memorywatches::on_memorywatch_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::string watch = tostdstring(watches->GetStringSelection());
	editbutton->Enable(watch != "");
	deletebutton->Enable(watch != "");
	renamebutton->Enable(watch != "");
}

void wxeditor_memorywatches::on_new(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string newname = pick_text(this, "New watch", "Enter name for watch:");
		if(newname == "")
			return;
		wxeditor_memorywatch* nwch = new wxeditor_memorywatch(this, inst, newname);
		nwch->ShowModal();
		nwch->Destroy();
		refresh();
	} catch(canceled_exception& e) {
		//Ignore.
	}
	on_memorywatch_change(e);
}

void wxeditor_memorywatches::on_rename(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::string watch = tostdstring(watches->GetStringSelection());
	if(watch == "")
		return;
	try {
		bool exists = false;
		std::string newname = pick_text(this, "Rename watch", "Enter New name for watch:");
		inst.iqueue->run([watch, newname, &exists]() {
			exists = !CORE().mwatch->rename(watch, newname);
		});
		if(exists)
			show_message_ok(this, "Error", "The target watch already exists", wxICON_EXCLAMATION);
		refresh();
	} catch(canceled_exception& e) {
		//Ignore.
	}
	on_memorywatch_change(e);
}

void wxeditor_memorywatches::on_delete(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::string watch = tostdstring(watches->GetStringSelection());
	if(watch != "")
		inst.iqueue->run([watch]() { CORE().mwatch->clear(watch); });
	refresh();
	on_memorywatch_change(e);
}

void wxeditor_memorywatches::on_edit(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string watch = tostdstring(watches->GetStringSelection());
		if(watch == "")
			return;
		std::string wtxt;
		wxeditor_memorywatch* ewch = new wxeditor_memorywatch(this, inst, watch);
		ewch->ShowModal();
		ewch->Destroy();
		refresh();
	} catch(canceled_exception& e) {
		//Ignore.
	}
	on_memorywatch_change(e);
}

void wxeditor_memorywatches::on_close(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_OK);
}

void wxeditor_memorywatches::refresh()
{
	CHECK_UI_THREAD;
	std::set<std::string> bind;
	inst.iqueue->run([&bind]() {
		bind = CORE().mwatch->enumerate();
	});
	watches->Clear();
	for(auto i : bind)
		watches->Append(towxstring(i));
	if(watches->GetCount())
		watches->SetSelection(0);
	wxCommandEvent e;
	on_memorywatch_change(e);
}

void wxeditor_memorywatches_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_memorywatches(parent, inst);
		editor->ShowModal();
	} catch(...) {
		return;
	}
	editor->Destroy();
}
