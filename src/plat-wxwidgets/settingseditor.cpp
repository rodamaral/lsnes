#include "core/command.hpp"
#include "core/misc.hpp"
#include "core/moviefile.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"

#include "plat-wxwidgets/settingseditor.hpp"
#include "plat-wxwidgets/common.hpp"

#include <fstream>
#include <stdexcept>

wx_settings_editor_setting::wx_settings_editor_setting(wxSizer* sizer, wxWindow* window, const std::string& name)
{
	a_name = name;
	parent = window;
	std::string pvalue = "<unknown>";
	try {
		if(!setting::is_set(a_name))
			pvalue = "<unset>";
		else 
			pvalue = setting::get(a_name);
	} catch(...) {
	}
	sizer->Add(new wxStaticText(window, wxID_ANY, towxstring(name)), 0, wxGROW);
	sizer->Add(label = new wxStaticText(window, wxID_ANY, towxstring(pvalue)), 0, wxGROW);
	sizer->Add(edit = new wxButton(window, wxID_ANY, wxT("Edit")), 0, wxGROW);
	sizer->Add(clear = new wxButton(window, wxID_ANY, wxT("Clear")), 0, wxGROW);
	edit->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_settings_editor_setting::on_edit_click), NULL, this);
	clear->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_settings_editor_setting::on_clear_click), NULL, this);
}

void wx_settings_editor_setting::on_clear_click(wxCommandEvent& e)
{
	try {
		setting::blank(a_name);
	}catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(std::string("Can't clear setting: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		d->Destroy();
	}
}

void wx_settings_editor_setting::on_edit_click(wxCommandEvent& e)
{
	try {
		std::string newsetting;
		std::string oldvalue = setting::get(a_name);
		wxTextEntryDialog* d = new wxTextEntryDialog(parent, towxstring("Enter new value for " + a_name),
			wxT("Enter new value for setting"), towxstring(oldvalue));
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		newsetting = tostdstring(d->GetValue());
		setting::set(a_name, newsetting);
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(std::string("Can't set setting: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		d->Destroy();
	}
}

void wx_settings_editor_setting::change_setting(const std::string& _setting, const std::string& value)
{
	if(_setting != a_name)
		return;
	label->SetLabel(towxstring(value));
}

void wx_settings_editor_setting::clear_setting(const std::string& _setting)
{
	if(_setting != a_name)
		return;
	label->SetLabel(wxT("<unset>"));
}

wx_settings_editor::wx_settings_editor(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit settings"), wxDefaultPosition, wxSize(-1, -1)), listener(this)
{
	std::set<std::string> settings_set = setting::get_settings_set();

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);
	
	wxFlexGridSizer* t_s = new wxFlexGridSizer(settings_set.size(), 4, 0, 0);
	for(auto i : settings_set)
		esettings.push_back(new wx_settings_editor_setting(t_s, this, i));
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(close = new wxButton(this, wxID_CANCEL, wxT("Close")), 0, wxGROW);
	close->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_settings_editor::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wx_settings_editor::~wx_settings_editor()
{
	for(auto i : esettings)
		delete i;
}

bool wx_settings_editor::ShouldPreventAppExit() const
{
	return false;
}

void wx_settings_editor::on_close(wxCommandEvent& e)
{
	EndModal(wxID_OK);
}

void wx_settings_editor::change_setting(const std::string& _setting, const std::string& value)
{
	for(auto i : esettings)
		i->change_setting(_setting, value);
}

void wx_settings_editor::clear_setting(const std::string& _setting)
{
	for(auto i : esettings)
		i->clear_setting(_setting);
}

wx_settings_editor_listener::wx_settings_editor_listener(wx_settings_editor* _editor)
	: information_dispatch("wx-settings-editor-listener")
{
	editor = _editor;
}

wx_settings_editor_listener::~wx_settings_editor_listener() throw()
{
}

void wx_settings_editor_listener::on_setting_change(const std::string& _setting, const std::string& value)
{
	editor->change_setting(_setting, value);
}

void wx_settings_editor_listener::on_setting_clear(const std::string& _setting)
{
	editor->clear_setting(_setting);
}

namespace
{
	void load_settings(std::istream& in)
	{
		try {
			std::string l;
			while(std::getline(in, l)) {
				if(l.length() > 0 && l[l.length() - 1] == '\r')
					l = l.substr(0, l.length() - 1);
				command::invokeC(l);
			}
		} catch(std::exception& e) {
			messages << "Error loading config file: " << e.what();
		}
	}

	void save_settings(std::ostream& out)
	{
		//Axes
		try {
			std::set<std::string> axisnames = keygroup::get_axis_set();
			for(auto i : axisnames) {
				try {
					keygroup* k = keygroup::lookup_by_name(i);
					std::ostringstream str;
					str << "axismode " << i << " ";
					enum keygroup::type ctype = k->get_parameters().ktype;
					int16_t low = k->get_parameters().cal_left;
					int16_t mid = k->get_parameters().cal_center;
					int16_t high = k->get_parameters().cal_right;
					double tol = k->get_parameters().cal_tolerance;
					switch(ctype) {
					case keygroup::KT_DISABLED:
						str << "disabled ";
						break;
					case keygroup::KT_AXIS_PAIR:
						str << "axis ";
						break;
					case keygroup::KT_AXIS_PAIR_INVERSE:
						str << "axis-inverse ";
						break;
					case keygroup::KT_PRESSURE_0M:
						str << "pressure0- ";
						break;
					case keygroup::KT_PRESSURE_0P:
						str << "pressure0+ ";
						break;
					case keygroup::KT_PRESSURE_M0:
						str << "pressure-0 ";
						break;
					case keygroup::KT_PRESSURE_MP:
						str << "pressure-+ ";
						break;
					case keygroup::KT_PRESSURE_P0:
						str << "pressure+0 ";
						break;
					case keygroup::KT_PRESSURE_PM:
						str << "pressure+- ";
						break;
					};
					str << "minus=" << low << " zero=" << mid << " plus=" << high;
					out << str.str() << std::endl;
				} catch(std::exception& e) {
					messages << "Error saving axis " << i << ": " << e.what();
				}
			}
		} catch(std::exception& e) {
			messages << "Error saving axes: " << e.what();
		}
		//Aliases.
		try {
			std::set<std::string> bind = command::get_aliases();
			for(auto i : bind) {
				try {
					std::string old_alias_value = command::get_alias_for(i);
					size_t itr = 0;
					while(itr < old_alias_value.length()) {
						size_t nsplit = old_alias_value.find_first_of("\n", itr);
						if(nsplit >= old_alias_value.length()) {
							out << "alias-command " << i << " " << old_alias_value.substr(
								itr) << std::endl;
							itr = old_alias_value.length();
						} else {
							out << "alias-command " << i << " " << old_alias_value.substr(
								itr, nsplit - itr) << std::endl;
							itr = nsplit + 1;
						}
					}
				} catch(std::exception& e) {
					messages << "Error saving alias " << i << ": " << e.what();
				}
			}
		} catch(std::exception& e) {
			messages << "Error saving aliases: " << e.what();
		}
		//Keybindings
		try {
			std::set<std::string> bind = keymapper::get_bindings();
			for(auto i : bind) {
				try {
					std::ostringstream str;
					str << "bind-key ";
					std::string command = keymapper::get_command_for(i);
					std::string keyspec = i;
					size_t split1 = keyspec.find_first_of("/");
					size_t split2 = keyspec.find_first_of("|");
					if(split1 >= keyspec.length() || split2 >= keyspec.length() || split1 > split2)
						throw std::runtime_error("Bad keyspec " + keyspec);
					std::string a = keyspec.substr(0, split1);
					std::string b = keyspec.substr(split1 + 1, split2 - split1 - 1);
					std::string c = keyspec.substr(split2 + 1);
					if(a != "" || b != "")
						str << a << "/" << b << " ";
					str << c << " " << command;
					out << str.str() << std::endl;
				} catch(std::exception& e) {
					messages << "Error saving keybinding " << i << ": " << e.what();
				}
			}
		} catch(std::exception& e) {
			messages << "Error saving keybindings: " << e.what();
		}
		//Settings
		try {
			std::set<std::string> bind = setting::get_settings_set();
			for(auto i : bind) {
				try {
					if(setting::is_set(i))
						out << "set-setting " << i << " " << setting::get(i) << std::endl;
					else
						out << "unset-setting " << i << std::endl;
				} catch(std::exception& e) {
					messages << "Error saving setting " << i << ": " << e.what();
				}
			}
		} catch(std::exception& e) {
			messages << "Error saving settings: " << e.what();
		}
	}
}

void load_settings()
{
	std::string cfgpath = get_config_path();
	std::ifstream cfg(cfgpath + "/lsneswx.rc");
	if(!cfg) {
		messages << "Can't load settings file!" << std::endl;
		return;
	}
	load_settings(cfg);
	cfg.close();
}

void save_settings()
{
	std::string cfgpath = get_config_path();
	std::ofstream cfg(cfgpath + "/lsneswx.rc");
	if(!cfg) {
		messages << "Can't save settings!" << std::endl;
		return;
	}
	save_settings(cfg);
	cfg.close();
	
}
