#ifndef _platform__wxwidgets__loadsave__hpp__included__
#define _platform__wxwidgets__loadsave__hpp__included__

#include <string>
#include "platform/wxwidgets/platform.hpp"

struct filedialog_type_entry
{
	filedialog_type_entry(std::string _name, std::string _extensions, std::string _primaryext)
	{
		name = _name;
		extensions = _extensions;
		primaryext = _primaryext;
	}
	std::string name;
	std::string extensions;
	std::string primaryext;
};

struct filedialog_input_params
{
	std::vector<filedialog_type_entry> types;
	int default_type;
	std::string default_filename;
};

struct filedialog_output_params
{
	std::string path;
	int typechoice;
};

class single_type
{
public:
	typedef std::string returntype;
	single_type(const std::string& _ext, const std::string& _desc = "");
	filedialog_input_params input(bool save) const;
	std::string output(const filedialog_output_params& p, bool save) const;
private:
	std::string ext;
	std::string desc;
};

class lua_script_type
{
public:
	typedef std::string returntype;
	filedialog_input_params input(bool save) const;
	std::string output(const filedialog_output_params& p, bool save) const;
};

extern lua_script_type filetype_lua_script;
extern single_type filetype_macro;
extern single_type filetype_watch;
extern single_type filetype_commentary;
extern single_type filetype_sox;
extern single_type filetype_sub;
extern single_type filetype_png;
extern single_type filetype_hexbookmarks;
extern single_type filetype_memorysearch;
extern single_type filetype_textfile;
extern single_type filetype_trace;
extern single_type filetype_font;
extern single_type filetype_disassembly;

filedialog_output_params show_filedialog(wxWindow* parent, const std::string& title, const std::string& basepath,
	const filedialog_input_params& p, const std::string& defaultname, bool saving);

template<typename T>
typename T::returntype choose_file_load(wxWindow* parent, const std::string& title, const std::string& basepath,
	const T& types, const std::string& defaultname = "")
{
	filedialog_input_params p = types.input(false);
	filedialog_output_params q = show_filedialog(parent, title, basepath, p, defaultname, false);
	return types.output(q, false);
}

template<typename T>
typename T::returntype choose_file_save(wxWindow* parent, const std::string& title, const std::string& basepath,
	const T& types, const std::string& defaultname = "")
{
	filedialog_input_params p = types.input(true);
	filedialog_output_params q = show_filedialog(parent, title, basepath, p, defaultname, true);
	return types.output(q, true);
}


#endif
