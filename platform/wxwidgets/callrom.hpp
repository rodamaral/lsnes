#ifndef _wxwidgets_callrom__hpp__included__
#define _wxwidgets_callrom__hpp__included__

#include "rom.hpp"
#include <wx/string.h>
#include <string>

enum rom_type romtype_from_string(const std::string& str);
enum rom_type romtype_from_string(const wxString& str);
wxString romname_wxs(enum rom_type rtype, unsigned index);
std::string romname_stds(enum rom_type rtype, unsigned index);
unsigned fill_rom_names(enum rom_type rtype, wxString* array);
unsigned romname_to_index(enum rom_type rtype, const wxString& name);
struct loaded_slot& get_rom_slot(struct loaded_rom& rom, unsigned index);
enum rom_region region_from_string(const std::string& str);
enum rom_region region_from_string(const wxString& str);
bool has_forced_region(enum rom_type rtype);
bool has_forced_region(const std::string& str);
bool has_forced_region(const wxString& str);
wxString forced_region_for_romtype(enum rom_type rtype);
wxString forced_region_for_romtype(const std::string& str);
wxString forced_region_for_romtype(const wxString& str);
unsigned populate_region_choices(wxString* array);
unsigned populate_system_choices(wxString* array);

#endif