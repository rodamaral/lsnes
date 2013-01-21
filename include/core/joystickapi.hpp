#ifndef _joystickapi__hpp__included__
#define _joystickapi__hpp__included__

#ifdef JOYSTICK_WEAK
#define JOYSTICK_DRV_ATTRIBUTE __attribute__((weak))
#else
#define JOYSTICK_DRV_ATTRIBUTE
#endif

/**
 * Joystick initialization function.
 *
 * - The third initialization function to be called by window_init().
 * - The call occurs in the main thread.
 * - Implemented by the joystick plugin.
 */
void joystick_driver_init() throw() JOYSTICK_DRV_ATTRIBUTE ;
/**
 * Joystick quit function.
 *
 * - The third last quit function to be called by window_quit().
 * - The call occurs in the main thread.
 * - Implemented by the joystick plugin.
 */
void joystick_driver_quit() throw() JOYSTICK_DRV_ATTRIBUTE ;
/**
 * This thread becomes the joystick polling thread.
 *
 * - Called in joystick polling thread.
 */
void joystick_driver_thread_fn() throw() JOYSTICK_DRV_ATTRIBUTE ;
/**
 * Signal the joystick thread to quit.
 */
void joystick_driver_signal() throw() JOYSTICK_DRV_ATTRIBUTE ;
/**
 * Identification for joystick plugin.
 */
extern const char* joystick_driver_name JOYSTICK_DRV_ATTRIBUTE ;

#endif


