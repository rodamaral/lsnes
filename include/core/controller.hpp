#ifndef _controller__hpp__included__
#define _controller__hpp__included__

#include "controllerframe.hpp"

void reread_active_buttons();
void reinitialize_buttonmap();
extern controller_state controls;
extern std::map<std::string, std::string> button_keys;

#endif
