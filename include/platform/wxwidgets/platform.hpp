#ifndef _plat_wxwidgets__platform__hpp__included__
#define _plat_wxwidgets__platform__hpp__included__

#include "core/queue.hpp"
#include "core/moviefile.hpp"
#include "core/window.hpp"

#include <cstdlib>
#include <cstdint>
#include <cstddef>

#include <wx/string.h>

class wxwin_mainwindow;
class wxwin_messages;
class wxwin_status;
class wxWindow;
class wxKeyEvent;
class emulator_instance;

//Scaling
extern double video_scale_factor;
extern int scaling_flags;
extern bool arcorrect_enabled;
extern bool hflip_enabled;
extern bool vflip_enabled;
extern bool rotate_enabled;
extern int wx_escape_count;

wxString towxstring(const std::string& str) throw(std::bad_alloc);
std::string tostdstring(const wxString& str) throw(std::bad_alloc);
wxString towxstring(const std::u32string& str) throw(std::bad_alloc);
std::u32string tou32string(const wxString& str) throw(std::bad_alloc);
void bring_app_foreground();
std::string pick_archive_member(wxWindow* parent, const std::string& filename) throw(std::bad_alloc);
void boot_emulator(emulator_instance& inst, loaded_rom& rom, moviefile& movie, bool fscreen);
void handle_wx_keyboard(emulator_instance& inst, wxKeyEvent& e, bool polarity);
std::string map_keycode_to_key(int kcode);
void initialize_wx_keyboard();
void deinitialize_wx_keyboard();
void signal_program_exit();
void signal_resize_needed();
void _runuifun_async(void (*fn)(void*), void* arg);
void show_projectwindow(wxWindow* modwin, emulator_instance& inst);
void signal_core_change();
void do_save_configuration();

std::vector<interface_action_paramval> prompt_action_params(wxWindow* parent, const std::string& label,
	const std::list<interface_action_param>& params);


//Editor dialogs.
void wxeditor_authors_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_hotkeys_display(wxWindow* parent);
void wxeditor_memorywatches_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_subtitles_display(wxWindow* parent, emulator_instance& inst);
std::string wxeditor_keyselect(wxWindow* parent, bool clearable);
void show_wxeditor_voicesub(wxWindow* parent, emulator_instance& inst);
void open_rom_select_window();
void open_new_project_window(wxWindow* parent, emulator_instance& inst);
void show_conflictwindow(wxWindow* parent);
void open_vumeter_window(wxWindow* parent, emulator_instance& inst);
void wxeditor_movie_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_movie_update(emulator_instance& inst);
void wxeditor_autohold_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_tasinput_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_macro_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_hexedit_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_multitrack_display(wxWindow* parent, emulator_instance& inst);
bool wxeditor_plugin_manager_display(wxWindow* parent);
void wxeditor_tracelog_display(wxWindow* parent, emulator_instance& inst, int cpuid, const std::string& cpuname);
void wxeditor_disassembler_display(wxWindow* parent, emulator_instance& inst);
void wxeditor_plugin_manager_notify_fail(const std::string& libname);

//Auxillary windows.
void wxwindow_memorysearch_display(emulator_instance& inst);
void wxwindow_memorysearch_update(emulator_instance& inst);
void wxeditor_hexeditor_update(emulator_instance& inst);
class memory_search;
memory_search* wxwindow_memorysearch_active(emulator_instance& inst);
bool wxeditor_hexeditor_available(emulator_instance& inst);
bool wxeditor_hexeditor_jumpto(emulator_instance& inst, uint64_t addr);
void wxwindow_tasinput_update(emulator_instance& inst);

template<typename T>
void runuifun(T fn)
{
	_runuifun_async(functor_call_helper2<T>, new T(fn));
}

//Thrown by various dialog functions if canceled.
class canceled_exception : public std::runtime_error
{
public:
	canceled_exception();
};

//Prompt for stuff. These all can throw canceled_exception.
std::string pick_file_member(wxWindow* parent, const std::string& title, const std::string& startdir);
std::string pick_among(wxWindow* parent, const std::string& title, const std::string& prompt,
	const std::vector<std::string>& choices, unsigned defaultchoice = 0);
std::string pick_text(wxWindow* parent, const std::string& title, const std::string& prompt,
	const std::string& dflt = "", bool multiline = false);
//Show message box with OK button.
void show_message_ok(wxWindow* parent, const std::string& title, const std::string& text, int icon);

//Run function and show errors. Returns true on error.

bool run_show_error(wxWindow* parent, const std::string& title, const std::string& text, std::function<void()> fn);
void show_exception(wxWindow* parent, const std::string& title, const std::string& text, std::exception& e);
void show_exception_any(wxWindow* parent, const std::string& title, const std::string& text, std::exception& e);

//Some important windows (if open).
extern wxwin_messages* msg_window;
extern wxwin_mainwindow* main_window;
extern std::string our_rom_name;
extern bool wxwidgets_exiting;

//Some important settings.
extern std::map<std::string, std::string> core_selections;


#endif
