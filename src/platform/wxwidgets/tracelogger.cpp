#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "platform/wxwidgets/scrollbar.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include "core/command.hpp"
#include "core/debug.hpp"
#include "core/instance.hpp"
#include "core/mainloop.hpp"
#include "core/memorymanip.hpp"
#include "core/project.hpp"
#include "core/ui-services.hpp"
#include "interface/disassembler.hpp"
#include "library/minmax.hpp"
#include "library/hex.hpp"
#include "library/memoryspace.hpp"
#include "library/serialization.hpp"
#include <wx/frame.h>
#include <wx/clipbrd.h>
#include <wx/msgdlg.h>
#include <wx/menu.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/statusbr.h>
#include <wx/dataobj.h>
#include <wx/sizer.h>
#include <set>

namespace
{
	enum
	{
		wxID_FIND_NEXT = wxID_HIGHEST + 1,
		wxID_FIND_PREV,
		wxID_GOTO,
		wxID_DISASM,
		wxID_DISASM_MORE,
		wxID_SINGLESTEP,
		wxID_BREAKPOINTS,
		wxID_CONTINUE,
		wxID_FRAMEADVANCE,
		wxID_CLEAR,
	};

	int prompt_for_save(wxWindow* parent, const std::string& what)
	{
		CHECK_UI_THREAD;
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(what + " has unsaved changes, "
			"save before closing?"), towxstring("Save on exit?"), wxCENTER | wxYES_NO | wxCANCEL |
			wxYES_DEFAULT);
		d->SetYesNoCancelLabels(wxT("Save"), wxT("Discard"), wxT("Cancel"));
		int r = d->ShowModal();
		d->Destroy();
		if(r == wxID_YES) return 1;
		if(r == wxID_NO) return 0;
		if(r == wxID_CANCEL) return -1;
		return -1;
	}

	class dialog_find : public wxDialog
	{
	public:
		dialog_find(wxWindow* parent);
		std::string get_pattern();
		void on_ok(wxCommandEvent& e) { EndModal(wxID_OK); }
		void on_cancel(wxCommandEvent& e) { EndModal(wxID_CANCEL); }
	private:
		wxTextCtrl* text;
		wxComboBox* type;
		wxButton* ok;
		wxButton* cancel;
	};

	dialog_find::dialog_find(wxWindow* parent)
		: wxDialog(parent, wxID_ANY, wxT("Find"))
	{
		CHECK_UI_THREAD;
		wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);
		wxBoxSizer* t_s = new wxBoxSizer(wxHORIZONTAL);
		t_s->Add(type = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			0, NULL, wxCB_READONLY), 1, wxGROW);
		type->Append(towxstring("Literal"));
		type->Append(towxstring("Wildcards"));
		type->Append(towxstring("Regexp"));
		type->SetSelection(0);
		t_s->Add(text = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(350, -1),
			wxTE_PROCESS_ENTER), 0, wxGROW);
		top_s->Add(t_s, 1, wxGROW);
		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("OK")));
		pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")));
		text->Connect(wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(dialog_find::on_ok), NULL, this);
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_find::on_ok), NULL, this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_find::on_cancel), NULL,
			this);
		top_s->Add(pbutton_s, 0, wxGROW);
		top_s->SetSizeHints(this);
		Fit();
	}

	std::string dialog_find::get_pattern()
	{
		CHECK_UI_THREAD;
		if(tostdstring(text->GetValue()) == "")
			return "";
		if(type->GetSelection() == 2)
			return "R" + tostdstring(text->GetValue());
		else if(type->GetSelection() == 1)
			return "W" + tostdstring(text->GetValue());
		else
			return "F" + tostdstring(text->GetValue());
	}

	class dialog_disassemble : public wxDialog
	{
	public:
		dialog_disassemble(wxWindow* parent, emulator_instance& _inst);
		dialog_disassemble(wxWindow* parent, emulator_instance& _inst, uint64_t dflt_base,
			const std::string& dflt_lang);
		std::string get_disassembler();
		uint64_t get_address();
		uint64_t get_count();
		void on_change(wxCommandEvent& e);
		void on_ok(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e) { EndModal(wxID_CANCEL); }
	private:
		void init(bool spec, uint64_t dflt_base, std::string dflt_lang);
		emulator_instance& inst;
		wxComboBox* type;
		wxComboBox* endian;
		wxComboBox* vma;
		wxTextCtrl* address;
		wxSpinCtrl* count;
		wxButton* ok;
		wxButton* cancel;
		bool has_default;
		unsigned code_types;
		static std::string old_dflt_lang;
		static uint64_t old_dflt_base;
	};

	std::string dialog_disassemble::old_dflt_lang;
	uint64_t dialog_disassemble::old_dflt_base;

	dialog_disassemble::dialog_disassemble(wxWindow* parent, emulator_instance& _inst)
		: wxDialog(parent, wxID_ANY, wxT("Disassemble region")), inst(_inst)
	{
		CHECK_UI_THREAD;
		init(false, 0, "");
	}

	dialog_disassemble::dialog_disassemble(wxWindow* parent, emulator_instance& _inst, uint64_t dflt_base,
		const std::string& dflt_lang)
		: wxDialog(parent, wxID_ANY, wxT("Disassemble region")), inst(_inst)
	{
		CHECK_UI_THREAD;
		init(true, dflt_base, dflt_lang);
	}

	void dialog_disassemble::init(bool spec, uint64_t dflt_base, std::string dflt_lang)
	{
		CHECK_UI_THREAD;
		std::map<std::string, std::pair<uint64_t, uint64_t>> regions;
		std::set<std::string> disasms;
		inst.iqueue->run([&regions, &disasms]() {
			for(auto i : CORE().memory->get_regions())
				regions[i->name] = std::make_pair(i->base, i->size);
			disasms = disassembler::list();
		});

		wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		wxBoxSizer* type_s = new wxBoxSizer(wxHORIZONTAL);
		type_s->Add(new wxStaticText(this, wxID_ANY, wxT("Language:")), 0, wxGROW);
		type_s->Add(type = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			0, NULL, wxCB_READONLY), 0, wxGROW);
		for(auto& i : disasms)
			type->Append(towxstring(i));
		code_types = type->GetCount();
		type->Append(towxstring("Data (signed byte)"));
		type->Append(towxstring("Data (unsigned byte)"));
		type->Append(towxstring("Data (hex byte)"));
		type->Append(towxstring("Data (signed word)"));
		type->Append(towxstring("Data (unsigned word)"));
		type->Append(towxstring("Data (hex word)"));
		type->Append(towxstring("Data (signed onehalfword)"));
		type->Append(towxstring("Data (unsigned onehalfword)"));
		type->Append(towxstring("Data (hex onehalfword)"));
		type->Append(towxstring("Data (signed doubleword)"));
		type->Append(towxstring("Data (unsigned doubleword)"));
		type->Append(towxstring("Data (hex doubleword)"));
		type->Append(towxstring("Data (signed quadword)"));
		type->Append(towxstring("Data (unsigned quadword)"));
		type->Append(towxstring("Data (hex quadword)"));
		type->Append(towxstring("Data (float)"));
		type->Append(towxstring("Data (double)"));
		type->SetSelection(0);
		type->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(dialog_disassemble::on_change),
			NULL, this);
		top_s->Add(type_s, 0, wxGROW);

		wxBoxSizer* endian_s = new wxBoxSizer(wxHORIZONTAL);
		endian_s->Add(new wxStaticText(this, wxID_ANY, wxT("Endian:")), 0, wxGROW);
		endian_s->Add(endian = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			0, NULL, wxCB_READONLY), 0, wxGROW);
		endian->Append(towxstring("(Memory area default)"));
		endian->Append(towxstring("Little-endian"));
		endian->Append(towxstring("Host-endian"));
		endian->Append(towxstring("Big-endian"));
		endian->SetSelection(0);
		endian->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(dialog_disassemble::on_change),
			NULL, this);
		top_s->Add(endian_s, 0, wxGROW);

		wxBoxSizer* vma_s = new wxBoxSizer(wxHORIZONTAL);
		vma_s->Add(new wxStaticText(this, wxID_ANY, wxT("Area:")), 0, wxGROW);
		vma_s->Add(vma = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			0, NULL, wxCB_READONLY), 0, wxGROW);
		vma->Append(towxstring("(Any)"));
		for(auto& i : regions)
			vma->Append(towxstring(i.first));
		vma->SetSelection(0);
		vma->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler(dialog_disassemble::on_change),
			NULL, this);
		top_s->Add(vma_s, 0, wxGROW);

		wxBoxSizer* addr_s = new wxBoxSizer(wxHORIZONTAL);
		addr_s->Add(new wxStaticText(this, wxID_ANY, wxT("Address:")), 0, wxGROW);
		addr_s->Add(address = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(200, -1)),
			0, wxGROW);
		address->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(dialog_disassemble::on_change),
			NULL, this);
		top_s->Add(addr_s, 0, wxGROW);

		wxBoxSizer* cnt_s = new wxBoxSizer(wxHORIZONTAL);
		cnt_s->Add(new wxStaticText(this, wxID_ANY, wxT("Count:")), 0, wxGROW);
		cnt_s->Add(count = new wxSpinCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			wxSP_ARROW_KEYS, 1, 1000000000, 10), 0, wxGROW);
		count->Connect(wxEVT_SPINCTRL, wxCommandEventHandler(dialog_disassemble::on_change), NULL,
			this);
		top_s->Add(cnt_s, 0, wxGROW);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("OK")));
		pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")));
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_disassemble::on_ok), NULL,
			this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_disassemble::on_cancel),
			NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);
		top_s->SetSizeHints(this);
		Fit();

		has_default = spec;
		if(!spec) {
			dflt_lang = old_dflt_lang;
			dflt_base = old_dflt_base;
		}
		//Set default language.
		if(regex_match("\\$data:.*", dflt_lang)) {
			switch(dflt_lang[6]) {
			case 'b':	type->SetSelection(code_types + 0); break;
			case 'B':	type->SetSelection(code_types + 1); break;
			case 'c':	type->SetSelection(code_types + 2); break;
			case 'w':	type->SetSelection(code_types + 3); break;
			case 'W':	type->SetSelection(code_types + 4); break;
			case 'C':	type->SetSelection(code_types + 5); break;
			case 'h':	type->SetSelection(code_types + 6); break;
			case 'H':	type->SetSelection(code_types + 7); break;
			case 'i':	type->SetSelection(code_types + 8); break;
			case 'd':	type->SetSelection(code_types + 9); break;
			case 'D':	type->SetSelection(code_types + 10); break;
			case 'I':	type->SetSelection(code_types + 11); break;
			case 'q':	type->SetSelection(code_types + 12); break;
			case 'Q':	type->SetSelection(code_types + 13); break;
			case 'r':	type->SetSelection(code_types + 14); break;
			case 'f':	type->SetSelection(code_types + 15); break;
			case 'F':	type->SetSelection(code_types + 16); break;
			}
			switch(dflt_lang[7]) {
			case 'l':	endian->SetSelection(1); break;
			case 'h':	endian->SetSelection(2); break;
			case 'b':	endian->SetSelection(3); break;
			}
		} else {
			unsigned j = 0;
			//Set default disasm.
			for(auto& i : disasms) {
				if(i == dflt_lang)
					break;
				j++;
			}
			if(j < disasms.size())
				type->SetSelection(j);
		}
		//Set default address.
		int k = 0;
		for(auto& i : regions) {
			if(dflt_base >= i.second.first && dflt_base < i.second.first + i.second.second) {
				vma->SetSelection(k + 1);
				dflt_base -= i.second.first;
				break;
			}
			k++;
		}
		address->SetValue(towxstring((stringfmt() << std::hex << dflt_base).str()));

		wxCommandEvent e;
		on_change(e);
	}

	void dialog_disassemble::on_ok(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		EndModal(wxID_OK);
	}

	std::string dialog_disassemble::get_disassembler()
	{
		CHECK_UI_THREAD;
		if(type->GetSelection() >= (ssize_t)code_types && type->GetSelection() < (ssize_t)type->GetCount()) {
			int _endian = endian->GetSelection();
			int dtsel = type->GetSelection() - code_types;
			std::string _vma = tostdstring(vma->GetStringSelection());
			if(_endian <= 0 || _endian > 3) {
				_endian = 1;
				inst.iqueue->run([&_endian, _vma]() {
					for(auto i : CORE().memory->get_regions()) {
						if(i->name == _vma) {
							_endian = i->endian + 2;
						}
					}
				});
			}
			if(dtsel < 0) dtsel = 0;
			if(dtsel > 16) dtsel = 16;
			static const char* typechars = "bBcwWChHidDIqQrfF";
			static const char* endianchars = " lhb";
			std::string res = std::string("$data:") + std::string(1, typechars[dtsel]) +
				std::string(1, endianchars[_endian]);
			if(!has_default)
				old_dflt_lang = res;
			return res;
		} else {
			std::string res = tostdstring(type->GetStringSelection());
			if(!has_default)
				old_dflt_lang = res;
			return res;
		}
	}

	uint64_t dialog_disassemble::get_address()
	{
		CHECK_UI_THREAD;
		uint64_t base = 0;
		if(vma->GetSelection() && vma->GetSelection() != wxNOT_FOUND) {
			std::string _vma = tostdstring(vma->GetStringSelection());
			inst.iqueue->run([&base, _vma]() {
				for(auto i : CORE().memory->get_regions()) {
					if(i->name == _vma) {
						base = i->base;
					}
				}
			});
		}
		uint64_t off = hex::from<uint64_t>(tostdstring(address->GetValue()));
		uint64_t res = base + off;
		if(!has_default)
			old_dflt_base = res;
		return res;
	}

	uint64_t dialog_disassemble::get_count()
	{
		CHECK_UI_THREAD;
		return count->GetValue();
	}

	void dialog_disassemble::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		bool is_ok = true;
		try {
			hex::from<uint64_t>(tostdstring(address->GetValue()));
		} catch(std::exception& e) {
			is_ok = false;
		}
		is_ok = is_ok && (type->GetSelection() != wxNOT_FOUND);
		is_ok = is_ok && (vma->GetSelection() != wxNOT_FOUND);
		endian->Enable(type->GetSelection() >= (ssize_t)code_types && type->GetSelection() <
			(ssize_t)type->GetCount());
		is_ok = is_ok && (!endian->IsEnabled() || endian->GetSelection() != wxNOT_FOUND);
		//If VMA is global, ensure there is valid endian.
		is_ok = is_ok && (vma->GetSelection() != 0 || !endian->IsEnabled() || endian->GetSelection() != 0);
		ok->Enable(is_ok);
	}

	class wxwin_tracelog;

	class dialog_breakpoint_add : public wxDialog
	{
	public:
		dialog_breakpoint_add(wxWindow* parent, std::list<memory_space::region*> regions);
		std::pair<uint64_t, debug_context::etype> get_result();
		void on_ok(wxCommandEvent& e) { EndModal(wxID_OK); }
		void on_cancel(wxCommandEvent& e) { EndModal(wxID_CANCEL); }
		void on_address_change(wxCommandEvent& e);
	private:
		std::list<memory_space::region*> regions;
		wxComboBox* vmasel;
		wxTextCtrl* address;
		wxComboBox* typesel;
		wxButton* ok;
		wxButton* cancel;
	};

	dialog_breakpoint_add::dialog_breakpoint_add(wxWindow* parent, std::list<memory_space::region*> _regions)
		: wxDialog(parent, wxID_ANY, wxT("Add breakpoint"))
	{
		CHECK_UI_THREAD;
		regions = _regions;
		wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Memory region:")), 0, wxGROW);
		top_s->Add(vmasel = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			0, NULL, wxCB_READONLY), 1, wxGROW);
		vmasel->Append(towxstring(""));
		for(auto i : regions)
			vmasel->Append(towxstring(i->name));
		vmasel->SetSelection(0);

		top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Offset (hexadecimal):")), 0, wxGROW);
		top_s->Add(address = new wxTextCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxSize(350, -1)), 0,
			 wxGROW);

		top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Breakpoint type:")), 0, wxGROW);
		top_s->Add(typesel = new wxComboBox(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			0, NULL, wxCB_READONLY), 1, wxGROW);
		typesel->Append(towxstring("Read"));
		typesel->Append(towxstring("Write"));
		typesel->Append(towxstring("Execute"));
		typesel->SetSelection(0);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("OK")));
		pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")));
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_breakpoint_add::on_ok), NULL,
			this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_breakpoint_add::on_cancel),
			NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);
		top_s->SetSizeHints(this);
		Fit();
	}

	void dialog_breakpoint_add::on_address_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		try {
			hex::from<uint64_t>(tostdstring(address->GetValue()));
			ok->Enable(true);
		} catch(...) {
			ok->Enable(false);
		}
	}

	std::pair<uint64_t, debug_context::etype> dialog_breakpoint_add::get_result()
	{
		CHECK_UI_THREAD;
		std::string vmaname = tostdstring(vmasel->GetStringSelection());
		std::string addrtext = tostdstring(address->GetValue());
		uint64_t base = 0;
		if(vmaname != "") {
			for(auto i : regions)
				if(i->name == vmaname)
					base = i->base;
		}
		uint64_t addr;
		try {
			addr = base + hex::from<uint64_t>(addrtext);
		} catch(std::exception& e) {
			addr = base;
		}
		debug_context::etype dtype = debug_context::DEBUG_EXEC;
		if(typesel->GetSelection() == 0)
			dtype = debug_context::DEBUG_READ;
		if(typesel->GetSelection() == 1)
			dtype = debug_context::DEBUG_WRITE;
		if(typesel->GetSelection() == 2)
			dtype = debug_context::DEBUG_EXEC;
		return std::make_pair(addr, dtype);
	}

	class dialog_breakpoints : public wxDialog
	{
	public:
		dialog_breakpoints(wxwin_tracelog* parent, emulator_instance& _inst);
		void on_ok(wxCommandEvent& e) { EndModal(wxID_OK); }
		void on_add(wxCommandEvent& e);
		void on_delete(wxCommandEvent& e);
		void on_selchange(wxCommandEvent& e);
	private:
		std::string format_line(std::pair<uint64_t, debug_context::etype> entry);
		size_t get_insert_pos(std::pair<uint64_t, debug_context::etype> entry);
		void populate_breakpoints();
		std::list<memory_space::region*> regions;
		emulator_instance& inst;
		wxButton* ok;
		wxButton* addb;
		wxButton* delb;
		wxListBox* brklist;
		wxwin_tracelog* pwin;
		std::vector<std::pair<uint64_t, debug_context::etype>> listsyms;
	};

	class wxwin_tracelog : public wxFrame, public debug_context::callback_base
	{
	public:
		wxwin_tracelog(wxWindow* parent, emulator_instance& _inst, int _cpuid, const std::string& cpuname);
		~wxwin_tracelog();
		bool ShouldPreventAppExit() const { return false; }
		scroll_bar* get_scroll() { return scroll; }
		void on_wclose(wxCloseEvent& e);
		void on_enabled(wxCommandEvent& e);
		void on_menu(wxCommandEvent& e);
		void process_lines();
		uint64_t get_find_line() { return find_active ? find_line : 0xFFFFFFFFFFFFFFFFULL; }
		std::set<std::pair<uint64_t, debug_context::etype>> get_breakpoints();
		void add_breakpoint(uint64_t addr, debug_context::etype dtype);
		void remove_breakpoint(uint64_t addr, debug_context::etype dtype);
	private:
		class _panel : public text_framebuffer_panel
		{
		public:
			_panel(wxwin_tracelog* parent, emulator_instance& _inst);
			void on_size(wxSizeEvent& e);
			void on_mouse(wxMouseEvent& e);
			wxSize DoGetBestSize() const;
			uint64_t pos;
			std::vector<std::string> rows;
			void on_popup_menu(wxCommandEvent& e);
			bool scroll_to_end_on_repaint;
		protected:
			void prepare_paint();
		private:
			emulator_instance& inst;
			uint64_t pressed_row;
			uint64_t current_row;
			bool holding;
			wxwin_tracelog* p;
		};
		emulator_instance& inst;
		bool do_exit_save();
		void scroll_pane(uint64_t line);
		int cpuid;
		volatile bool trace_active;
		void callback(const debug_context::params& params);
		void killed(uint64_t addr, debug_context::etype type);
		void do_rwx_break(uint64_t addr, uint64_t value, debug_context::etype type);
		void kill_debug_hooks(bool kill_hard = false);
		scroll_bar* scroll;
		_panel* panel;
		bool broken;
		bool broken2;
		wxCheckBox* enabled;
		threads::lock buffer_mutex;
		std::list<std::string> lines_waiting;
		bool unprocessed_lines;
		bool closing;
		bool find_active;
		uint64_t find_line;
		std::string find_string;
		bool dirty;
		bool singlestepping;
		std::map<std::pair<uint64_t, debug_context::etype>, bool> rwx_breakpoints;
		wxMenuItem* m_singlestep;
	};

	wxwin_tracelog::~wxwin_tracelog()
	{
	}

	void wxwin_tracelog::on_wclose(wxCloseEvent& e)
	{
		CHECK_UI_THREAD;
		if(dirty && !wxwidgets_exiting) {
			int r = prompt_for_save(this, "Trace log");
			if(r < 0 || (r > 0 && !do_exit_save()))
				return;
		}
		if(trace_active)
			inst.iqueue->run([this]() { kill_debug_hooks(); });
		trace_active = false;
		if(!closing)
			Destroy();
		closing = true;
	}

	void wxwin_tracelog::kill_debug_hooks(bool kill_hard)
	{
		CORE().dbg->remove_callback(cpuid, debug_context::DEBUG_FRAME, *this);
		if(!kill_hard)
			CORE().dbg->remove_callback(cpuid, debug_context::DEBUG_TRACE, *this);
		threads::alock h(buffer_mutex);
		for(auto& i : rwx_breakpoints) {
			if(!i.second)
				continue;
			if(!kill_hard)
				CORE().dbg->remove_callback(i.first.first, i.first.second, *this);
			i.second = false;
		}
		trace_active = false;
		convert_break_to_pause();
	}

	wxwin_tracelog::_panel::_panel(wxwin_tracelog* parent, emulator_instance& _inst)
		: text_framebuffer_panel(parent, 20, 5, wxID_ANY, NULL), inst(_inst)
	{
		CHECK_UI_THREAD;
		p = parent;
		pos = 0;
		pressed_row = 0;
		current_row = 0;
		holding = false;
		scroll_to_end_on_repaint = false;
		this->Connect(wxEVT_SIZE, wxSizeEventHandler(wxwin_tracelog::_panel::on_size), NULL, this);
		this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxwin_tracelog::_panel::on_mouse), NULL, this);
		this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxwin_tracelog::_panel::on_mouse), NULL, this);
		this->Connect(wxEVT_MOTION, wxMouseEventHandler(wxwin_tracelog::_panel::on_mouse), NULL, this);
		this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(wxwin_tracelog::_panel::on_mouse), NULL, this);
	}

	void wxwin_tracelog::_panel::on_size(wxSizeEvent& e)
	{
		wxSize newsize = e.GetSize();
		auto tcell = get_cell();
		size_t lines = newsize.y / tcell.second;
		size_t linelen = newsize.x / tcell.first;
		if(lines < 1) lines = 1;
		if(linelen < 1) linelen = 1;
		set_size(linelen, lines);
		p->get_scroll()->set_page_size(lines);
		request_paint();
		e.Skip();
	}

	void wxwin_tracelog::_panel::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		uint64_t local_line = pos + e.GetY() / get_cell().second;
		if(e.RightDown()) {
			if(local_line < rows.size()) {
				holding = true;
				pressed_row = local_line;
			}
		}else if(e.RightUp()) {
			holding = false;
			wxMenu menu;
			menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
				wxCommandEventHandler(wxwin_tracelog::_panel::on_popup_menu), NULL, this);
			menu.Append(wxID_COPY, wxT("Copy to clipboard"));
			menu.Append(wxID_SAVE, wxT("Save to file"));
			menu.AppendSeparator();
			menu.Append(wxID_DELETE, wxT("Delete"));
			PopupMenu(&menu);
		} else {
			current_row = min(local_line, static_cast<uint64_t>(rows.size()));
			request_paint();
		}
		unsigned speed = 1;
		if(e.ShiftDown())
			speed = 10;
		p->get_scroll()->apply_wheel(e.GetWheelRotation(), e.GetWheelDelta(), speed);
	}

	wxSize wxwin_tracelog::_panel::DoGetBestSize() const
	{
		return wxSize(120 * 8, 25 * 16);
	}

	void wxwin_tracelog::_panel::prepare_paint()
	{
		p->get_scroll()->set_range(rows.size());
		if(scroll_to_end_on_repaint) {
			scroll_to_end_on_repaint = false;
			p->get_scroll()->set_position(rows.size());
			pos = p->get_scroll()->get_position();
		}
		uint64_t m = min(pressed_row, current_row);
		uint64_t M = max(pressed_row, current_row);
		auto s = get_characters();
		uint64_t fline = p->get_find_line();
		for(uint64_t i = pos; i < pos + s.second && i < rows.size(); i++) {
			bool selected = holding && (i >= m) && (i <= M);
			bool isfl = (i == fline);
			uint32_t fg = selected ? 0x0000FF : 0x000000;
			uint32_t bg = selected ? 0x000000 : (isfl ? 0xC0FFC0 : 0xFFFFFF);
			write(rows[i], s.first, 0, i - pos, fg, bg);
		}
		for(uint64_t i = rows.size(); i < pos + s.second; i++)
			write("", s.first, 0, i - pos, 0xFFFFFF, 0xFFFFFF);
	}

	void wxwin_tracelog::process_lines()
	{
		CHECK_UI_THREAD;
		threads::alock h(this->buffer_mutex);
		size_t osize = panel->rows.size();
		if(broken) {
			panel->rows.push_back(std::string(120, '-'));
			broken = false;
		}
		for(auto& i : lines_waiting)
			panel->rows.push_back(i);
		lines_waiting.clear();
		unprocessed_lines = false;
		if(panel->rows.size() != osize) {
			panel->scroll_to_end_on_repaint = true;
			dirty = true;
		}
		panel->request_paint();
	}

	void wxwin_tracelog::do_rwx_break(uint64_t addr, uint64_t value, debug_context::etype type)
	{
		inst.dbg->request_break();
	}

	void wxwin_tracelog::callback(const debug_context::params& p)
	{
		switch(p.type) {
		case debug_context::DEBUG_READ:
		case debug_context::DEBUG_WRITE:
		case debug_context::DEBUG_EXEC:
			do_rwx_break(p.rwx.addr, p.rwx.value, p.type);
			break;
		case debug_context::DEBUG_TRACE: {
			if(!trace_active)
				return;
			//Got tracelog line, send it.
			threads::alock h(buffer_mutex);
			lines_waiting.push_back(p.trace.decoded_insn);
			if(!unprocessed_lines) {
				unprocessed_lines = true;
				runuifun([this]() { this->process_lines(); });
			}
			if(singlestepping && p.trace.true_insn) {
				inst.dbg->request_break();
				singlestepping = false;
			}
			break;
		}
		case debug_context::DEBUG_FRAME: {
			std::ostringstream xstr;
			xstr << "------------ ";
			xstr << "Frame " << p.frame.frame;
			if(p.frame.loadstated) xstr << " (loadstated)";
			xstr << " ------------";
			std::string str = xstr.str();
			threads::alock h(buffer_mutex);
			lines_waiting.push_back(str);
			if(!unprocessed_lines) {
				unprocessed_lines = true;
				runuifun([this]() { this->process_lines(); });
			}
			break;
		}
		}
	}

	void wxwin_tracelog::killed(uint64_t addr, debug_context::etype type)
	{
		switch(type) {
		case debug_context::DEBUG_READ:
		case debug_context::DEBUG_WRITE:
		case debug_context::DEBUG_EXEC: {
			//We need to kill this hook if still active.
			auto i2 = std::make_pair(addr, type);
			rwx_breakpoints[i2] = false;
			break;
		}
		case debug_context::DEBUG_TRACE:
			//Dtor!
			if(!trace_active)
				return;
			kill_debug_hooks(true);
			runuifun([this]() {
				this->enabled->SetValue(false);
				this->enabled->Enable(false);
				this->m_singlestep->Enable(false);
			});
			break;
		case debug_context::DEBUG_FRAME:
			//Do nothing.
			break;
		}
	}

	void wxwin_tracelog::on_enabled(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		bool enable = enabled->GetValue();
		inst.iqueue->run([this, enable]() {
			if(enable) {
				threads::alock h(buffer_mutex);
				broken = broken2;
				broken2 = true;
				for(auto& i : rwx_breakpoints) {
					auto i2 = i.first;
					CORE().dbg->add_callback(i2.first, i2.second, *this);
					i.second = true;
				}
				CORE().dbg->add_callback(cpuid, debug_context::DEBUG_TRACE, *this);
				CORE().dbg->add_callback(0, debug_context::DEBUG_FRAME, *this);
				this->trace_active = true;
			} else if(trace_active) {
				this->trace_active = false;
				this->kill_debug_hooks();
			}
		});
		m_singlestep->Enable(enable);
	}

	bool find_match(const std::string& pattern, const std::string& candidate)
	{
		static std::string last_find;
		if(pattern == "")
			return false;
		std::string tmp = pattern;
		tmp = tmp.substr(1);
		if(pattern[0] == 'F')
			return regex_match(tmp, candidate, REGEX_MATCH_LITERIAL);
		if(pattern[0] == 'W')
			return regex_match(tmp, candidate, REGEX_MATCH_IWILDCARDS);
		if(pattern[0] == 'R')
			return regex_match(tmp, candidate, REGEX_MATCH_IREGEX);
		return false;
	}

	void wxwin_tracelog::on_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(e.GetId() == wxID_EXIT) {
			if(dirty) {
				int r = prompt_for_save(this, "Trace log");
				if(r < 0 || (r > 0 && !do_exit_save()))
					return;
			}
			if(trace_active) {
				inst.iqueue->run([this]() { this->kill_debug_hooks(); });
			}
			trace_active = false;
			Destroy();
			return;
		} else if(e.GetId() == wxID_SAVE) {
			try {
				std::string filename = choose_file_save(this, "Save tracelog to",
					UI_get_project_otherpath(inst), filetype_trace);
				std::ofstream s(filename, std::ios::app);
				if(!s) throw std::runtime_error("Error opening output file");
				for(auto& i : panel->rows)
					s << i << std::endl;
				if(!s) throw std::runtime_error("Error writing output file");
				dirty = false;
			} catch(canceled_exception& e) {
			} catch(std::exception& e) {
				wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION |
					wxOK, this);
			}
		} else if(e.GetId() == wxID_FIND) {
			std::string tmp;
			dialog_find* d = new dialog_find(this);
			if(d->ShowModal() != wxID_OK) {
				d->Destroy();
				return;
			}
			tmp = d->get_pattern();
			d->Destroy();
			if(tmp == "") {
				find_active = false;
				return;
			}
			//Do syntax check.
			try {
				find_match(tmp, "");
			} catch(...) {
				find_active = false;
				wxMessageBox(towxstring("Invalid search pattern"), _T("Invalid pattern"),
					wxICON_EXCLAMATION | wxOK, this);
				return;
			}
			find_string = tmp;
			find_active = true;
			find_line = 0;
			while(find_line < panel->rows.size()) {
				if(find_match(find_string, panel->rows[find_line]))
					break;
				find_line++;
			}
			if(find_line == panel->rows.size()) {
				//Not found.
				find_active = false;
				wxMessageBox(towxstring("Found nothing appropriate"), _T("Not found"),
					wxICON_EXCLAMATION | wxOK, this);
			} else
				scroll_pane(find_line);
		} else if(e.GetId() == wxID_FIND_NEXT) {
			if(!find_active)
				return;
			uint64_t old_find_line = find_line;
			find_line++;
			while(!panel->rows.empty() && find_line != old_find_line) {
				if(find_line >= panel->rows.size())
					find_line = 0;
				if(find_match(find_string, panel->rows[find_line]))
					break;
				find_line++;
			}
			scroll_pane(find_line);
		} else if(e.GetId() == wxID_FIND_PREV) {
			if(!find_active)
				return;
			uint64_t old_find_line = find_line;
			find_line--;
			while(!panel->rows.empty() && find_line != old_find_line) {
				if(find_line >= panel->rows.size())
					find_line = panel->rows.size() - 1;
				if(find_match(find_string, panel->rows[find_line]))
					break;
				find_line--;
			}
			scroll_pane(find_line);
		} else if(e.GetId() == wxID_SINGLESTEP) {
			inst.iqueue->run_async([this]() {
				this->singlestepping = true;
				CORE().command->invoke("unpause-emulator");
			}, [](std::exception& e) {});
		} else if(e.GetId() == wxID_FRAMEADVANCE) {
			inst.iqueue->run_async([this]() {
				CORE().command->invoke("+advance-frame");
				CORE().command->invoke("-advance-frame");
			}, [](std::exception& e) {});
		} else if(e.GetId() == wxID_CONTINUE) {
			inst.iqueue->run_async([this]() {
				CORE().command->invoke("unpause-emulator");
			}, [](std::exception& e) {});
		} else if(e.GetId() == wxID_BREAKPOINTS) {
			dialog_breakpoints* d = new dialog_breakpoints(this, inst);
			d->ShowModal();
			d->Destroy();
		} else if(e.GetId() == wxID_CLEAR) {
			int r = prompt_for_save(this, "Trace log");
			if(r < 0 || (r > 0 && !do_exit_save()))
				return;
			panel->rows.clear();
			panel->request_paint();
			find_active = false;
		}
	}

	void wxwin_tracelog::_panel::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		std::string str;
		uint64_t m = min(pressed_row, current_row);
		uint64_t M = max(pressed_row, current_row) + 1;
		m = min(m, (uint64_t)rows.size());
		M = min(M, (uint64_t)rows.size());
		size_t lines = 0;
		{
			for(uint64_t i = m; i < M && i < rows.size(); i++) {
				try {
					std::string mline = rows[i];
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
				std::string filename = choose_file_save(this, "Save tracelog fragment to",
					UI_get_project_otherpath(inst), filetype_trace);
				std::ofstream s(filename, std::ios::app);
				if(!s) throw std::runtime_error("Error opening output file");
				if(lines == 1) str += "\n";
				s << str;
				if(!s) throw std::runtime_error("Error writing output file");
			} catch(canceled_exception& e) {
			} catch(std::exception& e) {
				wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION |
					wxOK, this);
			}
			break;
		case wxID_DELETE:
			rows.erase(rows.begin() + m, rows.begin() + M);
			if(m != M)
				p->dirty = true;
			request_paint();
			break;
		}
	}

	void wxwin_tracelog::scroll_pane(uint64_t line)
	{
		unsigned r = panel->get_characters().second;
		unsigned offset = r / 2;
		if(offset > line)
			scroll->set_position(panel->pos = 0);
		else if(line + r <= panel->rows.size())
			scroll->set_position(panel->pos = line - offset);
		else
			scroll->set_position(panel->pos = panel->rows.size() - r);
		panel->request_paint();
	}

	bool wxwin_tracelog::do_exit_save()
	{
		CHECK_UI_THREAD;
back:
		try {
			std::string filename = choose_file_save(this, "Save tracelog to",
				UI_get_project_otherpath(inst), filetype_trace);
			std::ofstream s(filename, std::ios::app);
			if(!s) throw std::runtime_error("Error opening output file");
			for(auto& i : panel->rows)
				s << i << std::endl;
			if(!s) throw std::runtime_error("Error writing output file");
			dirty = false;
		} catch(canceled_exception& e) {
			return false;
		} catch(std::exception& e) {
			wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION |
				wxOK, this);
			goto back;
		}
		return true;
	}

	std::set<std::pair<uint64_t, debug_context::etype>> wxwin_tracelog::get_breakpoints()
	{
		std::set<std::pair<uint64_t, debug_context::etype>> ret;
		inst.iqueue->run([this, &ret]() {
			for(auto i : rwx_breakpoints)
				ret.insert(i.first);
		});
		return ret;
	}

	void wxwin_tracelog::add_breakpoint(uint64_t addr, debug_context::etype dtype)
	{
		std::pair<uint64_t, debug_context::etype> i2 = std::make_pair(addr, dtype);
		if(!trace_active) {
			//We'll register this later.
			rwx_breakpoints[i2] = false;
			return;
		}
		inst.dbg->add_callback(i2.first, i2.second, *this);
		rwx_breakpoints[i2] = true;
	}

	void wxwin_tracelog::remove_breakpoint(uint64_t addr, debug_context::etype dtype)
	{
		std::pair<uint64_t, debug_context::etype> i2 = std::make_pair(addr, dtype);
		auto& h = rwx_breakpoints[i2];
		if(h)
			inst.dbg->remove_callback(i2.first, i2.second, *this);
		rwx_breakpoints.erase(i2);
	}

	wxwin_tracelog::wxwin_tracelog(wxWindow* parent, emulator_instance& _inst, int _cpuid,
		const std::string& cpuname)
		: wxFrame(parent, wxID_ANY, towxstring("lsnes: Tracelog for " + cpuname), wxDefaultPosition,
			wxDefaultSize, wxMINIMIZE_BOX | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX |
			wxCLIP_CHILDREN), inst(_inst)
	{
		CHECK_UI_THREAD;
		cpuid = _cpuid;
		singlestepping = false;
		find_active = false;
		find_line = 0;
		closing = false;
		trace_active = false;
		unprocessed_lines = false;
		broken = false;
		broken2 = false;
		dirty = false;
		wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);
		wxBoxSizer* bottom_s = new wxBoxSizer(wxHORIZONTAL);
		top_s->Add(enabled = new wxCheckBox(this, wxID_ANY, wxT("Enabled")), 0, wxGROW);
		bottom_s->Add(panel = new _panel(this, inst), 1, wxGROW);
		bottom_s->Add(scroll = new scroll_bar(this, wxID_ANY, true), 0, wxGROW);
		top_s->Add(bottom_s, 1, wxGROW);
		enabled->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(wxwin_tracelog::on_enabled),
			NULL, this);
		Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_tracelog::on_wclose),
			NULL, this);
		scroll->set_page_size(panel->get_characters().second);
		scroll->set_handler([this](scroll_bar& s) {
			this->panel->pos = s.get_position();
			this->panel->request_paint();
		});
		wxMenuBar* mb;
		wxStatusBar* sb;
		wxMenu* menu;

		SetMenuBar(mb = new wxMenuBar);
		SetStatusBar(sb = new wxStatusBar(this));
		mb->Append(menu = new wxMenu(), wxT("File"));
		menu->Append(wxID_SAVE, wxT("Save"));
		menu->AppendSeparator();
		menu->Append(wxID_EXIT, wxT("Close"));
		mb->Append(menu = new wxMenu(), wxT("Edit"));
		menu->Append(wxID_FIND, wxT("Find..."));
		menu->Append(wxID_FIND_NEXT, wxT("Find next\tF3"));
		menu->Append(wxID_FIND_PREV, wxT("Find previous\tSHIFT+F3"));
		menu->AppendSeparator();
		menu->Append(wxID_CLEAR, towxstring("Clear"));
		mb->Append(menu = new wxMenu(), wxT("Debug"));
		m_singlestep = menu->Append(wxID_SINGLESTEP, towxstring("Singlestep\tF2"));
		menu->Append(wxID_FRAMEADVANCE, towxstring("Frame advance\tF4"));
		menu->Append(wxID_CONTINUE, towxstring("Continue\tF5"));
		menu->AppendSeparator();
		menu->Append(wxID_BREAKPOINTS, towxstring("Breakpoints"));
		m_singlestep->Enable(false);
		Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxwin_tracelog::on_menu),
			NULL, this);
		//Very nasty hack.
		wxSize tmp = panel->GetMinSize();
		panel->SetMinSize(panel->DoGetBestSize());
		top_s->SetSizeHints(this);
		wxSize tmp2 = GetClientSize();
		panel->SetMinSize(tmp);
		top_s->SetSizeHints(this);
		SetClientSize(tmp2);
	}

	struct disasm_row
	{
		uint64_t cover;
		std::string language;
		std::string row;
	};

	class wxwin_disassembler : public wxFrame
	{
	public:
		wxwin_disassembler(wxWindow* parent, emulator_instance& _inst);
		bool ShouldPreventAppExit() const { return false; }
		scroll_bar* get_scroll() { return scroll; }
		void on_menu(wxCommandEvent& e);
		void on_wclose(wxCloseEvent& e);
	private:
		class _panel : public text_framebuffer_panel
		{
		public:
			_panel(wxwin_disassembler* parent, emulator_instance& _inst);
			void on_size(wxSizeEvent& e);
			void on_mouse(wxMouseEvent& e);
			wxSize DoGetBestSize() const;
			uint64_t pos;
			std::vector<uint64_t> rows;
			std::map<uint64_t, disasm_row> row_map;
			void on_popup_menu(wxCommandEvent& e);
		protected:
			void prepare_paint();
		private:
			emulator_instance& inst;
			uint64_t pressed_row;
			uint64_t current_row;
			bool holding;
			wxwin_disassembler* p;
		};
		bool do_exit_save();
		void add_row(uint64_t addr, const disasm_row& row, bool last);
		void add_rows(const std::map<uint64_t, disasm_row>& rowdata);
		void add_rows_main(const std::map<uint64_t, disasm_row>& rowdata);
		void run_disassembler(const std::string& disasm, uint64_t addrbase, uint64_t count);
		void scroll_pane(uint64_t line);
		emulator_instance& inst;
		scroll_bar* scroll;
		_panel* panel;
		bool dirty;
		bool closing;
	};

	wxwin_disassembler::_panel::_panel(wxwin_disassembler* parent, emulator_instance& _inst)
		: text_framebuffer_panel(parent, 20, 5, wxID_ANY, NULL), inst(_inst)
	{
		CHECK_UI_THREAD;
		p = parent;
		pos = 0;
		pressed_row = 0;
		current_row = 0;
		holding = false;
		this->Connect(wxEVT_SIZE, wxSizeEventHandler(wxwin_disassembler::_panel::on_size), NULL, this);
		this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxwin_disassembler::_panel::on_mouse), NULL,
			this);
		this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxwin_disassembler::_panel::on_mouse), NULL, this);
		this->Connect(wxEVT_MOTION, wxMouseEventHandler(wxwin_disassembler::_panel::on_mouse), NULL, this);
		this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(wxwin_disassembler::_panel::on_mouse), NULL,
			this);
	}

	void wxwin_disassembler::_panel::on_size(wxSizeEvent& e)
	{
		wxSize newsize = e.GetSize();
		auto tcell = get_cell();
		size_t lines = newsize.y / tcell.second;
		size_t linelen = newsize.x / tcell.first;
		if(lines < 1) lines = 1;
		if(linelen < 1) linelen = 1;
		set_size(linelen, lines);
		p->get_scroll()->set_page_size(lines);
		request_paint();
		e.Skip();
	}

	void wxwin_disassembler::_panel::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		uint64_t local_line = pos + e.GetY() / get_cell().second;
		if(e.RightDown()) {
			if(local_line < rows.size()) {
				holding = true;
				pressed_row = local_line;
			}
		}else if(e.RightUp()) {
			holding = false;
			wxMenu menu;
			menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
				wxCommandEventHandler(wxwin_disassembler::_panel::on_popup_menu), NULL, this);
			menu.Append(wxID_COPY, wxT("Copy to clipboard"));
			menu.Append(wxID_SAVE, wxT("Save to file"));
			menu.AppendSeparator();
			menu.Append(wxID_DISASM_MORE, wxT("Disassemble more"));
			menu.AppendSeparator();
			menu.Append(wxID_DELETE, wxT("Delete"));
			PopupMenu(&menu);
		} else {
			current_row = min(local_line, static_cast<uint64_t>(rows.size()));
			request_paint();
		}
		unsigned speed = 1;
		if(e.ShiftDown())
			speed = 10;
		p->get_scroll()->apply_wheel(e.GetWheelRotation(), e.GetWheelDelta(), speed);
	}

	wxSize wxwin_disassembler::_panel::DoGetBestSize() const
	{
		return wxSize(40 * 8, 25 * 16);
	}

	void wxwin_disassembler::_panel::prepare_paint()
	{
		p->get_scroll()->set_range(rows.size());
		uint64_t m = min(pressed_row, current_row);
		uint64_t M = max(pressed_row, current_row);
		auto s = get_characters();
		uint64_t i;
		for(i = pos; i < pos + s.second && i < rows.size(); i++) {
			bool selected = holding && (i >= m) && (i <= M);
			uint32_t fg = selected ? 0x0000FF : 0x000000;
			uint32_t bg = selected ? 0x000000 : 0xFFFFFF;
			write(row_map[rows[i]].row, s.first, 0, i - pos, fg, bg);
		}
		for(; i < pos + s.second; i++) {
			write("", s.first, 0, i - pos, 0xFFFFFF, 0xFFFFFF);
		}
	}

	void wxwin_disassembler::on_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(e.GetId() == wxID_EXIT) {
			if(dirty) {
				int r = prompt_for_save(this, "Disassembly");
				if(r < 0 || (r > 0 && !do_exit_save()))
					return;
			}
			Destroy();
			return;
		} else if(e.GetId() == wxID_SAVE) {
			try {
				std::string filename = choose_file_save(this, "Save disassembly to",
					UI_get_project_otherpath(inst), filetype_disassembly);
				std::ofstream s(filename, std::ios::app);
				if(!s) throw std::runtime_error("Error opening output file");
				for(auto& i : panel->rows)
					s << panel->row_map[i].row << std::endl;
				if(!s) throw std::runtime_error("Error writing output file");
				dirty = false;
			} catch(canceled_exception& e) {
			} catch(std::exception& e) {
				wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION |
					wxOK, this);
			}
		} else if(e.GetId() == wxID_DISASM) {
			std::string tmp;
			dialog_disassemble* d = new dialog_disassemble(this, inst);
			if(d->ShowModal() != wxID_OK) {
				d->Destroy();
				return;
			}
			std::string disasm = d->get_disassembler();
			uint64_t addr = d->get_address();
			uint64_t count = d->get_count();
			d->Destroy();
			inst.iqueue->run_async([this, disasm, addr, count]() {
				this->run_disassembler(disasm, addr, count);
			}, [](std::exception& e) {});
		} else if(e.GetId() == wxID_GOTO) {
			try {
				std::string to = pick_text(this, "Goto", "Enter address to go to:", "");
				inst.iqueue->run_async([this, to]() {
					uint64_t addr;
					uint64_t base = 0;
					std::string vma;
					std::string offset;
					std::string _to = to;
					size_t sp = _to.find_first_of("+");
					if(sp >= _to.length()) {
						offset = _to;
					} else {
						vma = _to.substr(0, sp);
						offset = _to.substr(sp + 1);
					}
					if(vma != "") {
						bool found = false;
						for(auto i : CORE().memory->get_regions()) {
							if(i->name == vma) {
								base = i->base;
								found = true;
							}
						}
						if(!found) {
							runuifun([this] {
								show_message_ok(this, "Error in address",
									"No such memory area known",
									wxICON_EXCLAMATION);
							});
							return;
						}
					}
					if(run_show_error(this, "Error in address", "Expected <hexdigits> or "
						" <name>+<hexdigits>", [&addr, offset]() {
						addr = hex::from<uint64_t>(offset); }))
						return;
					addr += base;
					runuifun([this, addr]() {
						uint64_t nrow = 0;
						uint64_t low = 0;
						uint64_t high = this->panel->rows.size();
						while(low < high && low < high - 1) {
							nrow = (low + high) / 2;
							if(this->panel->rows[nrow] > addr)
								high = nrow;
							else if(this->panel->rows[nrow] < addr)
								low = nrow;
							else
								break;
						}
						this->scroll_pane(nrow);
					});
				}, [](std::exception& e) {});
			} catch(canceled_exception& e) {
			}
		}
	}

	void remove_from_array(std::vector<uint64_t>& v, uint64_t e)
	{
		//Binary search for the element to remove.
		size_t low = 0;
		size_t high = v.size();
		size_t mid = 0;
		while(low < high) {
			mid = (low + high) / 2;
			if(v[mid] < e)
				low = mid;
			else if(v[mid] > e)
				high = mid;
			else
				break;
		}
		if(v[mid] == e)
			v.erase(v.begin() + mid);
	}

	void wxwin_disassembler::_panel::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(e.GetId() == wxID_DISASM_MORE)
		{
			if(current_row >= rows.size())
				return;
			uint64_t base = rows[current_row];
			uint64_t rbase = base;
			if(!row_map.count(base))
				return;
			auto& r = row_map[base];
			base = base + r.cover;
			std::string disasm = r.language;
			dialog_disassemble* d = new dialog_disassemble(this, inst, base, disasm);
			if(d->ShowModal() != wxID_OK) {
				d->Destroy();
				return;
			}
			disasm = d->get_disassembler();
			uint64_t addr = d->get_address();
			uint64_t count = d->get_count();
			d->Destroy();
			auto pp = p;
			inst.iqueue->run_async([pp, disasm, addr, count]() {
				pp->run_disassembler(disasm, addr, count);
			}, [](std::exception& e) {});
			//Delete entries in (rbase, addr) if addr = base.
			if(addr == base) {
				for(uint64_t i = rbase + 1; i < addr; i++)
					if(row_map.count(i)) {
						//This line needs to be removed from rows too.
						row_map.erase(i);
						remove_from_array(rows, i);
					}
			}
		}
		std::string str;
		uint64_t m = min(min(pressed_row, current_row), (uint64_t)rows.size());
		uint64_t M = min(max(pressed_row, current_row) + 1, (uint64_t)rows.size());
		size_t lines = 0;
		{
			for(uint64_t i = m; i < M; i++) {
				try {
					std::string mline = row_map[rows[i]].row;
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
				std::string filename = choose_file_save(this, "Save disassembly fragment to",
					UI_get_project_otherpath(inst), filetype_disassembly);
				std::ofstream s(filename, std::ios::app);
				if(!s) throw std::runtime_error("Error opening output file");
				if(lines == 1) str += "\n";
				s << str;
				if(!s) throw std::runtime_error("Error writing output file");
			} catch(canceled_exception& e) {
			} catch(std::exception& e) {
				wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION |
					wxOK, this);
			}
			break;
		case wxID_DELETE:
			for(uint64_t i = m; i < M; i++)
				row_map.erase(rows[i]);
			rows.erase(rows.begin() + m, rows.begin() + M);
			if(m != M)
				p->dirty = true;
			request_paint();
			break;
		}
	}

	std::string format_vma_offset(memory_space::region& region, uint64_t offset)
	{
		std::ostringstream y;
		y << region.name;
		size_t sizedigits = 0;
		uint64_t tmp = region.size - 1;
		while(tmp > 0) {
			tmp >>= 4;
			sizedigits++;
		}
		y << "+" << std::hex << std::setfill('0') << std::setw(sizedigits) << offset;
		return y.str();
	}

	std::string lookup_address(emulator_instance& inst, uint64_t raw)
	{
		auto g = inst.memory->lookup(raw);
		if(!g.first)
			return hex::to<uint64_t>(raw);
		else
			return format_vma_offset(*g.first, g.second);
	}

	inline int sign_compare(uint64_t a, uint64_t b)
	{
		if(a < b) return -1;
		if(b < a) return 1;
		return 0;
	}

	void insert_into_array(std::vector<uint64_t>& v, uint64_t e)
	{
		//Binary search for the gap to insert to.
		size_t low = 0;
		size_t high = v.size();
		size_t mid = 0;
		while(low < high) {
			mid = (low + high) / 2;
			int s1 = sign_compare(v[mid], e);
			int s2 = ((mid + 1) < v.size()) ? sign_compare(v[mid + 1], e) : 1;
			if(s1 < 0 && s2 > 0)
				break;
			else if(s1 == 0 || s2 == 0)
				return;
			else if(s1 > 0)
				high = mid;
			else if(s2 < 0)
				low = mid;
		}
		if(mid < v.size() && v[mid] < e)
			mid++;
		v.insert(v.begin() + mid, e);
	}

	void wxwin_disassembler::add_row(uint64_t addr, const disasm_row& row, bool last)
	{
		auto& rows = panel->rows;
		auto& row_map = panel->row_map;
		if(row_map.count(addr)) {
			row_map[addr] = row;
		} else {
			//We need to insert the row into rows.
			row_map[addr] = row;
			insert_into_array(rows, addr);
		}
		dirty = true;
		if(!last)
			for(uint64_t i = addr + 1; i < addr + row.cover; i++)
				if(row_map.count(i)) {
					//This line needs to be removed from rows too.
					row_map.erase(i);
					remove_from_array(rows, i);
				}
	}

	void wxwin_disassembler::add_rows(const std::map<uint64_t, disasm_row>& rowdata)
	{
		for(auto i = rowdata.begin(); i != rowdata.end(); i++) {
			auto j = i;
			j++;
			bool last = (j == rowdata.end());
			add_row(i->first, i->second, last);
		}
		panel->request_paint();
	}

	void wxwin_disassembler::add_rows_main(const std::map<uint64_t, disasm_row>& rowdata)
	{
		std::map<uint64_t, disasm_row> _rowdata;
		for(auto& i : rowdata) {
			_rowdata[i.first] = i.second;
			_rowdata[i.first].row = lookup_address(inst, i.first) + " " + i.second.row;
		}
		runuifun([this, _rowdata]() { this->add_rows(_rowdata); });
	}

	template<typename T, bool hex> disasm_row _disassemble_data_item(emulator_instance& inst, uint64_t& addrbase,
		int endian, const std::string& disasm)
	{
		char buf[sizeof(T)];
		for(size_t i = 0; i < sizeof(T); i++)
			buf[i] = inst.memory->read<uint8_t>(addrbase + i);
		disasm_row r;
		if(hex)
			r.row = (stringfmt() << "DATA 0x" << hex::to<T>(serialization::read_endian<T>(buf, endian))).
				str();
		else if(sizeof(T) > 1)
			r.row = (stringfmt() << "DATA " << serialization::read_endian<T>(buf, endian)).str();
		else
			r.row = (stringfmt() << "DATA " << (int)serialization::read_endian<T>(buf, endian)).str();
		r.cover = sizeof(T);
		r.language = disasm;
		addrbase += sizeof(T);
		return r;
	}

	disasm_row disassemble_data_item(emulator_instance& inst, uint64_t& addrbase, const std::string& disasm)
	{
		int endian;
		if(disasm[7] == 'l') endian = -1;
		if(disasm[7] == 'h') endian = 0;
		if(disasm[7] == 'b') endian = 1;
		switch(disasm[6]) {
		case 'b':	return _disassemble_data_item<int8_t, false>(inst, addrbase, endian, disasm);
		case 'B':	return _disassemble_data_item<uint8_t, false>(inst, addrbase, endian, disasm);
		case 'c':	return _disassemble_data_item<uint8_t, true>(inst, addrbase, endian, disasm);
		case 'C':	return _disassemble_data_item<uint16_t, true>(inst, addrbase, endian, disasm);
		case 'd':	return _disassemble_data_item<int32_t, false>(inst, addrbase, endian, disasm);
		case 'D':	return _disassemble_data_item<uint32_t, false>(inst, addrbase, endian, disasm);
		case 'f':	return _disassemble_data_item<float, false>(inst, addrbase, endian, disasm);
		case 'F':	return _disassemble_data_item<double, false>(inst, addrbase, endian, disasm);
		case 'h':	return _disassemble_data_item<ss_int24_t, false>(inst, addrbase, endian, disasm);
		case 'H':	return _disassemble_data_item<ss_uint24_t, false>(inst, addrbase, endian, disasm);
		case 'i':	return _disassemble_data_item<ss_uint24_t, true>(inst, addrbase, endian, disasm);
		case 'I':	return _disassemble_data_item<uint32_t, true>(inst, addrbase, endian, disasm);
		case 'q':	return _disassemble_data_item<int64_t, false>(inst, addrbase, endian, disasm);
		case 'Q':	return _disassemble_data_item<uint64_t, false>(inst, addrbase, endian, disasm);
		case 'r':	return _disassemble_data_item<uint64_t, true>(inst, addrbase, endian, disasm);
		case 'w':	return _disassemble_data_item<int16_t, false>(inst, addrbase, endian, disasm);
		case 'W':	return _disassemble_data_item<uint16_t, false>(inst, addrbase, endian, disasm);
		};
		throw std::runtime_error("Invalid kind of data");
	}

	void wxwin_disassembler::scroll_pane(uint64_t line)
	{
		unsigned r = panel->get_characters().second;
		unsigned offset = r / 2;
		if(offset > line)
			scroll->set_position(panel->pos = 0);
		else if(line + r < panel->rows.size())
			scroll->set_position(panel->pos = line - offset);
		else
			scroll->set_position(panel->pos = panel->rows.size() - r);
		panel->request_paint();
	}

	void wxwin_disassembler::on_wclose(wxCloseEvent& e)
	{
		CHECK_UI_THREAD;
		if(dirty && !wxwidgets_exiting) {
			int r = prompt_for_save(this, "Disassembly");
			if(r < 0 || (r > 0 && !do_exit_save()))
				return;
		}
		if(!closing)
			Destroy();
		closing = true;
	}

	void wxwin_disassembler::run_disassembler(const std::string& disasm, uint64_t addrbase, uint64_t count)
	{
		std::map<uint64_t, disasm_row> rowdata;
		if(regex_match("\\$data:.*", disasm)) {
			if(run_show_error(this, "Error in disassember", "Error in disassember",
				[this, disasm, &rowdata, &addrbase, count]() {
				for(uint64_t i = 0; i < count; i++) {
					uint64_t base = addrbase;
					disasm_row r = disassemble_data_item(this->inst, addrbase, disasm);
					rowdata[base] = r;
				}
				}))
				return;
			add_rows_main(rowdata);
			return;
		}
		disassembler* d;
		if(run_show_error(this, "Error in disassember", "No disassembler '" + disasm + "' found",
			[&d, disasm]() {
			d = &disassembler::byname(disasm);
			}))
			return;
		for(uint64_t i = 0; i < count; i++) {
			uint64_t base = addrbase;
			disasm_row r;
			r.row = d->disassemble(addrbase, [this, &addrbase]() -> unsigned char {
				return this->inst.memory->read<uint8_t>(addrbase++);
			});
			r.cover = addrbase - base;
			r.language = disasm;
			rowdata[base] = r;
		}
		add_rows_main(rowdata);
	}

	bool wxwin_disassembler::do_exit_save()
	{
		CHECK_UI_THREAD;
back:
		try {
			std::string filename = choose_file_save(this, "Save disassembly to",
				UI_get_project_otherpath(inst), filetype_disassembly);
			std::ofstream s(filename, std::ios::app);
			if(!s) throw std::runtime_error("Error opening output file");
			for(auto& i : panel->rows)
				s << panel->row_map[i].row << std::endl;
			if(!s) throw std::runtime_error("Error writing output file");
			dirty = false;
		} catch(canceled_exception& e) {
			return false;
		} catch(std::exception& e) {
			wxMessageBox(towxstring(e.what()), _T("Error creating file"), wxICON_EXCLAMATION |
				wxOK, this);
			goto back;
		}
		return true;
	}

	wxwin_disassembler::wxwin_disassembler(wxWindow* parent, emulator_instance& _inst)
		: wxFrame(parent, wxID_ANY, towxstring("lsnes: Disassembler"), wxDefaultPosition,
			wxDefaultSize, wxMINIMIZE_BOX | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX |
			wxCLIP_CHILDREN), inst(_inst)
	{
		CHECK_UI_THREAD;
		closing = false;
		dirty = false;
		wxBoxSizer* top_s = new wxBoxSizer(wxHORIZONTAL);
		SetSizer(top_s);
		top_s->Add(panel = new _panel(this, inst), 1, wxGROW);
		top_s->Add(scroll = new scroll_bar(this, wxID_ANY, true), 0, wxGROW);
		scroll->set_page_size(panel->get_characters().second);
		scroll->set_handler([this](scroll_bar& s) {
			this->panel->pos = s.get_position();
			this->panel->request_paint();
		});
		wxMenuBar* mb;
		wxStatusBar* sb;
		wxMenu* menu;

		SetMenuBar(mb = new wxMenuBar);
		SetStatusBar(sb = new wxStatusBar(this));
		mb->Append(menu = new wxMenu(), wxT("File"));
		menu->Append(wxID_DISASM, wxT("Disassemble..."));
		menu->AppendSeparator();
		menu->Append(wxID_SAVE, wxT("Save"));
		menu->AppendSeparator();
		menu->Append(wxID_EXIT, wxT("Close"));
		Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxwin_disassembler::on_menu),
			NULL, this);
		mb->Append(menu = new wxMenu(), wxT("Edit"));
		menu->Append(wxID_GOTO, wxT("Goto"));

		Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_disassembler::on_wclose),
			NULL, this);
		//Very nasty hack.
		wxSize tmp = panel->GetMinSize();
		panel->SetMinSize(panel->DoGetBestSize());
		top_s->SetSizeHints(this);
		wxSize tmp2 = GetClientSize();
		panel->SetMinSize(tmp);
		top_s->SetSizeHints(this);
		SetClientSize(tmp2);
	}

	dialog_breakpoints::dialog_breakpoints(wxwin_tracelog* parent, emulator_instance& _inst)
		: wxDialog(parent, wxID_ANY, wxT("Breakpoints")), inst(_inst)
	{
		CHECK_UI_THREAD;
		pwin = parent;
		regions = inst.memory->get_regions();
		wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);
		top_s->Add(brklist = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(300, 400)), 1, wxGROW);
		brklist->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(dialog_breakpoints::on_selchange), NULL, this);
		populate_breakpoints();
		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->Add(addb = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		pbutton_s->Add(delb = new wxButton(this, wxID_ANY, wxT("Remove")), 0, wxGROW);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
		delb->Enable(false);
		addb->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_breakpoints::on_add), NULL,
			this);
		delb->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_breakpoints::on_delete),
			NULL, this);
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(dialog_breakpoints::on_ok), NULL,
			this);
		top_s->Add(pbutton_s, 0, wxGROW);
		top_s->SetSizeHints(this);
		Fit();
	}

	void dialog_breakpoints::populate_breakpoints()
	{
		CHECK_UI_THREAD;
		auto t = pwin->get_breakpoints();
		for(auto i : t) {
			std::string line = format_line(i);
			unsigned insert_pos = get_insert_pos(i);
			brklist->Insert(towxstring(line), insert_pos);
			listsyms.insert(listsyms.begin() + insert_pos, i);
		}
	}

	void dialog_breakpoints::on_add(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		uint64_t addr;
		debug_context::etype dtype;
		dialog_breakpoint_add* d = new dialog_breakpoint_add(this, regions);
		if(d->ShowModal() != wxID_OK) {
			d->Destroy();
			return;
		}
		rpair(addr, dtype) = d->get_result();
		d->Destroy();
		inst.iqueue->run_async([this, addr, dtype]() {
			pwin->add_breakpoint(addr, dtype);
		}, [](std::exception& e) {});
		auto ent = std::make_pair(addr, dtype);
		std::string line = format_line(ent);
		unsigned insert_pos = get_insert_pos(ent);
		brklist->Insert(towxstring(line), insert_pos);
		listsyms.insert(listsyms.begin() + insert_pos, ent);
	}

	void dialog_breakpoints::on_delete(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		int idx = brklist->GetSelection();
		if(idx == wxNOT_FOUND)
			return;
		uint64_t addr;
		debug_context::etype dtype;
		addr = listsyms[idx].first;
		dtype = listsyms[idx].second;
		inst.iqueue->run_async([this, addr, dtype]() {
			pwin->remove_breakpoint(addr, dtype);
		}, [](std::exception& e) {});
		brklist->Delete(idx);
		listsyms.erase(listsyms.begin() + idx);
	}

	size_t dialog_breakpoints::get_insert_pos(std::pair<uint64_t, debug_context::etype> entry)
	{
		size_t i = 0;
		for(i = 0; i < listsyms.size(); i++)
			if(entry < listsyms[i])
				return i;
		return i;
	}

	std::string dialog_breakpoints::format_line(std::pair<uint64_t, debug_context::etype> entry)
	{
		std::string base = "";
		for(auto i : regions) {
			if(entry.first >= i->base && entry.first < i->base + i->size) {
				base = format_vma_offset(*i, entry.first - i->base);
				break;
			}
		}
		if(base == "")
			base = hex::to<uint64_t>(entry.first);
		if(entry.second == debug_context::DEBUG_READ)
			return base + ": Read";
		if(entry.second == debug_context::DEBUG_WRITE)
			return base + ": Write";
		if(entry.second == debug_context::DEBUG_EXEC)
			return base + ": Execute";
		return base + ": Unknown";
	}

	void dialog_breakpoints::on_selchange(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		delb->Enable(brklist->GetSelection() != wxNOT_FOUND);
	}
}

void wxeditor_tracelog_display(wxWindow* parent, emulator_instance& inst, int cpuid, const std::string& cpuname)
{
	CHECK_UI_THREAD;
	try {
		wxwin_tracelog* d = new wxwin_tracelog(parent, inst, cpuid, cpuname);
		d->Show();
	} catch(std::exception& e) {
		show_message_ok(parent, "Error opening trace logger", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_disassembler_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	wxwin_disassembler* d = new wxwin_disassembler(parent, inst);
	d->Show();
}
