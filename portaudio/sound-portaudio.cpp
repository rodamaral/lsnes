#include "window.hpp"
#include "render.hpp"
#include <cstring>
#include "command.hpp"
#include "framerate.hpp"
#include "misc.hpp"
#include "lsnes.hpp"
#include "settings.hpp"
#include <vector>
#include <iostream>
#include <csignal>
#include "keymapper.hpp"
#include "framerate.hpp"
#include <sstream>
#include <fstream>
#include <cassert>

#define WATCHDOG_TIMEOUT 15
#define MAXMESSAGES 6
#define MSGHISTORY 1000
#define MAXHISTORY 1000

#include <portaudio.h>
#include <string>
#include <map>
#include <stdexcept>

namespace
{
	uint32_t audio_playback_freq = 0;
	const size_t audiobuf_size = 8192;
	uint16_t audiobuf[audiobuf_size];
	volatile size_t audiobuf_get = 0;
	volatile size_t audiobuf_put = 0;
	uint64_t sampledup_ctr = 0;
	uint64_t sampledup_inc = 0;
	uint64_t sampledup_mod = 1;
	PaStream* s;

	void calculate_sampledup(uint32_t rate_n, uint32_t rate_d)
	{
		if(!audio_playback_freq) {
			//Sound disabled.
			sampledup_ctr = 0;
			sampledup_inc = 0;
			sampledup_mod = 0;
		} else {
			sampledup_ctr = 0;
			sampledup_inc = rate_n;
			sampledup_mod = rate_d * audio_playback_freq + rate_n;
		}
	}

	function_ptr_command<const std::string&> enable_sound("enable-sound", "Enable/Disable sound",
		"Syntax: enable-sound <on/off>\nEnable or disable sound.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			std::string s = args;
			if(s == "on" || s == "true" || s == "1" || s == "enable" || s == "enabled")
				window::sound_enable(true);
			else if(s == "off" || s == "false" || s == "0" || s == "disable" || s == "disabled")
				window::sound_enable(false);
			else
				throw std::runtime_error("Bad sound setting");
		});

	int audiocb(const void *input, void *output, unsigned long frame_count,
		const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void* user)
	{
		static uint16_t lprev, rprev;
		int16_t* _output = reinterpret_cast<int16_t*>(output);
		for(unsigned long i = 0; i < frame_count; i++) {
			uint16_t l, r;
			if(audiobuf_get == audiobuf_put) {
				l = lprev;
				r = rprev;
			} else {
				l = lprev = audiobuf[audiobuf_get++];
				r = rprev = audiobuf[audiobuf_get++];
				if(audiobuf_get == audiobuf_size)
					audiobuf_get = 0;
			}
			*(_output++) = l - 32768;
			*(_output++) = r - 32768;
		}
		return 0;
	}

	double attempts[] = {44100, 48000, 32000, -1};
}

void window::sound_enable(bool enable) throw()
{
	PaError err;
	if(enable)
		err = Pa_StartStream(s);
	else
		err = Pa_StopStream(s);
	if(err != paNoError)
		window::message(std::string("Portaudio error (start/stop): ") + Pa_GetErrorText(err));
}

void sound_init()
{
	PaError err = Pa_Initialize();
	if(err != paNoError) {
		window::message(std::string("Portaudio error (init): ") + Pa_GetErrorText(err));
		window::message("Audio can't be initialized, audio playback disabled");
		//Disable audio.
		audio_playback_freq = 0;
		calculate_sampledup(32000, 1);
		return;
	}

	PaDeviceIndex device = 0;
	PaDeviceIndex forcedevice = static_cast<PaDeviceIndex>(-1);
	if(getenv("LSNES_FORCE_AUDIO_OUT")) {
		forcedevice = atoi(getenv("LSNES_FORCE_AUDIO_OUT"));
		std::ostringstream str;
		str << "Attempting to force sound output #" << forcedevice;
		window::message(str.str());
		device = forcedevice;
	}
	{
		std::ostringstream str;
		str << "Detected " << Pa_GetDeviceCount() << " sound output devices.";
		window::message(str.str());
	}
	for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++) {
		const PaDeviceInfo* inf = Pa_GetDeviceInfo(j);
		window::message(std::string("Audio device: ") + inf->name);
	}

	unsigned i = 0;
	double freq = 0;
	while((freq = attempts[i]) > 0) {
		PaStreamParameters output;
		memset(&output, 0, sizeof(output));
		output.device = device;
		output.channelCount = 2;
		output.sampleFormat = paInt16;
		output.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowOutputLatency;
		output.hostApiSpecificStreamInfo = NULL;
		err = Pa_OpenStream(&s, NULL, &output, freq, 0, 0, audiocb, NULL);
		if(err == paNoError)
			break;
		{
			std::ostringstream str;
			str << "Portaudio: Can't open audio at " << freq << " using '"
				<< Pa_GetDeviceInfo(device)->name << "': " << Pa_GetErrorText(err) << std::endl;
			window::message(str.str());
		}

		i++;
		if(attempts[i] <= 0 && device < Pa_GetDeviceCount()) {
			i = 0;
			device++;
			if(forcedevice != static_cast<PaDeviceIndex>(-1))
				break;
		}
		if(device == Pa_GetDeviceCount())
			break;
	}

	//GO!!!
	if(freq <= 0) {
		window::message(std::string("Portaudio error (open): ") + Pa_GetErrorText(err));
		window::message("Audio can't be initialized, audio playback disabled");
		//Disable audio.
		audio_playback_freq = 0;
		calculate_sampledup(32000, 1);
		return;
	}
	{
		std::ostringstream str;
		str << "Portaudio: Opened audio at " << freq << " using '" << Pa_GetDeviceInfo(device)->name
			<< "'" << std::endl;
		window::message(str.str());
	}
	audio_playback_freq = freq;
	calculate_sampledup(32000, 1);
	err = Pa_StartStream(s);
	if(err != paNoError)
		window::message(std::string("Portaudio error (start): ") + Pa_GetErrorText(err));
}

void sound_quit()
{
	PaError err = Pa_StopStream(s);
	if(err != paNoError)
		window::message(std::string("Portaudio error (stop): ") + Pa_GetErrorText(err));
	Pa_Terminate();
}

void window::play_audio_sample(uint16_t left, uint16_t right) throw()
{
	sampledup_ctr += sampledup_inc;
	while(sampledup_ctr < sampledup_mod) {
		audiobuf[audiobuf_put++] = left;
		audiobuf[audiobuf_put++] = right;
		if(audiobuf_put == audiobuf_size)
			audiobuf_put = 0;
		sampledup_ctr += sampledup_inc;
	}
	sampledup_ctr -= sampledup_mod;
}

void window::set_sound_rate(uint32_t rate_n, uint32_t rate_d)
{
	uint32_t g = gcd(rate_n, rate_d);
	calculate_sampledup(rate_n / g, rate_d / g);
}

const char* sound_plugin_name = "Portaudio sound plugin";
