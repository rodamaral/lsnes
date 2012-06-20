#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/framebuffer.hpp"

#include <vector>
#include <iostream>
#include <csignal>
#include <sstream>
#include <fstream>
#include <cassert>

#define WATCHDOG_TIMEOUT 15
#define MAXMESSAGES 6
#define MSGHISTORY 1000
#define MAXHISTORY 1000

#include <SDL.h>
#include <string>
#include <map>
#include <stdexcept>

namespace
{
	SDL_AudioSpec* desired;
	SDL_AudioSpec* obtained;
	uint32_t audio_playback_freq = 0;
	const size_t audiobuf_size = 8192;
	uint16_t audiobuf[audiobuf_size];
	volatile size_t audiobuf_get = 0;
	volatile size_t audiobuf_put = 0;
	uint64_t sampledup_ctr = 0;
	uint64_t sampledup_inc = 0;
	uint64_t sampledup_mod = 1;
	Uint16 format = AUDIO_S16SYS;
	bool stereo = true;
	bool sound_enabled = true;

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

	void audiocb(void* dummy, Uint8* stream, int len)
	{
		static uint16_t lprev = 32768;
		static uint16_t rprev = 32768;
		if(!sound_enabled)
			lprev = rprev = 32768;
		uint16_t bias = (format == AUDIO_S8 || format == AUDIO_S16LSB || format == AUDIO_S16MSB || format ==
			AUDIO_S16SYS) ? 32768 : 0;
		while(len > 0) {
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
			if(format == AUDIO_U8 || format == AUDIO_S8) {
				stream[0] = (l - bias) >> 8;
				if(stereo)
					stream[1] = (r - bias) >> 8;
				stream += (stereo ? 2 : 1);
				len -= (stereo ? 2 : 1);
			} else if(format == AUDIO_S16SYS || format == AUDIO_U16SYS) {
				reinterpret_cast<uint16_t*>(stream)[0] = (l - bias);
				if(stereo)
					reinterpret_cast<int16_t*>(stream)[1] = (r - bias);
				stream += (stereo ? 4 : 2);
				len -= (stereo ? 4 : 2);
			} else if(format == AUDIO_S16LSB || format == AUDIO_U16LSB) {
				stream[0] = (l - bias);
				stream[1] = (l - bias) >> 8;
				if(stereo) {
					stream[2] = (r - bias);
					stream[3] = (r - bias) >> 8;
				}
				stream += (stereo ? 4 : 2);
				len -= (stereo ? 4 : 2);
			} else if(format == AUDIO_S16MSB || format == AUDIO_U16MSB) {
				stream[1] = (l - bias);
				stream[0] = (l - bias) >> 8;
				if(stereo) {
					stream[3] = (r - bias);
					stream[2] = (r - bias) >> 8;
				}
				stream += (stereo ? 4 : 2);
				len -= (stereo ? 4 : 2);
			}
		}
	}
}

void sound_plugin::enable(bool enable) throw()
{
	sound_enabled = enable;
	SDL_PauseAudio(enable ? 0 : 1);
}

void sound_plugin::init() throw()
{
	desired = new SDL_AudioSpec();
	obtained = new SDL_AudioSpec();

	desired->freq = 44100;
	desired->format = AUDIO_S16SYS;
	desired->channels = 2;
	desired->samples = 8192;
	desired->callback = audiocb;
	desired->userdata = NULL;

	if(SDL_OpenAudio(desired, obtained) < 0) {
		platform::message("Audio can't be initialized, audio playback disabled");
		//Disable audio.
		audio_playback_freq = 0;
		auto g = information_dispatch::get_sound_rate();
		calculate_sampledup(g.first, g.second);
		return;
	}

	//Fill the parameters.
	audio_playback_freq = obtained->freq;
	auto g = information_dispatch::get_sound_rate();
	calculate_sampledup(g.first, g.second);
	format = obtained->format;
	stereo = (obtained->channels == 2);
	//GO!!!
	SDL_PauseAudio(0);
}

void sound_plugin::quit() throw()
{
	SDL_PauseAudio(1);
	SDL_Delay(100);
	delete desired;
	delete obtained;
}

void sound_plugin::sample(uint16_t left, uint16_t right) throw()
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

class sound_change_listener : public information_dispatch
{
public:
	sound_change_listener() : information_dispatch("sdl-sound-change-listener") {}
	void on_sound_rate(uint32_t rate_n, uint32_t rate_d)
	{
		uint32_t g = gcd(rate_n, rate_d);
		calculate_sampledup(rate_n / g, rate_d / g);
	}
} sndchgl;

bool sound_plugin::initialized()
{
	return (audio_playback_freq != 0);
}

void sound_plugin::set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error)
{
	if(dev != "default")
		throw std::runtime_error("Bad sound device '" + dev + "'");
}

std::string sound_plugin::get_device() throw(std::bad_alloc)
{
	return "default";
}

std::map<std::string, std::string> sound_plugin::get_devices() throw(std::bad_alloc)
{
	std::map<std::string, std::string> ret;
	ret["default"] = "default sound output";
	return ret;
}

const char* sound_plugin::name = "SDL sound plugin";
