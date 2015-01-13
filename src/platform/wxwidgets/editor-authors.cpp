#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/ui-services.hpp"
#include "library/zip.hpp"

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"

class wxeditor_authors : public wxDialog
{
public:
	wxeditor_authors(wxWindow* parent, emulator_instance& _inst);
	bool ShouldPreventAppExit() const;
	void on_authors_change(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
	void on_dir_select(wxCommandEvent& e);
	void on_add(wxCommandEvent& e);
	void on_remove(wxCommandEvent& e);
	void on_down(wxCommandEvent& e);
	void on_up(wxCommandEvent& e);
	void on_luasel(wxCommandEvent& e);
private:
	void reorder_scripts(int delta);
	emulator_instance& inst;
	wxTextCtrl* gamename;
	wxTextCtrl* pname;
	wxTextCtrl* directory;
	wxTextCtrl* authors;
	wxTextCtrl* projectpfx;
	wxListBox* luascripts;
	wxCheckBox* autorunlua;
	wxButton* addbutton;
	wxButton* removebutton;
	wxButton* upbutton;
	wxButton* downbutton;
	wxButton* okbutton;
	wxButton* cancel;
};


wxeditor_authors::wxeditor_authors(wxWindow* parent, emulator_instance& _inst)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit game name & authors"), wxDefaultPosition, wxSize(-1, -1)),
	inst(_inst)
{
	CHECK_UI_THREAD;
	project_author_info ainfo = UI_load_author_info(inst);
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(ainfo.is_project ? 12 : 5, 1, 0, 0);
	SetSizer(top_s);

	if(ainfo.is_project) {
		wxFlexGridSizer* c4_s = new wxFlexGridSizer(1, 2, 0, 0);
		c4_s->Add(new wxStaticText(this, wxID_ANY, wxT("Project:")), 0, wxGROW);
		c4_s->Add(pname = new wxTextCtrl(this, wxID_ANY, towxstring(ainfo.projectname),
			wxDefaultPosition, wxSize(400, -1)), 1, wxGROW);
		top_s->Add(c4_s);

		wxFlexGridSizer* c2_s = new wxFlexGridSizer(1, 3, 0, 0);
		c2_s->Add(new wxStaticText(this, wxID_ANY, wxT("Directory:")), 0, wxGROW);
		c2_s->Add(directory = new wxTextCtrl(this, wxID_ANY, towxstring(ainfo.directory),
			wxDefaultPosition, wxSize(350, -1)), 1, wxGROW);
		wxButton* pdir;
		c2_s->Add(pdir = new wxButton(this, wxID_ANY, wxT("...")), 1, wxGROW);
		pdir->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_authors::on_dir_select), NULL, this);
		top_s->Add(c2_s);

		wxFlexGridSizer* c3_s = new wxFlexGridSizer(1, 2, 0, 0);
		c3_s->Add(new wxStaticText(this, wxID_ANY, wxT("Prefix:")), 0, wxGROW);
		c3_s->Add(projectpfx = new wxTextCtrl(this, wxID_ANY, towxstring(ainfo.prefix),
			wxDefaultPosition, wxSize(300, -1)), 1, wxGROW);
		top_s->Add(c3_s);
	} else {
		directory = NULL;
		pname = NULL;
		wxFlexGridSizer* c2_s = new wxFlexGridSizer(1, 2, 0, 0);
		c2_s->Add(new wxStaticText(this, wxID_ANY, wxT("Save slot prefix:")), 0, wxGROW);
		c2_s->Add(projectpfx = new wxTextCtrl(this, wxID_ANY, towxstring(ainfo.prefix),
			wxDefaultPosition, wxSize(300, -1)), 1, wxGROW);
		top_s->Add(c2_s);
	}

	wxFlexGridSizer* c_s = new wxFlexGridSizer(1, 2, 0, 0);
	c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Game name:")), 0, wxGROW);
	c_s->Add(gamename = new wxTextCtrl(this, wxID_ANY, towxstring(ainfo.gamename), wxDefaultPosition,
		wxSize(400, -1)), 1, wxGROW);
	top_s->Add(c_s);

	top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Authors (one per line):")), 0, wxGROW);
	top_s->Add(authors = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE), 0, wxGROW);
	authors->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxeditor_authors::on_authors_change), NULL, this);

	if(ainfo.is_project) {
		top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Autoload lua scripts:")), 0, wxGROW);
		top_s->Add(luascripts = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 100)), 1,
			wxEXPAND);
		luascripts->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxeditor_authors::on_luasel), NULL, this);

		wxFlexGridSizer* c3_s = new wxFlexGridSizer(1, 4, 0, 0);
		c3_s->Add(addbutton = new wxButton(this, wxID_ANY, wxT("Add")), 1, wxGROW);
		addbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_authors::on_add), NULL, this);
		c3_s->Add(removebutton = new wxButton(this, wxID_ANY, wxT("Remove")), 1, wxGROW);
		removebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_authors::on_remove), NULL, this);
		removebutton->Disable();
		c3_s->Add(upbutton = new wxButton(this, wxID_ANY, wxT("Up")), 1, wxGROW);
		upbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_authors::on_up), NULL, this);
		upbutton->Disable();
		c3_s->Add(downbutton = new wxButton(this, wxID_ANY, wxT("Down")), 1, wxGROW);
		downbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_authors::on_down), NULL, this);
		downbutton->Disable();
		top_s->Add(c3_s);
		top_s->Add(autorunlua = new wxCheckBox(this, wxID_ANY, wxT("Autorun added Lua scripts")), 0, wxGROW);
		autorunlua->SetValue(true);
	} else {
		luascripts = NULL;
		addbutton = NULL;
		removebutton = NULL;
		upbutton = NULL;
		downbutton = NULL;
		autorunlua = NULL;
	}

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_authors::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_authors::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	c_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	std::string x;
	for(auto i : ainfo.authors)
		x = x + i.first + "|" + i.second + "\n";
	for(auto i : ainfo.luascripts)
		luascripts->Append(towxstring(i));
	authors->SetValue(towxstring(x));
}

bool wxeditor_authors::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_authors::on_authors_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		size_t lines = authors->GetNumberOfLines();
		for(size_t i = 0; i < lines; i++) {
			std::string l = tostdstring(authors->GetLineText(i));
			if(l == "|")
				throw 43;
		}
		okbutton->Enable();
	} catch(...) {
		okbutton->Disable();
	}
}

void wxeditor_authors::on_cancel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}

void wxeditor_authors::on_ok(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	project_author_info ainfo;
	ainfo.gamename = tostdstring(gamename->GetValue());
	ainfo.prefix = tostdstring(projectpfx->GetValue());
	ainfo.directory = directory ? tostdstring(directory->GetValue()) : std::string("");
	ainfo.projectname = pname ? tostdstring(pname->GetValue()) : std::string("");

	size_t lines = authors->GetNumberOfLines();
	for(size_t i = 0; i < lines; i++) {
		std::string l = tostdstring(authors->GetLineText(i));
		if(l != "" && l != "|")
			ainfo.authors.push_back(split_author(l));
	}

	if(luascripts)
		for(unsigned i = 0; i < luascripts->GetCount(); i++)
			ainfo.luascripts.push_back(tostdstring(luascripts->GetString(i)));
	ainfo.autorunlua = autorunlua ? autorunlua->GetValue() : false;

	UI_save_author_info(inst, ainfo);
	EndModal(wxID_OK);
}

void wxeditor_authors::on_dir_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	wxDirDialog* d = new wxDirDialog(this, wxT("Select project directory"), directory->GetValue(),
		wxDD_DIR_MUST_EXIST);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	directory->SetValue(d->GetPath());
	d->Destroy();
}

void wxeditor_authors::on_add(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string luascript = choose_file_load(this, "Pick lua script", ".", filetype_lua_script);
		try {
			auto& p = zip::openrel(luascript, "");
			delete &p;
		} catch(std::exception& e) {
			show_message_ok(this, "File not found", "File '" + luascript + "' can't be opened",
				wxICON_EXCLAMATION);
			return;
		}
		luascripts->Append(towxstring(luascript));
	} catch(...) {
	}
}

void wxeditor_authors::on_remove(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = luascripts->GetSelection();
	int count = luascripts->GetCount();
	luascripts->Delete(sel);
	if(sel < count - 1)
		luascripts->SetSelection(sel);
	else if(count > 1)
		luascripts->SetSelection(count - 2);
	else
		luascripts->SetSelection(wxNOT_FOUND);
	on_luasel(e);
}

void wxeditor_authors::reorder_scripts(int delta)
{
	CHECK_UI_THREAD;
	int sel = luascripts->GetSelection();
	int count = luascripts->GetCount();
	if(sel == wxNOT_FOUND || sel + delta >= count || sel + delta < 0)
		return;
	wxString a = luascripts->GetString(sel);
	wxString b = luascripts->GetString(sel + delta);
	luascripts->SetString(sel, b);
	luascripts->SetString(sel + delta, a);
	luascripts->SetSelection(sel + delta);
}

void wxeditor_authors::on_up(wxCommandEvent& e)
{
	reorder_scripts(-1);
	on_luasel(e);
}

void wxeditor_authors::on_down(wxCommandEvent& e)
{
	reorder_scripts(1);
	on_luasel(e);
}

void wxeditor_authors::on_luasel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = luascripts->GetSelection();
	int count = luascripts->GetCount();
	removebutton->Enable(sel != wxNOT_FOUND);
	upbutton->Enable(sel != wxNOT_FOUND && sel > 0);
	downbutton->Enable(sel != wxNOT_FOUND && sel < count - 1);
}

void wxeditor_authors_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	modal_pause_holder hld;
	if(!*inst.mlogic) {
		show_message_ok(parent, "No movie", "Can't edit authors of nonexistent movie", wxICON_EXCLAMATION);
		return;
	}
	wxDialog* editor;
	try {
		editor = new wxeditor_authors(parent, inst);
		editor->ShowModal();
	} catch(...) {
		return;
	}
	editor->Destroy();
}
