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

wxString towxstring(const std::string& str) throw(std::bad_alloc);
std::string tostdstring(const wxString& str) throw(std::bad_alloc);
void bring_app_foreground();
std::string pick_archive_member(wxWindow* parent, const std::string& filename) throw(std::bad_alloc);
void boot_emulator(loaded_rom& rom, moviefile& movie);
void handle_wx_keyboard(wxKeyEvent& e, bool polarity);
void initialize_wx_keyboard();
void signal_program_exit();
void _runuifun_async(void (*fn)(void*), void* arg);

//Editor dialogs.
void wxeditor_axes_display(wxWindow* parent);
void wxeditor_authors_display(wxWindow* parent);
void wxeditor_settings_display(wxWindow* parent);


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

//Some important windows (if open).
extern wxwin_messages* msg_window;
extern wxwin_status* status_window;
extern wxwin_mainwindow* main_window;


#endif
