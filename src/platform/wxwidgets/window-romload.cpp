#include "platform/wxwidgets/window-romload.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "interface/romtype.hpp"
#include "core/misc.hpp"
#include <string>
#include <sstream>
#include <map>


wxwindow_romload::wxwindow_romload(const std::string& _path)
{
	path = _path;
}

bool wxwindow_romload::show(wxWindow* parent)
{
	CHECK_UI_THREAD;
	std::map<int, std::string> cores;
	std::map<int, std::string> types;
	std::map<int, std::string> exts;
	std::string spec = "All Files (*)|*.*";
	int nent = 1;

	//Collect all extensions.
	for(auto i : core_type::get_core_types())
	{
		cores[nent] = i->get_core_identifier();
		types[nent] = i->get_hname();
		bool f = true;
		for(auto j : i->get_extensions()) {
			if(!f)
				exts[nent] = exts[nent] + ";";
			f = false;
			exts[nent] = exts[nent] + "*." + j;
		}
		if(exts[nent] == "")
			continue;
		spec = spec + "|" + mangle_name(cores[nent]) + " / " + mangle_name(types[nent]) +
			"(" + exts[nent] + ")|" + exts[nent];
		nent++;
	}
	wxFileDialog* d = new wxFileDialog(parent, towxstring("Choose ROM to load"), towxstring(path),
		towxstring(""), towxstring(spec), wxFD_OPEN);
	int r = d->ShowModal();
	if(r == wxID_CANCEL) {
		delete d;
		return false;
	}
	filename = tostdstring(d->GetPath());
	int idx = d->GetFilterIndex();
	if(idx > 0) {
		core = cores[idx];
		type = types[idx];
	}
	delete d;
	return true;
}
