#ifndef _audioapi__hpp__included__
#define _audioapi__hpp__included__

#include <map>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <stdexcept>

/**
 * Audio API music buffer.
 */
struct audioapi_buffer
{
/**
 * The samples.
 *
 * Note: May be NULL if no samples are available..
 */
	int16_t* samples;
/**
 * Playback pointer in samples structure.
 */
	size_t pointer;
/**
 * Total number of samples in this buffer.
 */
	size_t total;
/**
 * True if buffer is stereo, false if mono.
 */
	bool stereo;
/**
 * The rate in samples per second the buffer is supposed to be played at.
 */
	double rate;
};

//Resampler.
class audioapi_resampler
{
public:
	audioapi_resampler();
	//After call, either insize or outsize is zero.
	void resample(float*& in, size_t& insize, float*& out, size_t& outsize, double ratio, bool stereo);
private:
	double position;
	double vAl, vBl, vCl, vDl, vAr, vBr, vCr, vDr;
};

//The following are intended to be used by the emulator core.

/** 
 * Submit a buffer for playback on music channel.
 *
 * Parameter samples: The samples in the buffer. If stereo is true, each sample takes two elements (L, R).
 * Parameter count: The number of samples in buffer.
 * Parameter stereo: If true, the signal is stereo. If false, mono.
 * Parameter rate: Rate of buffer in samples per second.
 */
void audioapi_submit_buffer(int16_t* samples, size_t count, bool stereo, double rate);

/**
 * Get the voice channel playback/record rate.
 *
 * Returns: The rate in samples per second.
 */
unsigned audioapi_voice_rate();

/**
 * Get the voice channel playback status register.
 *
 * Returns: The number of samples free for playback.
 */
unsigned audioapi_voice_p_status();

/**
 * Get the voice channel playback status register2.
 *
 * Returns: The number of samples in playback buffer.
 */
unsigned audioapi_voice_p_status2();

/**
 * Get the voice channel record status register.
 *
 * Returns: The number of samples in capture buffer.
 */
unsigned audioapi_voice_r_status();

/**
 * Play sound on voice channel.
 *
 * Parameter samples: The samples to play.
 * Parameter count: Number of samples to play. Must be less than number of samples free for playback.
 */
void audioapi_play_voice(float* samples, size_t count);

/**
 * Capture sound on voice channel.
 *
 * Parameter samples: The buffer to store captured samples to.
 * Parameter count: Number of samples to capture. Must be less than number of samples used for capture.
 */
void audioapi_record_voice(float* samples, size_t count);

/**
 * Init the audio. Call on emulator startup.
 */
void audioapi_init();

/**
 * Quit the audio. Call on emulator shutdown.
 */
void audioapi_quit();

/**
 * Set music volume.
 *
 * Parameter volume: The volume (0-1).
 */
void audioapi_music_volume(float volume);

/**
 * Get music volume.
 *
 * Returns: The music volume.
 */
float audioapi_music_volume();

/**
 * Set voice playback volume.
 *
 * Parameter volume: The volume (0-1).
 */
void audioapi_voicep_volume(float volume);

/**
 * Get voice playback volume.
 *
 * Returns: The voice playback volume.
 */
float audioapi_voicep_volume();

/**
 * Set voice capture volume.
 *
 * Parameter volume: The volume (0-1).
 */
void audioapi_voicer_volume(float volume);

/**
 * Get voice capture volume.
 *
 * Returns: The voice capture volume.
 */
float audioapi_voicer_volume();

//The following are intender to be used by the driver.

/**
 * Set dummy CB enable flag.
 *
 * Parameter enable: The new state for enable flag.
 *
 * Note: Dummy CB enable should be set when the audio driver itself does not have active CB running.
 * Note: After negative edge, reprogram voice rate.
 */
void audioapi_set_dummy_cb(bool enable);

//The following are intended to be used by the driver from the callback

/**
 * Get mixed music + voice buffer to play (at voice rate).
 *
 * Parameter samples: Buffer to store the samples to.
 * Parameter count: Number of samples to generate.
 * Parameter stereo: If true, return stereo buffer, else mono.
 */
void audioapi_get_mixed(int16_t* samples, size_t count, bool stereo);

/**
 * Get music channel buffer to play.
 *
 * Parameter played: Number of samples to ACK as played.
 * Returns: Buffer to play.
 *
 * Note: This should only be called from the sound driver.
 */
struct audioapi_buffer audioapi_get_music(size_t played);

/**
 * Get voice channel buffer to play.
 *
 * Parameter samples: The place to store the samples.
 * Parameter count: Number of samples to fill.
 *
 * Note: This should only be called from the sound driver.
 */
void audioapi_get_voice(float* samples, size_t count);

/**
 * Put recorded voice channel buffer.
 *
 * Parameter samples: The samples to send. Can be NULL to send silence.
 * Parameter count: Number of samples to send.
 *
 * Note: Even if audio driver does not support capture, one should send in silence.
 * Note: This should only be called from the sound driver.
 */
void audioapi_put_voice(float* samples, size_t count);

/**
 * Set the voice channel playback/record rate.
 *
 * Parameter rate: The rate in samples per second.
 *
 * Note: This should only be called from the sound driver.
 */
void audioapi_voice_rate(unsigned rate);

//All the following need to be implemented by the sound driver itself

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
 * Set sound device.
 *
 * - If new sound device is invalid, the sound device is not changed.
 *
 * Parameter dev: The new sound device.
 */
void audioapi_driver_set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error);

/**
 * Get current sound device.
 *
 * Returns: The current sound device.
 */
std::string audioapi_driver_get_device() throw(std::bad_alloc);

/**
 * Get available sound devices.
 *
 * Returns: The map of devices. Keyed by name of the device, values are human-readable names for devices.
 */
std::map<std::string, std::string> audioapi_driver_get_devices() throw(std::bad_alloc);

/**
 * Identification for sound plugin.
 */
extern const char* audioapi_driver_name;

#endif
