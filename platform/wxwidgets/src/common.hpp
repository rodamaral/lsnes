#ifndef _wxwidgets_common__hpp__included__
#define _wxwidgets_common__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>

wxString towxstring(const std::string& str);
std::string tostdstring(const wxString& str);
extern long primary_window_style;
extern long secondary_window_style;
typedef wxStaticText* wxStaticText_ptr;

extern wxFrame* window1;
extern wxFrame* window2;

template<class T>
void cmdbutton_action(T* target, wxButton* button, void (T::*fptr)(wxCommandEvent& e))
{
	button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)(wxEventFunction)
		static_cast<wxCommandEventFunction>(fptr), NULL, target);
}

template<class T>
void menu_action(T* target, int id, void (T::*fptr)(wxCommandEvent& e))
{
	target->Connect(id, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)(wxEventFunction)
		static_cast<wxCommandEventFunction>(fptr), NULL, target);
}

template<class T>
void menu_action(T* target, int id, void (T::*fptr)(wxCommandEvent& e), std::string subcmd)
{
	target->Connect(id, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)(wxEventFunction)
		static_cast<wxCommandEventFunction>(fptr), NULL, target);
}

#define CNAME_NONE "None"
#define CNAME_GAMEPAD "Gamepad"
#define CNAME_MULTITAP "Multitap"
#define CNAME_MOUSE "Mouse"
#define CNAME_SUPERSCOPE "Superscope"
#define CNAME_JUSTIFIER "Justifier"
#define CNAME_JUSTIFIERS "2 Justifiers"

#endif
