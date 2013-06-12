#ifndef _controller__hpp__included__
#define _controller__hpp__included__

#include "controllerframe.hpp"
#include "project.hpp"

struct project_info;

void reread_active_buttons();
void reinitialize_buttonmap();
void load_macros(controller_state& ctrlstate);
void load_project_macros(controller_state& ctrlstate, project_info& pinfo);

extern controller_state controls;
extern std::map<std::string, std::string> button_keys;


#endif
