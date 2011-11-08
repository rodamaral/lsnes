#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/misc.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/render.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"

#include <cstring>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <csignal>
#include <sstream>
#include <fstream>
#include <cassert>
#include <string>
#include <map>
#include <stdexcept>
#include <portaudio.h>
#include <boost/lexical_cast.hpp>

namespace
{
	uint32_t audio_playback_freq = 0;
	const size_t audiobuf_size = 8192;
	uint16_t audiobuf[audiobuf_size];
	volatile size_t audiobuf_get = 0;
	volatile size_t audiobuf_put = 0;
	uint64_t sampledup_ctr = 0;
	uint64_t sampledup_inc = 0;
	uint64_t sampledup_mod = 0;
	uint32_t use_rate_n = 32000;
	uint32_t use_rate_d = 1;
	PaDeviceIndex current_device = paNoDevice;
	bool stereo = false;
	PaStream* s = NULL;
	bool init_flag = false;
	bool was_enabled = false;

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
			if(!stereo)
				l = l / 2 + r / 2;
			*(_output++) = l - 32768;
			if(stereo)
				*(_output++) = r - 32768;
		}
		return 0;
	}

	bool switch_devices(PaDeviceIndex newdevice)
	{
		//Check that the new device index is valid at all.
		if((newdevice >= 0 && !Pa_GetDeviceInfo(newdevice)) || (newdevice < 0 && newdevice != paNoDevice)) {
			window::out() << "Invalid device " << newdevice << std::endl;
			return false;
		}
		if(newdevice != paNoDevice && Pa_GetDeviceInfo(newdevice)->maxOutputChannels < 1) {
			window::out() << "Portaudio: Device " << newdevice << " is not capable of playing sound."
				<< std::endl;
			return false;
		}
		//If audio is open somewhere, close it.
		if(current_device != paNoDevice) {
			PaError err;
			if(was_enabled) {
				err = Pa_StopStream(s);
				if(err != paNoError) {
					window::out() << "Portaudio error (stop): " << Pa_GetErrorText(err)
						<< std::endl;
					return false;
				}
			}
			err = Pa_CloseStream(s);
			if(err != paNoError) {
				window::out() << "Portaudio error (close): " << Pa_GetErrorText(err) << std::endl;
				return false;
			}
			current_device = paNoDevice;
			//Sound disabled.
			sampledup_ctr = 0;
			sampledup_inc = 0;
			sampledup_mod = 0;
		}
		//If new audio device is to be opened, try to do it.
		if(newdevice != paNoDevice) {
			const PaDeviceInfo* inf = Pa_GetDeviceInfo(newdevice);
			PaStreamParameters output;
			memset(&output, 0, sizeof(output));
			output.device = newdevice;
			output.channelCount = (inf->maxOutputChannels > 1) ? 2 : 1;
			output.sampleFormat = paInt16;
			output.suggestedLatency = inf->defaultLowOutputLatency;
			output.hostApiSpecificStreamInfo = NULL;
			PaError err = Pa_OpenStream(&s, NULL, &output, inf->defaultSampleRate, 0, 0, audiocb, NULL);
			if(err != paNoError) {
				window::out() << "Portaudio: error (open): " << Pa_GetErrorText(err) << std::endl
					<< "\tOn device: '" << inf->name << "'" << std::endl;
				return false;
			}

			stereo = (output.channelCount == 2);
			if(was_enabled) {
				err = Pa_StartStream(s);
				if(err != paNoError) {
					window::out() << "Portaudio error (start): " << Pa_GetErrorText(err)
						<< std::endl << "\tOn device: '" << inf->name << "'" << std::endl;
					return false;
				}
			}
			audio_playback_freq = inf->defaultSampleRate;
			calculate_sampledup(use_rate_n, use_rate_d);
			window::out() << "Portaudio: Opened " << inf->defaultSampleRate << "Hz "
				<< (stereo ? "Stereo" : "Mono") << " sound on '" << inf->name << "'" << std::endl;
			window::out() << "Switched to sound device '" << inf->name << "'" << std::endl;
		} else
			window::out() << "Switched to sound device NULL" << std::endl;
		current_device = newdevice;
		return true;
	}

}

void window::_sound_enable(bool enable) throw()
{
	PaError err;
	if(enable == was_enabled)
		return;
	if(enable)
		err = Pa_StartStream(s);
	else
		err = Pa_StopStream(s);
	if(err != paNoError)
		window::out() << "Portaudio error (start/stop): " << Pa_GetErrorText(err) << std::endl;
	else
		was_enabled = enable;
}

void sound_init()
{
	PaError err = Pa_Initialize();
	if(err != paNoError) {
		window::out() << "Portaudio error (init): " << Pa_GetErrorText(err) << std::endl;
		window::out() << "Audio can't be initialized, audio playback disabled" << std::endl;
		return;
	}
	init_flag = true;

	PaDeviceIndex forcedevice = paNoDevice;
	if(getenv("LSNES_FORCE_AUDIO_OUT")) {
		forcedevice = atoi(getenv("LSNES_FORCE_AUDIO_OUT"));
		window::out() << "Attempting to force sound output " << forcedevice << std::endl;
	}
	window::out() << "Detected " << Pa_GetDeviceCount() << " sound output devices." << std::endl;
	for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++)
		window::out() << "Audio device " << j << ": " << Pa_GetDeviceInfo(j)->name << std::endl;
	bool any_success = false;
	was_enabled = true;
	if(forcedevice == paNoDevice) {
		for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++) {
			any_success |= switch_devices(j);
			if(any_success)
				break;
		}
	} else
		any_success |= switch_devices(forcedevice);
	if(!any_success)
		window::out() << "Portaudio: Can't open any sound device, audio disabled" << std::endl;
}

void sound_quit()
{
	if(!init_flag)
		return;
	switch_devices(paNoDevice);
	Pa_Terminate();
	init_flag = false;
}

void window::play_audio_sample(uint16_t left, uint16_t right) throw()
{
	if(!init_flag)
		return;
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

class sound_change_listener : public information_dispatch
{
public:
	sound_change_listener() : information_dispatch("portaudio-sound-change-listener") {}
	void on_sound_rate(uint32_t rate_n, uint32_t rate_d)
	{
		if(!init_flag)
			return;
		uint32_t g = gcd(rate_n, rate_d);
		use_rate_n = rate_n / g;
		use_rate_d = rate_d / g;
		calculate_sampledup(use_rate_n, use_rate_d);
	}
} sndchgl;

bool window::sound_initialized()
{
	return init_flag;
}

void window::_set_sound_device(const std::string& dev)
{
	if(dev == "null") {
		if(!switch_devices(paNoDevice))
			throw std::runtime_error("Failed to switch sound outputs");
	} else {
		PaDeviceIndex idx;
		try {
			idx = boost::lexical_cast<PaDeviceIndex>(dev);
			if(idx < 0 || !Pa_GetDeviceInfo(idx))
				throw std::runtime_error("foo");
		} catch(std::exception& e) {
			throw std::runtime_error("Invalid device '" + dev + "'");
		}
		if(!switch_devices(idx))
			throw std::runtime_error("Failed to switch sound outputs");
	}
}

std::string window::get_current_sound_device()
{
	if(current_device == paNoDevice)
		return "null";
	else {
		std::ostringstream str;
		str << current_device;
		return str.str();
	}
}

std::map<std::string, std::string> window::get_sound_devices()
{
	std::map<std::string, std::string> ret;
	ret["null"] = "null sound output";
	for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++) {
		std::ostringstream str;
		str << j;
		ret[str.str()] = Pa_GetDeviceInfo(j)->name;
	}
	return ret;
}

const char* sound_plugin_name = "Portaudio sound plugin";
