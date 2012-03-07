#ifndef _threaddebug__hpp__included__
#define _threaddebug__hpp__included__

#define THREAD_OTHER -1
#define THREAD_MAIN -2
#define THREAD_EMULATION 0
#define THREAD_UI 1
#define THREAD_JOYSTICK 2

void assert_thread(signed shouldbe);
void mark_thread_as(signed call_me);
void init_threaded_malloc();

#endif