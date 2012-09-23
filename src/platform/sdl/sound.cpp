#include "lsnes.hpp"

#include "core/audioapi.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/framebuffer.hpp"
#include "library/minmax.hpp"

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
	volatile double sample_rate = 32000;
	Uint16 format = AUDIO_S16SYS;
	bool stereo = true;
	bool sound_enabled = true;


	void audiocb(void* dummy, Uint8* stream, int len)
	{
		const unsigned voice_blocksize = 128;
		int16_t voicebuf[voice_blocksize];
		if(format == AUDIO_S8 || format == AUDIO_U8)
			len /= (stereo ? 2 : 1);
		else
			len /= (stereo ? 4 : 2);
		
		size_t ptr = 0;
		while(len > 0) {
			unsigned bsize = min(voice_blocksize / (stereo ? 2 : 1), static_cast<unsigned>(len));
			audioapi_get_mixed(voicebuf, bsize, stereo);
			if(format == AUDIO_S16LSB) {
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++) {
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]);
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]) >> 8;
				}
			} else if(format == AUDIO_U16LSB) {
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++) {
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]);
					stream[ptr++] = (static_cast<uint16_t>(voicebuf[i]) + 32768) >> 8;
				}
			} else if(format == AUDIO_S16MSB) {
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++) {
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]) >> 8;
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]);
				}
			} else if(format == AUDIO_U16MSB) {
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++) {
					stream[ptr++] = (static_cast<uint16_t>(voicebuf[i]) + 32768) >> 8;
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]);
				}
			} else if(format == AUDIO_S16SYS) {
				int16_t* _stream = reinterpret_cast<int16_t*>(stream);
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
					_stream[ptr++] = voicebuf[i];
			} else if(format == AUDIO_U16SYS) {
				uint16_t* _stream = reinterpret_cast<uint16_t*>(stream);
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
					_stream[ptr++] = voicebuf[i] + 32768;
			} else if(format == AUDIO_S8) {
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
					stream[ptr++] = static_cast<uint16_t>(voicebuf[i]) >> 8;
			} else if(format == AUDIO_U8) {
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
					stream[ptr++] = (static_cast<uint16_t>(voicebuf[i]) + 32768) >> 8;
			}
			audioapi_put_voice(NULL, bsize);
			len -= bsize;
		}
	}
}

void audioapi_driver_enable(bool enable) throw()
{
	sound_enabled = enable;
	SDL_PauseAudio(enable ? 0 : 1);
	audioapi_set_dummy_cb(!enable);
	if(enable)
		audioapi_voice_rate(audio_playback_freq);
}

void audioapi_driver_init() throw()
{
	desired = new SDL_AudioSpec();
	obtained = new SDL_AudioSpec();

	desired->freq = 44100;
	desired->format = AUDIO_S16SYS;
	desired->channels = 2;
	desired->samples = 2048;
	desired->callback = audiocb;
	desired->userdata = NULL;

	auto g = information_dispatch::get_sound_rate();
	sample_rate = g.first * 1.0 / g.second;

	if(SDL_OpenAudio(desired, obtained) < 0) {
		platform::message("Audio can't be initialized, audio playback disabled");
		//Disable audio.
		audio_playback_freq = 0;
		audioapi_set_dummy_cb(true);
		return;
	}

	//Fill the parameters.
	audio_playback_freq = obtained->freq;
	audioapi_voice_rate(audio_playback_freq);
	format = obtained->format;
	stereo = (obtained->channels == 2);
	//GO!!!
	SDL_PauseAudio(0);
}

void audioapi_driver_quit() throw()
{
	SDL_PauseAudio(1);
	SDL_Delay(100);
	delete desired;
	delete obtained;
}

class sound_change_listener : public information_dispatch
{
public:
	sound_change_listener() : information_dispatch("sdl-sound-change-listener") {}
	void on_sound_rate(uint32_t rate_n, uint32_t rate_d)
	{
	}
} sndchgl;

bool audioapi_driver_initialized()
{
	return (audio_playback_freq != 0);
}

void audioapi_driver_set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error)
{
	if(dev != "default")
		throw std::runtime_error("Bad sound device '" + dev + "'");
}

std::string audioapi_driver_get_device() throw(std::bad_alloc)
{
	return "default";
}

std::map<std::string, std::string> audioapi_driver_get_devices() throw(std::bad_alloc)
{
	std::map<std::string, std::string> ret;
	ret["default"] = "default sound output";
	return ret;
}

const char* audioapi_driver_name = "SDL sound plugin";
