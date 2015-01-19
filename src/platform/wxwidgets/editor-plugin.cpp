#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include "core/messages.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "library/directory.hpp"
#include "library/filelist.hpp"
#include "library/loadlib.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include <iostream>
#include <cerrno>
#include <cstring>
#if defined(_WIN32) || defined(_WIN64)
#else
#include <sys/stat.h>
#endif

//TODO: Handle plugin blacklist/killlist
namespace
{
	std::set<std::string> failed_plugins;
	std::string killlist_file = "/killlist";
	std::string blacklist_file = "/blacklist";

	std::string string_add_list(std::string a, std::string b)
	{
		if(a.length())
			return a + "," + b;
		else
			return b;
	}

	std::string get_name(std::string path)
	{
#if defined(_WIN32) || defined(_WIN64)
		const char* sep = "\\/";
#else
		const char* sep = "/";
#endif
		size_t p = path.find_last_of(sep);
		std::string name;
		if(p == std::string::npos)
			name = path;
		else
			name = path.substr(p + 1);
		return name;
	}

	std::string strip_extension(std::string tmp, std::string ext)
	{
		regex_results r = regex("(.*)\\." + ext, tmp);
		if(!r) return tmp;
		return r[1];
	}
}

class wxeditor_plugins : public wxDialog
{
public:
	wxeditor_plugins(wxWindow* parent);
	void on_selection_change(wxCommandEvent& e);
	void on_add(wxCommandEvent& e);
	void on_rename(wxCommandEvent& e);
	void on_enable(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_start(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
private:
	void reload_plugins();
	filelist& get_blacklist();
	filelist& get_killlist();
	wxListBox* plugins;
	wxButton* addbutton;
	wxButton* renamebutton;
	wxButton* enablebutton;
	wxButton* deletebutton;
	wxButton* startbutton;
	wxButton* closebutton;
	std::vector<std::pair<std::string, bool>> pluginstbl;
	std::string extension;
	std::string pathpfx;
};

wxeditor_plugins::wxeditor_plugins(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Plugin manager"), wxDefaultPosition, wxSize(-1, -1))
{
	CHECK_UI_THREAD;
	Center();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);
	pathpfx = get_config_path() + "/autoload";
	extension = loadlib::library::extension();

	top_s->Add(plugins = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 300)), 1, wxGROW);
	plugins->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_plugins::on_selection_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(addbutton = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
	pbutton_s->Add(renamebutton = new wxButton(this, wxID_ANY, wxT("Rename")), 0, wxGROW);
	pbutton_s->Add(enablebutton = new wxButton(this, wxID_ANY, wxT("Enable")), 0, wxGROW);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	pbutton_s->AddStretchSpacer();
	if(!parent)
		pbutton_s->Add(startbutton = new wxButton(this, wxID_ANY, wxT("Start")), 0, wxGROW);
	else
		startbutton = NULL;
	if(!parent)
		pbutton_s->Add(closebutton = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxGROW);
	else
		pbutton_s->Add(closebutton = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
	addbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_plugins::on_add), NULL,
		this);
	renamebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_plugins::on_rename), NULL,
		this);
	enablebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_plugins::on_enable), NULL,
		this);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_plugins::on_delete), NULL,
		this);
	if(startbutton)
		startbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_plugins::on_start),
		NULL, this);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_plugins::on_close), NULL,
		this);
	top_s->Add(pbutton_s, 0, wxGROW);
	reload_plugins();
	wxCommandEvent e;
	on_selection_change(e);
	Fit();
}

void wxeditor_plugins::reload_plugins()
{
	CHECK_UI_THREAD;
	int sel = plugins->GetSelection();
	std::string name;
	if(sel == wxNOT_FOUND || sel >= (ssize_t)pluginstbl.size())
		name = "";
	else
		name = pluginstbl[sel].first;

	if(!directory::ensure_exists(pathpfx)) {
		throw std::runtime_error("Can't create plugins directory");
	}
	auto dir = directory::enumerate(pathpfx, ".*\\." + extension);
	plugins->Clear();
	pluginstbl.clear();
	for(auto i : dir) {
		std::string name = get_name(i);
		regex_results r = regex("(.*)\\." + extension, name);
		if(!r) continue;
		//Blacklisted plugins should be listed as disabled. Killlisted plugins shouldn't appear at all.
		bool is_disabled = false;
		auto blacklist = get_blacklist().enumerate();
		auto killlist = get_killlist().enumerate();
		if(killlist.count(name))
			continue;
		is_disabled = blacklist.count(name);
		pluginstbl.push_back(std::make_pair(r[1], !is_disabled));
		std::string r1 = r[1];
		std::string attributes;
		if(is_disabled) attributes = string_add_list(attributes, "disabled");
		if(failed_plugins.count(r[1] + "." + extension)) attributes = string_add_list(attributes, "failed");
		if(attributes.length()) attributes = " (" + attributes + ")";
		plugins->Append(towxstring(r1 + attributes));
	}

	for(size_t i = 0; i < pluginstbl.size(); i++) {
		if(pluginstbl[i].first == name)
			plugins->SetSelection(i);
	}
	wxCommandEvent e;
	on_selection_change(e);
}

void wxeditor_plugins::on_selection_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = plugins->GetSelection();
	if(sel == wxNOT_FOUND || sel >= (ssize_t)pluginstbl.size()) {
		renamebutton->Enable(false);
		enablebutton->Enable(false);
		deletebutton->Enable(false);
	} else {
		enablebutton->SetLabel(towxstring(pluginstbl[sel].second ? "Disable" : "Enable"));
		renamebutton->Enable(true);
		enablebutton->Enable(true);
		deletebutton->Enable(true);
	}
}

void wxeditor_plugins::on_add(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string file = choose_file_load(this, "Choose plugin to add", ".",
			single_type(loadlib::library::extension(), loadlib::library::name()));
		std::string name = strip_extension(get_name(file), extension);
		std::string nname = pathpfx + "/" + name + "." + extension;
		bool overwrite_ok = false;
		bool first = true;
		int counter = 2;
		while(!overwrite_ok && directory::exists(nname)) {
			if(first) {
				wxMessageDialog* d3 = new wxMessageDialog(this,
					towxstring("Plugin '" + name  + "' already exists.\n\nOverwrite?"),
					towxstring("Plugin already exists"),
					wxYES_NO | wxCANCEL | wxNO_DEFAULT | wxICON_QUESTION);
				int r = d3->ShowModal();
				d3->Destroy();
				first = false;
				if(r == wxID_YES)
					break;
				if(r == wxID_CANCEL) {
					reload_plugins();
					return;
				}
			}
			nname = pathpfx + "/" + name + "(" + (stringfmt() << counter++).str() + ")." + extension;
		}
		std::string nnamet = nname + ".tmp";
		std::ifstream in(file, std::ios::binary);
		std::ofstream out(nnamet, std::ios::binary);
		if(!out) {
			show_message_ok(this, "Error", "Can't write file '" + nnamet + "'", wxICON_EXCLAMATION);
			reload_plugins();
			return;
		}
		if(!in) {
			remove(nnamet.c_str());
			show_message_ok(this, "Error", "Can't read file '" + file + "'", wxICON_EXCLAMATION);
			reload_plugins();
			return;
		}
		while(true) {
			char buf[4096];
			size_t r;
			r = in.readsome(buf, sizeof(buf));
			out.write(buf, r);
			if(!r)
				break;
		}
		if(!out) {
			remove(nnamet.c_str());
			show_message_ok(this, "Error", "Can't write file '" + nnamet + "'", wxICON_EXCLAMATION);
			reload_plugins();
			return;
		}
		//Set permissions.
#if defined(_WIN32) || defined(_WIN64)
#else
		struct stat s;
		if(stat(file.c_str(), &s) < 0)
			s.st_mode = 0644;
		if(s.st_mode & 0400) s.st_mode |= 0100;
		if(s.st_mode & 040) s.st_mode |= 010;
		if(s.st_mode & 04) s.st_mode |= 01;
		chmod(nnamet.c_str(), s.st_mode & 0777);
#endif
		if(directory::rename_overwrite(nnamet.c_str(), nname.c_str())) {
			remove(nnamet.c_str());
			show_message_ok(this, "Error", "Can't rename-over file '" + nname + "'",
				wxICON_EXCLAMATION);
			reload_plugins();
			return;
		}
		//The new plugin isn't failed.
		failed_plugins.erase(get_name(nname));
		//Nor is it on killlist/blacklist
		try { get_blacklist().remove(get_name(nname)); } catch(...) {}
		try { get_killlist().remove(get_name(nname)); } catch(...) {}
		reload_plugins();
	} catch(canceled_exception& e) {
	}
}

void wxeditor_plugins::on_rename(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = plugins->GetSelection();
	if(sel == wxNOT_FOUND || sel >= (ssize_t)pluginstbl.size())
		return;
	std::string name = pluginstbl[sel].first;
	std::string name2;
	try {
		name2 = pick_text(this, "Rename plugin to", "Enter new name for plugin", name, false);
	} catch(canceled_exception& e) {
		return;
	}
	std::string oname = pathpfx + "/" + name + "." + extension;
	std::string nname = pathpfx + "/" + name2 + "." + extension;
	if(oname != nname) {
		directory::rename_overwrite(oname.c_str(), nname.c_str());
	}
	pluginstbl[sel].first = name2;
	if(failed_plugins.count(name + "." + extension)) {
		failed_plugins.insert(name2 + "." + extension);
		failed_plugins.erase(name + "." + extension);
	} else
		failed_plugins.erase(name2 + "." + extension);
	try { get_blacklist().rename(name + "." + extension, name2 + "." + extension); } catch(...) {}
	reload_plugins();
}

void wxeditor_plugins::on_enable(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = plugins->GetSelection();
	if(sel == wxNOT_FOUND || sel >= (ssize_t)pluginstbl.size())
		return;
	try {
		if(pluginstbl[sel].second)
			get_blacklist().add(pluginstbl[sel].first + "." + extension);
		else
			get_blacklist().remove(pluginstbl[sel].first + "." + extension);
	} catch(std::exception& e) {
		show_message_ok(this, "Error", "Can't enable/disable plugin '" + pluginstbl[sel].first +
			"': " + e.what(), wxICON_EXCLAMATION);
		reload_plugins();
		return;
	}
	pluginstbl[sel].second = !pluginstbl[sel].second;
	reload_plugins();
}

void wxeditor_plugins::on_delete(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = plugins->GetSelection();
	if(sel == wxNOT_FOUND || sel >= (ssize_t)pluginstbl.size())
		return;
	std::string oname = pathpfx + "/" + pluginstbl[sel].first + "." + extension;
	if(remove(oname.c_str()) < 0) {
		//Killlist it then.
		try { get_killlist().add(pluginstbl[sel].first + "." + extension); } catch(...) {}
	}
	failed_plugins.erase(pluginstbl[sel].first + "." + extension);
	reload_plugins();
}

void wxeditor_plugins::on_start(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_OK);
}

void wxeditor_plugins::on_close(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}

filelist& wxeditor_plugins::get_blacklist()
{
	static filelist x(pathpfx + blacklist_file, pathpfx);
	return x;
}

filelist& wxeditor_plugins::get_killlist()
{
	static filelist x(pathpfx + killlist_file, pathpfx);
	return x;
}

bool wxeditor_plugin_manager_display(wxWindow* parent)
{
	CHECK_UI_THREAD;
	int r;
	modal_pause_holder* hld = NULL;
	try {
		if(parent)
			hld = new modal_pause_holder();
		wxDialog* editor;
		try {
			editor = new wxeditor_plugins(parent);
			r = editor->ShowModal();
		} catch(std::exception& e) {
			if(hld)
				messages << "Error opening plugin dialog: " << e.what() << std::endl;
			else
				std::cerr << "Error opening plugin dialog: " << e.what() << std::endl;
			if(hld) delete hld;
			return false;
		} catch(...) {
			if(hld) delete hld;
			return false;
		}
		editor->Destroy();
		if(hld) delete hld;
		return (r == wxID_OK);
	} catch(...) {
		if(hld) delete hld;
		return false;
	}
}

void wxeditor_plugin_manager_notify_fail(const std::string& libname)
{
	failed_plugins.insert(libname);
}
