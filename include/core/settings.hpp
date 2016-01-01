#ifndef _settings__hpp__included__
#define _settings__hpp__included__

#include "library/settingvar.hpp"

extern settingvar::set lsnes_setgrp;

extern settingvar::supervariable<settingvar::model_path> SET_rompath;
extern settingvar::supervariable<settingvar::model_path> SET_moviepath;
extern settingvar::supervariable<settingvar::model_path> SET_firmwarepath;
extern settingvar::supervariable<settingvar::model_path> SET_slotpath;


#endif
