#ifndef _plat_wxwidgets__platform__hpp__included__
#define _plat_wxwidgets__platform__hpp__included__

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

//Scaling
extern double horizontal_scale_factor;
extern double vertical_scale_factor;
extern int scaling_flags;

wxString towxstring(const std::string& str) throw(std::bad_alloc);
std::string tostdstring(const wxString& str) throw(std::bad_alloc);
void bring_app_foreground();
std::string pick_archive_member(wxWindow* parent, const std::string& filename) throw(std::bad_alloc);
void boot_emulator(loaded_rom& rom, moviefile& movie);
void handle_wx_keyboard(wxKeyEvent& e, bool polarity);
std::string map_keycode_to_key(int kcode);
void initialize_wx_keyboard();
void signal_program_exit();
void signal_resize_needed();
void _runuifun_async(void (*fn)(void*), void* arg);
void show_projectwindow(wxWindow* modwin);

//Editor dialogs.
void wxeditor_authors_display(wxWindow* parent);
void wxeditor_hotkeys_display(wxWindow* parent);
void wxeditor_memorywatch_display(wxWindow* parent);
void wxeditor_subtitles_display(wxWindow* parent);
std::string wxeditor_keyselect(wxWindow* parent, bool clearable);
void wxsetingsdialog_display(wxWindow* parent, bool hotkeys_only);
void show_wxeditor_voicesub(wxWindow* parent);
void open_rom_select_window();

//Auxillary windows.
void wxwindow_memorysearch_display();

template<typename T>
void functor_call_helper2(void* args)
{
	(*reinterpret_cast<T*>(args))();
	delete reinterpret_cast<T*>(args);
}

template<typename T>
void runuifun(T fn)
{
	_runuifun_async(functor_call_helper<T>, new T(fn));
}

//Thrown by various dialog functions if canceled.
class canceled_exception : public std::runtime_error
{
public:
	canceled_exception();
};

//Prompt for stuff. These all can throw canceled_exception.
std::string pick_file(wxWindow* parent, const std::string& title, const std::string& startdir, bool forsave);
std::string pick_file_member(wxWindow* parent, const std::string& title, const std::string& startdir);
std::string pick_among(wxWindow* parent, const std::string& title, const std::string& prompt,
	const std::vector<std::string>& choices);
std::string pick_text(wxWindow* parent, const std::string& title, const std::string& prompt,
	const std::string& dflt = "", bool multiline = false);
//Show message box with OK button.
void show_message_ok(wxWindow* parent, const std::string& title, const std::string& text, int icon);


//Some important windows (if open).
extern wxwin_messages* msg_window;
extern wxwin_mainwindow* main_window;
extern std::string our_rom_name;
extern bool wxwidgets_exiting;

#endif
