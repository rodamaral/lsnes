#ifndef _audioapi__hpp__included__
#define _audioapi__hpp__included__

#include "library/threads.hpp"

#include <map>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <stdexcept>

class audioapi_instance
{
public:
/**
 * Audio API music buffer.
 */
	struct buffer
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
	struct vumeter
	{
/**
 * Initialize.
 */
		vumeter();
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
	//Resampler.
	class resampler
	{
	public:
		resampler();
		//After call, either insize or outsize is zero.
		void resample(float*& in, size_t& insize, float*& out, size_t& outsize, double ratio, bool stereo);
	private:
		double position;
		double vAl, vBl, vCl, vDl, vAr, vBr, vCr, vDr;
	};
/**
 * Ctor.
 */
	audioapi_instance();
	~audioapi_instance();
//The following are intended to be used by the emulator core.
/**
 * Submit a buffer for playback on music channel.
 *
 * Parameter samples: The samples in the buffer. If stereo is true, each sample takes two elements (L, R).
 * Parameter count: The number of samples in buffer.
 * Parameter stereo: If true, the signal is stereo. If false, mono.
 * Parameter rate: Rate of buffer in samples per second.
 */
	void submit_buffer(int16_t* samples, size_t count, bool stereo, double rate);
/**
 * Get the voice channel playback/record rate.
 *
 * Returns: The rate in samples per second (first for recording, then for playback).
 */
	std::pair<unsigned,unsigned> voice_rate();
/**
 * Get the voice channel nominal playback/record rate.
 *
 * Returns: The rate in samples per second.
 */
	unsigned orig_voice_rate();
/**
 * Get the voice channel playback status register.
 *
 * Returns: The number of samples free for playback.
 */
	unsigned voice_p_status();
/**
 * Get the voice channel playback status register2.
 *
 * Returns: The number of samples in playback buffer.
 */
	unsigned voice_p_status2();
/**
 * Get the voice channel record status register.
 *
 * Returns: The number of samples in capture buffer.
 */
	unsigned voice_r_status();
/**
 * Play sound on voice channel.
 *
 * Parameter samples: The samples to play.
 * Parameter count: Number of samples to play. Must be less than number of samples free for playback.
 */
	void play_voice(float* samples, size_t count);
/**
 * Capture sound on voice channel.
 *
 * Parameter samples: The buffer to store captured samples to.
 * Parameter count: Number of samples to capture. Must be less than number of samples used for capture.
 */
	void record_voice(float* samples, size_t count);
/**
 * Init the audio. Call on emulator startup.
 */
	void init();
/**
 * Quit the audio. Call on emulator shutdown.
 */
	void quit();
/**
 * Set music volume.
 *
 * Parameter volume: The volume (0-1).
 */
	void music_volume(float volume);
/**
 * Get music volume.
 *
 * Returns: The music volume.
 */
	float music_volume();
/**
 * Set voice playback volume.
 *
 * Parameter volume: The volume (0-1).
 */
	void voicep_volume(float volume);
/**
 * Get voice playback volume.
 *
 * Returns: The voice playback volume.
 */
	float voicep_volume();
/**
 * Set voice capture volume.
 *
 * Parameter volume: The volume (0-1).
 */
	void voicer_volume(float volume);
/**
 * Get voice capture volume.
 *
 * Returns: The voice capture volume.
 */
	float voicer_volume();

//The following are intended to be used by the driver from the callback
/**
 * Get mixed music + voice buffer to play (at voice rate).
 *
 * Parameter samples: Buffer to store the samples to.
 * Parameter count: Number of samples to generate.
 * Parameter stereo: If true, return stereo buffer, else mono.
 */
	void get_mixed(int16_t* samples, size_t count, bool stereo);
/**
 * Get music channel buffer to play.
 *
 * Parameter played: Number of samples to ACK as played.
 * Returns: Buffer to play.
 *
 * Note: This should only be called from the sound driver.
 */
	struct buffer get_music(size_t played);
/**
 * Get voice channel buffer to play.
 *
 * Parameter samples: The place to store the samples.
 * Parameter count: Number of samples to fill.
 *
 * Note: This should only be called from the sound driver.
 */
	void get_voice(float* samples, size_t count);
/**
 * Put recorded voice channel buffer.
 *
 * Parameter samples: The samples to send. Can be NULL to send silence.
 * Parameter count: Number of samples to send.
 *
 * Note: Even if audio driver does not support capture, one should send in silence.
 * Note: This should only be called from the sound driver.
 */
	void put_voice(float* samples, size_t count);
/**
 * Set the voice channel playback/record rate.
 *
 * Parameter rate_r: The recording rate in samples per second.
 * Parameter rate_p: The playback rate in samples per second.
 *
 * Note: This should only be called from the sound driver.
 * Note: Setting rate to 0 enables dummy callbacks.
 */
	void voice_rate(unsigned rate_r, unsigned rate_p);
/**
 * Suppress all future VU updates.
 */
	static void disable_vu_updates();
	//Vu values.
	vumeter vu_mleft;
	vumeter vu_mright;
	vumeter vu_vout;
	vumeter vu_vin;
private:
	struct dummy_cb_proc
	{
		dummy_cb_proc(audioapi_instance& _parent);
		int operator()();
		audioapi_instance& parent;
	};
	dummy_cb_proc dummyproc;
	threads::thread* dummythread;
	//3 music buffers is not enough due to huge blocksizes used by SDL.
	const static unsigned MUSIC_BUFFERS = 8;
	const static unsigned voicep_bufsize = 65536;
	const static unsigned voicer_bufsize = 65536;
	const static unsigned music_bufsize = 8192;
	float voicep_buffer[voicep_bufsize];
	float voicer_buffer[voicer_bufsize];
	int16_t music_buffer[MUSIC_BUFFERS * music_bufsize];
	volatile bool music_stereo[MUSIC_BUFFERS];
	volatile double music_rate[MUSIC_BUFFERS];
	volatile size_t music_size[MUSIC_BUFFERS];
	unsigned music_ptr;
	unsigned last_complete_music_seen;
	volatile unsigned last_complete_music;
	volatile unsigned voicep_get;
	volatile unsigned voicep_put;
	volatile unsigned voicer_get;
	volatile unsigned voicer_put;
	volatile unsigned voice_rate_play;
	volatile unsigned orig_voice_rate_play;
	volatile unsigned voice_rate_rec;
	volatile bool dummy_cb_active_record;
	volatile bool dummy_cb_active_play;
	volatile bool dummy_cb_quit;
	volatile float _music_volume;
	volatile float _voicep_volume;
	volatile float _voicer_volume;
	resampler music_resampler;
	bool last_adjust;	//Adjusting consequtively is too hard.
	static bool vu_disabled;
};


#endif
