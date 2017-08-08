#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/cmdline.h>
#include "platform/wxwidgets/loadsave.hpp"

single_type::single_type(const std::string& _ext, const std::string& _desc)
{
	ext = _ext;
	if(_desc != "")
		desc = _desc;
	else if(_ext != "")
		desc = _ext + " files";
	else
		desc = "All files";
}

filedialog_input_params single_type::input(bool save) const
{
	filedialog_input_params p;
	if(ext != "")
		p.types.push_back(filedialog_type_entry(desc, "*."+ext, ext));
	p.types.push_back(filedialog_type_entry("All files", "", ""));
	p.default_type = 0;
	return p;
}

std::string single_type::output(const filedialog_output_params& p, bool save) const
{
	return p.path;
}

filedialog_input_params lua_script_type::input(bool save) const
{
	filedialog_input_params p;
	p.types.push_back(filedialog_type_entry("Lua scripts", "*.lua", "lua"));
	p.types.push_back(filedialog_type_entry("Packed lua scripts", "*.zlua", "zlua"));
	p.types.push_back(filedialog_type_entry("All files", "", ""));
	p.default_type = 0;
	return p;
}

std::string lua_script_type::output(const filedialog_output_params& p, bool save) const
{
	if(p.typechoice == 1)
		return p.path + "/main.lua";
	else
		return p.path;
}


filedialog_output_params show_filedialog(wxWindow* parent, const std::string& title, const std::string& basepath,
	const filedialog_input_params& p, const std::string& defaultname, bool saving)
{
	CHECK_UI_THREAD;
	wxString _title = towxstring(title);
	wxString _startdir = towxstring(basepath);
	std::string filespec;
	for(auto i : p.types) {
		if(filespec != "")
			filespec = filespec + "|";
		if(i.extensions != "")
			filespec = filespec + i.name + " (" + i.extensions + ")|" + i.extensions;
		else
			filespec = filespec + i.name + "|*";
	}
	wxFileDialog* d = new wxFileDialog(parent, _title, _startdir, wxT(""), towxstring(filespec), saving ?
		wxFD_SAVE : wxFD_OPEN);
	if(defaultname != "")
		d->SetFilename(towxstring(defaultname));
	d->SetFilterIndex(p.default_type);
	if(p.default_filename != "")
		d->SetFilename(towxstring(p.default_filename));
	if(d->ShowModal() == wxID_CANCEL)
		throw canceled_exception();
	std::string filename = tostdstring(d->GetPath());
	int findex = d->GetFilterIndex();
	d->Destroy();
	if(filename == "")
		throw canceled_exception();
	if(saving && p.types[findex].primaryext != "") {
		//Append extension if needed.
		std::string ext = p.types[findex].primaryext;
		size_t dpos = filename.find_last_of(".");
		std::string extension;
		if(dpos < filename.length()) {
			extension = filename.substr(dpos + 1);
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		}
		if(extension != ext)
			filename = filename + "." + ext;
	}
	filedialog_output_params r;
	r.path = filename;
	r.typechoice = findex;
	return r;
}

lua_script_type filetype_lua_script;
single_type filetype_macro("lmc", "Macro files");
single_type filetype_watch("lwch", "Memory watch");
single_type filetype_commentary("lsvs", "Commentary track");
single_type filetype_sox("sox", "SoX file");
single_type filetype_sub("sub", "Microsub subtitles");
single_type filetype_png("png", "Portable Network Graphics");
single_type filetype_hexbookmarks("lhb", "Hex editor bookmarks");
single_type filetype_memorysearch("lms", "Memory search save");
single_type filetype_textfile("txt", "Text file");
single_type filetype_trace("trace", "Trace file");
single_type filetype_font("font", "Font file");
single_type filetype_disassembly("asm", "Disassembly");
