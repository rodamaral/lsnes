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

/**
 * Audio API VU calculator.
 */
struct audioapi_vumeter
{
/**
 * Initialize.
 */
	audioapi_vumeter();
/**
 * Submit samples.
 *
 * Parameter samples: The samples to submit. If NULL, reads all samples as 0.
 * Parameter count: Number of samples.
 * Parameter stereo: If true, read only every other sample (but still read count samples).
 * Parameter rate: Sound sampling rate.
 * Parameter scale: Value to scale the samples by.
 */
	void operator()(float* samples, size_t count, bool stereo, double rate, double scale);
/**
 * Get VU value in dB.
 */
	operator float() const throw() { return vu; }
private:
	double accumulator;
	size_t samples;
	float vu;
	void update_vu();
};

//VU values.
extern audioapi_vumeter audioapi_vu_mleft;
extern audioapi_vumeter audioapi_vu_mright;
extern audioapi_vumeter audioapi_vu_vout;
extern audioapi_vumeter audioapi_vu_vin;

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
 * Returns: The rate in samples per second (first for recording, then for playback).
 */
std::pair<unsigned,unsigned> audioapi_voice_rate();

/**
 * Get the voice channel nominal playback/record rate.
 *
 * Returns: The rate in samples per second.
 */
unsigned audioapi_orig_voice_rate();

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
 * Parameter rate_r: The recording rate in samples per second.
 * Parameter rate_p: The playback rate in samples per second.
 *
 * Note: This should only be called from the sound driver.
 * Note: Setting rate to 0 enables dummy callbacks.
 */
void audioapi_voice_rate(unsigned rate_r, unsigned rate_p);

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

#endif
