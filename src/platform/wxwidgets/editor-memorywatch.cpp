#include "core/moviedata.hpp"
#include "core/memorywatch.hpp"

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>

#include "library/string.hpp"
#include "interface/romtype.hpp"

class wxeditor_watchexpr : public wxDialog
{
public:
	wxeditor_watchexpr(wxWindow* parent, const std::string& name, const std::string& expr);
	bool ShouldPreventAppExit() const;
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_rb_structured(wxCommandEvent& e);
	void on_rb_arbitrary(wxCommandEvent& e);
	void on_rb_busaddr(wxCommandEvent& e);
	void on_rb_mapaddr(wxCommandEvent& e);
	void on_addr_change(wxCommandEvent& e);
	std::string get_expr();
private:
	std::string out;
	wxRadioButton* structured;
	wxRadioButton* arbitrary;
	wxComboBox* typesel;
	wxRadioButton* busaddr;
	wxRadioButton* mapaddr;
	wxTextCtrl* addr;
	wxCheckBox* hex;
	wxTextCtrl* watchtxt;
	wxButton* ok;
	wxButton* cancel;
};

int memorywatch_recognize_typech(char ch)
{
	switch(ch) {
	case 'b':	return 0;
	case 'B':	return 1;
	case 'w':	return 2;
	case 'W':	return 3;
	case 'd':	return 4;
	case 'D':	return 5;
	case 'q':	return 6;
	case 'Q':	return 7;
	default:	return 0;
	}
}

wxeditor_watchexpr::wxeditor_watchexpr(wxWindow* parent, const std::string& name, const std::string& expr)
	: wxDialog(parent, wxID_ANY, towxstring("lsnes: Edit watch " + name), wxDefaultPosition, wxSize(-1, -1))
{
	wxString types[] = {
		wxT("Signed byte"), wxT("Unsigned byte"), wxT("Signed word"), wxT("Unsigned word"),
		wxT("Signed doubleword"), wxT("Unsigned doubleword"), wxT("Signed quadword"), wxT("Unsigned quadword")
	};
	
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(9, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(structured = new wxRadioButton(this, wxID_ANY, wxT("Value"), wxDefaultPosition, wxDefaultSize,
		wxRB_GROUP), 0, wxGROW);
	top_s->Add(arbitrary = new wxRadioButton(this, wxID_ANY, wxT("Expression"), wxDefaultPosition, wxDefaultSize,
		0), 0, wxGROW);
	top_s->Add(typesel = new wxComboBox(this, wxID_ANY, types[0], wxDefaultPosition, wxDefaultSize,
		8, types, wxCB_READONLY), 0, wxGROW);
	top_s->Add(busaddr = new wxRadioButton(this, wxID_ANY, wxT("Bus address"), wxDefaultPosition, wxDefaultSize,
		wxRB_GROUP), 0, wxGROW);
	top_s->Add(mapaddr = new wxRadioButton(this, wxID_ANY, wxT("Map address"), wxDefaultPosition, wxDefaultSize,
		0), 0, wxGROW);
	top_s->Add(addr = new wxTextCtrl(this, wxID_ANY, towxstring(""), wxDefaultPosition, wxSize(200, -1)), 0,
		wxGROW);
	top_s->Add(hex = new wxCheckBox(this, wxID_ANY, towxstring("Hex formatting")), 0, wxGROW);
	top_s->Add(watchtxt = new wxTextCtrl(this, wxID_ANY, towxstring(expr), wxDefaultPosition, wxSize(400, -1)), 1,
		wxGROW);
	structured->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wxeditor_watchexpr::on_rb_structured), NULL, this);
	arbitrary->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wxeditor_watchexpr::on_rb_arbitrary), NULL, this);
	busaddr->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wxeditor_watchexpr::on_rb_busaddr), NULL, this);
	mapaddr->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wxeditor_watchexpr::on_rb_mapaddr), NULL, this);
	addr->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxeditor_watchexpr::on_addr_change), NULL, this);
	addr->SetMaxLength(16);
	watchtxt->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxeditor_watchexpr::on_addr_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("Ok")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")), 0, wxGROW);
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_watchexpr::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_watchexpr::on_cancel), NULL, this);
	top_s->Add(pbutton_s);

	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	regex_results r;
	if(expr == "") {
		structured->SetValue(true);
		busaddr->SetValue(our_rom->rtype->get_bus_map().second);
		mapaddr->SetValue(!our_rom->rtype->get_bus_map().second);
		busaddr->Enable(our_rom->rtype->get_bus_map().second);
		watchtxt->Disable();
	} else if(r = regex("C0x([0-9A-Fa-f]{1,16})z([bBwWdDqQ])(H([0-9A-Ga-g]))?", expr)) {
		structured->SetValue(true);
		mapaddr->SetValue(true);
		busaddr->Enable(our_rom->rtype->get_bus_map().second);
		watchtxt->Disable();
		std::string addr2 = r[1];
		char* end;
		char buf[512];
		std::copy(addr2.begin(), addr2.end(), buf);
		uint64_t parsed = strtoull(buf, &end, 16);
		auto r2 = our_rom->rtype->get_bus_map();
		if(parsed >= r2.first && parsed < r2.first + r2.second) {
			parsed -= r2.first;
			busaddr->SetValue(true);
		}
		addr->SetValue(towxstring((stringfmt() << std::hex << parsed).str()));
		typesel->SetSelection(memorywatch_recognize_typech(r[2][0]));
		hex->SetValue(r[3] != "");
	} else {
		arbitrary->SetValue(true);
		(our_rom->rtype->get_bus_map().second ? busaddr : mapaddr)->SetValue(true);
		mapaddr->Disable();
		busaddr->Disable();
		hex->Disable();
		watchtxt->Enable();
	}
	wxCommandEvent e;
	on_addr_change(e);
}

bool wxeditor_watchexpr::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_watchexpr::on_ok(wxCommandEvent& e)
{
	const char* letters = "bBwWdDqQ";
	const char* hexwidths = "224488GG";
	if(structured->GetValue()) {
		std::string hexmod;
		std::string addr2 = tostdstring(addr->GetValue());
		char* end;
		uint64_t parsed = strtoull(addr2.c_str(), &end, 16);
		if(busaddr->GetValue())
			parsed += our_rom->rtype->get_bus_map().first;
		if(hex->GetValue())
			hexmod = std::string("H") + hexwidths[typesel->GetSelection()];
		out = (stringfmt() << "C0x" << std::hex << parsed << "z" << letters[typesel->GetSelection()]
			<< hexmod).str();
	} else
		out = tostdstring(watchtxt->GetValue());
	EndModal(wxID_OK);
}

void wxeditor_watchexpr::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_watchexpr::on_rb_arbitrary(wxCommandEvent& e)
{
	typesel->Disable();
	busaddr->Disable();
	mapaddr->Disable();
	addr->Disable();
	hex->Disable();
	watchtxt->Enable();
	on_addr_change(e);
}

void wxeditor_watchexpr::on_rb_structured(wxCommandEvent& e)
{
	typesel->Enable();
	busaddr->Enable(our_rom->rtype->get_bus_map().second);
	mapaddr->Enable();
	addr->Enable();
	hex->Enable();
	watchtxt->Disable();
	on_addr_change(e);
}

void wxeditor_watchexpr::on_rb_busaddr(wxCommandEvent& e)
{
	on_addr_change(e);
}

void wxeditor_watchexpr::on_rb_mapaddr(wxCommandEvent& e)
{
	on_addr_change(e);
}

void wxeditor_watchexpr::on_addr_change(wxCommandEvent& e)
{
	if(structured->GetValue()) {
		std::string addr2 = tostdstring(addr->GetValue());
		if(!regex_match("[0-9A-Fa-f]{1,16}", addr2)) {
			ok->Enable(false);
			return;
		}
		char* end;
		uint64_t parsed = strtoull(addr2.c_str(), &end, 16);
		if(busaddr->GetValue() && parsed >= our_rom->rtype->get_bus_map().second) {
			ok->Enable(false);
			return;
		}
		ok->Enable(true);
	} else
		ok->Enable(tostdstring(watchtxt->GetValue()) != "");
}


std::string wxeditor_watchexpr::get_expr()
{
	return out;
}


std::string memorywatch_edit_watchexpr(wxWindow* parent, const std::string& name, const std::string& expr)
{
	wxeditor_watchexpr* d = new wxeditor_watchexpr(parent, name, expr);
	if(d->ShowModal() != wxID_OK) {
		d->Destroy();
		throw canceled_exception();
	}
	std::string out = d->get_expr();
	d->Destroy();
	return out;
}

class wxeditor_memorywatch : public wxDialog
{
public:
	wxeditor_memorywatch(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_memorywatch_change(wxCommandEvent& e);
	void on_new(wxCommandEvent& e);
	void on_rename(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
private:
	void refresh();
	wxListBox* watches;
	wxButton* newbutton;
	wxButton* renamebutton;
	wxButton* deletebutton;
	wxButton* editbutton;
	wxButton* closebutton;
};


wxeditor_memorywatch::wxeditor_memorywatch(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit memory watches"), wxDefaultPosition, wxSize(-1, -1))
{
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(watches = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 300)), 1, wxGROW);
	watches->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_memorywatch::on_memorywatch_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(newbutton = new wxButton(this, wxID_ANY, wxT("New")), 0, wxGROW);
	pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
	pbutton_s->Add(renamebutton = new wxButton(this, wxID_ANY, wxT("Rename")), 0, wxGROW);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	pbutton_s->Add(closebutton = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
	newbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::on_new), NULL, this);
	editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::on_edit), NULL, this);
	renamebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::on_rename), NULL, this);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::on_delete), NULL, this);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_memorywatch::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	refresh();
}

bool wxeditor_memorywatch::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_memorywatch::on_memorywatch_change(wxCommandEvent& e)
{
	std::string watch = tostdstring(watches->GetStringSelection());
	editbutton->Enable(watch != "");
	deletebutton->Enable(watch != "");
	renamebutton->Enable(watch != "");
}

void wxeditor_memorywatch::on_new(wxCommandEvent& e)
{
	try {
		std::string newname = pick_text(this, "New watch", "Enter name for watch:");
		if(newname == "")
			return;
		std::string newtext = memorywatch_edit_watchexpr(this, newname, "");
		if(newtext != "")
			runemufn([newname, newtext]() { set_watchexpr_for(newname, newtext); });
		refresh();
	} catch(canceled_exception& e) {
		//Ignore.
	}
	on_memorywatch_change(e);
}

void wxeditor_memorywatch::on_rename(wxCommandEvent& e)
{
	std::string watch = tostdstring(watches->GetStringSelection());
	if(watch == "")
		return;
	try {
		bool exists = false;
		std::string newname = pick_text(this, "Rename watch", "Enter New name for watch:");
		runemufn([watch, newname, &exists]() {
			std::string x = get_watchexpr_for(watch);
			std::string y = get_watchexpr_for(newname);
			if(y != "")
				exists = true;
			else {
				set_watchexpr_for(watch, "");
				set_watchexpr_for(newname, x);
			}
		});
		if(exists)
			show_message_ok(this, "Error", "The target watch already exists", wxICON_EXCLAMATION);
		refresh();
	} catch(canceled_exception& e) {
		//Ignore.
	}
	on_memorywatch_change(e);
}

void wxeditor_memorywatch::on_delete(wxCommandEvent& e)
{
	std::string watch = tostdstring(watches->GetStringSelection());
	if(watch != "")
		runemufn([watch]() { set_watchexpr_for(watch, ""); });
	refresh();
	on_memorywatch_change(e);
}

void wxeditor_memorywatch::on_edit(wxCommandEvent& e)
{
	std::string watch = tostdstring(watches->GetStringSelection());
	if(watch == "")
		return;
	try {
		std::string wtxt;
		runemufn([watch, &wtxt]() { wtxt = get_watchexpr_for(watch); });
		wtxt = memorywatch_edit_watchexpr(this, watch, wtxt);
		if(wtxt != "")
			runemufn([watch, wtxt]() { set_watchexpr_for(watch, wtxt); });
		refresh();
	} catch(canceled_exception& e) {
		//Ignore.
	}
	on_memorywatch_change(e);
}

void wxeditor_memorywatch::on_close(wxCommandEvent& e)
{
	EndModal(wxID_OK);
}

void wxeditor_memorywatch::refresh()
{
	std::map<std::string, std::string> bind;
	runemufn([&bind]() { 
		std::set<std::string> x = get_watches();
		for(auto i : x)
			bind[i] = get_watchexpr_for(i);
	});
	watches->Clear();
	for(auto i : bind)
		watches->Append(towxstring(i.first));
	if(watches->GetCount())
		watches->SetSelection(0);
	wxCommandEvent e;
	on_memorywatch_change(e);
}

void wxeditor_memorywatch_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_memorywatch(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
