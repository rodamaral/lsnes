#ifndef _plat_wxwidgets__menu_upload__hpp__included__
#define _plat_wxwidgets__menu_upload__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>
#include <map>
#include <set>

class emulator_instance;

class upload_menu : public wxMenu
{
public:
	upload_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high);
	~upload_menu();
	void on_select(wxCommandEvent& e);
	struct upload_entry
	{
		std::string name;
		std::string url;
		enum _auth
		{
			AUTH_DH25519
		} auth;
		wxMenuItem* item;   //Internally used.
	};
	std::set<unsigned> entries();
	upload_entry get_entry(unsigned num);
	void configure_entry(unsigned num, struct upload_entry entry);
	void delete_entry(unsigned num);
private:
	void save();
	emulator_instance& inst;
	wxWindow* pwin;
	int wxid_range_low;
	int wxid_range_high;
	std::map<int, upload_entry> destinations;
};

#endif
