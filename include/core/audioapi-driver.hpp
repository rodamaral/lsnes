#ifndef _audioapi_driver__hpp__included__
#define _audioapi_driver__hpp__included__

#include <stdexcept>
#include <string>
#include <map>

class audioapi_instance;

//All the following need to be implemented by the sound driver itself
struct _audioapi_driver
{
	//These correspond to various audioapi_driver_* functions.
	void (*init)() throw();
	void (*quit)() throw();
	void (*enable)(bool enable);
	bool (*initialized)();
	void (*set_device)(const std::string& pdev, const std::string& rdev);
	std::string (*get_device)(bool rec);
	std::map<std::string, std::string> (*get_devices)(bool rec);
	const char* (*name)();
};

struct audioapi_driver
{
	audioapi_driver(struct _audioapi_driver driver);
};


/**
 * Initialize the driver.
 */
void audioapi_driver_init() throw();

/**
 * Deinitialize the driver.
 */
void audioapi_driver_quit() throw();

/**
 * Enable or disable sound.
 *
 * parameter enable: Enable sounds if true, otherwise disable sounds.
 */
void audioapi_driver_enable(bool enable) throw();

/**
 * Has the sound system been successfully initialized?
 *
 * Returns: True if sound system has successfully initialized, false otherwise.
 */
bool audioapi_driver_initialized();

/**
 * Set sound device (playback).
 *
 * - If new sound device is invalid, the sound device is not changed.
 *
 * Parameter pdev: The new sound device (playback).
 * Parameter rdev: The new sound device (recording)
 */
void audioapi_driver_set_device(const std::string& pdev, const std::string& rdev) throw(std::bad_alloc,
	 std::runtime_error);

/**
 * Get current sound device (playback).
 *
 * Returns: The current sound device.
 */
std::string audioapi_driver_get_device(bool rec) throw(std::bad_alloc);

/**
 * Get available sound devices (playback).
 *
 * Returns: The map of devices. Keyed by name of the device, values are human-readable names for devices.
 */
std::map<std::string, std::string> audioapi_driver_get_devices(bool rec) throw(std::bad_alloc);

/**
 * Identification for sound plugin.
 */
const char* audioapi_driver_name() throw();

/**
 * Add an instance to be mixed.
 */
void audioapi_connect_instance(audioapi_instance& instance);

/**
 * Remove an instance from being mixed.
 */
void audioapi_disconnect_instance(audioapi_instance& instance);

/**
 * Send a rate change.
 */
void audioapi_send_rate_change(unsigned rrate, unsigned prate);

/**
 * Broadcast voice input to all instances.
 */
void audioapi_put_voice(float* samples, size_t count);

/**
 * Get mixed music + voice out from all instances.
 */
void audioapi_get_mixed(int16_t* samples, size_t count, bool stereo);

#endif
