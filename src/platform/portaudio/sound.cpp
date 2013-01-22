#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/misc.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/framebuffer.hpp"

#include <cstring>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <csignal>
#include <sstream>
#include <fstream>
#include <cassert>
#include "core/audioapi.hpp"
#include "library/minmax.hpp"
#include <string>
#include <map>
#include <stdexcept>
#include <portaudio.h>
#include <boost/lexical_cast.hpp>

namespace
{
	uint32_t audio_playback_freq = 0;
	PaDeviceIndex current_device = paNoDevice;
	bool stereo = false;
	bool istereo = false;
	PaStream* s = NULL;
	bool init_flag = false;
	bool was_enabled = false;
	double ltnow = 0;
	double ltadc = 0;
	double ltdac = 0;

	uint64_t first_ts;
	uint64_t frames;

	int audiocb(const void *input, void *output, unsigned long frame_count,
		const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void* user)
	{
		if(!first_ts)
			first_ts = get_utime();
		frames += frame_count;
//		std::cerr << "Frames requested: " << frame_count << std::endl;
//		std::cerr << "dtnow=" << (time_info->currentTime - ltnow)
//			<< " dtdac=" << (time_info->outputBufferDacTime - ltdac)
//			<< " dtadc=" << (time_info->inputBufferAdcTime - ltadc) << std::endl;
		ltnow = time_info->currentTime;
		ltadc = time_info->inputBufferAdcTime;
		ltdac = time_info->outputBufferDacTime;
		int16_t* _output = reinterpret_cast<int16_t*>(output);
		const int16_t* _input = reinterpret_cast<const int16_t*>(input);
		const unsigned voice_blocksize = 256;
		int16_t voicebuf[voice_blocksize];
		float voicebuf2[voice_blocksize];
		size_t ptr = 0;
		size_t iptr = 0;
		while(frame_count > 0) {
			unsigned bsize = min(voice_blocksize / 2, static_cast<unsigned>(frame_count));
			audioapi_get_mixed(voicebuf, bsize, stereo);
			if(was_enabled)
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
					_output[ptr++] = voicebuf[i];
			else
				for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
					_output[ptr++] = 0;
			if(!input)
				audioapi_put_voice(NULL, bsize);
			else {
				if(istereo)
					for(size_t i = 0; i < bsize; i++) {
						float l = _input[iptr++];
						float r = _input[iptr++];
						voicebuf2[i] = (l + r) / 2;
					}
				else
					for(size_t i = 0; i < bsize; i++)
						voicebuf2[i] = _input[iptr++];
				audioapi_put_voice(voicebuf2, bsize);
			}
			frame_count -= bsize;
		}
		return 0;
	}

	bool switch_devices(PaDeviceIndex newdevice)
	{
		//Check that the new device index is valid at all.
		if((newdevice >= 0 && !Pa_GetDeviceInfo(newdevice)) || (newdevice < 0 && newdevice != paNoDevice)) {
			messages << "Invalid device " << newdevice << std::endl;
			return false;
		}
		if(newdevice != paNoDevice && Pa_GetDeviceInfo(newdevice)->maxOutputChannels < 1) {
			messages << "Portaudio: Device " << newdevice << " is not capable of playing sound."
				<< std::endl;
			return false;
		}
		//If audio is open somewhere, close it.
		if(current_device != paNoDevice) {
			PaError err;
			err = Pa_StopStream(s);
			if(err != paNoError) {
				messages << "Portaudio error (stop): " << Pa_GetErrorText(err)
					<< std::endl;
				return false;
			}
			err = Pa_CloseStream(s);
			if(err != paNoError) {
				messages << "Portaudio error (close): " << Pa_GetErrorText(err) << std::endl;
				return false;
			}
			current_device = paNoDevice;
			//Sound disabled.
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

			//Blacklist these devices for recording, portaudio is buggy with recording off
			//these things.
			bool buggy = (!strcmp(inf->name, "default") || !strcmp(inf->name, "sysdefault"));

			PaStreamParameters input;
			memset(&input, 0, sizeof(input));
			input.device = newdevice;
			input.channelCount = (inf->maxInputChannels > 1) ? 2 : 1;
			input.sampleFormat = paInt16;
			input.suggestedLatency = inf->defaultLowInputLatency;
			input.hostApiSpecificStreamInfo = NULL;
			PaStreamParameters* _input = (!buggy && inf->maxInputChannels) ? &input : NULL;
			if(!_input)
				if(!buggy)
					messages << "Portaudio: Warning: Audio capture not available on this device"
						<< std::endl;
				else
					messages << "Portaudio: Warning: This device is blacklisted for capture"
						<< std::endl;
			else
				messages << "Portaudio: Notice: Audio capture available" << std::endl;
	
			PaError err = Pa_OpenStream(&s, _input, &output, inf->defaultSampleRate, 0, 0, audiocb, NULL);
			if(err != paNoError) {
				messages << "Portaudio: error (open): " << Pa_GetErrorText(err) << std::endl
					<< "\tOn device: '" << inf->name << "'" << std::endl;
				audioapi_set_dummy_cb(true);
				return false;
			}

			frames = 0;
			first_ts = 0;
			stereo = (output.channelCount == 2);
			istereo = (input.channelCount == 2);
			err = Pa_StartStream(s);
			if(err != paNoError) {
				messages << "Portaudio error (start): " << Pa_GetErrorText(err)
					<< std::endl << "\tOn device: '" << inf->name << "'" << std::endl;
				audioapi_set_dummy_cb(true);
				return false;
			}
			const PaStreamInfo* si = Pa_GetStreamInfo(s);
			audio_playback_freq = si ? si->sampleRate : inf->defaultSampleRate;
			audioapi_set_dummy_cb(false);
			audioapi_voice_rate(audio_playback_freq);
			messages << "Portaudio: Opened " << audio_playback_freq << "Hz "
				<< (stereo ? "Stereo" : "Mono") << " sound on '" << inf->name << "'" << std::endl;
			messages << "Switched to sound device '" << inf->name << "'" << std::endl;
		} else {
			messages << "Switched to sound device NULL" << std::endl;
			audioapi_set_dummy_cb(true);
		}
		current_device = newdevice;
		return true;
	}

	function_ptr_command<const std::string&> x(lsnes_cmd, "portaudio", "", "",
		[](const std::string& value) throw(std::bad_alloc, std::runtime_error) {
			messages << "Load: " << Pa_GetStreamCpuLoad(s) << std::endl;
			messages << "Rate: " << 1000000.0 * frames / (get_utime() - first_ts) << std::endl;
		});

	class sound_change_listener : public information_dispatch
	{
	public:
		sound_change_listener() : information_dispatch("portaudio-sound-change-listener") {}
		void on_sound_rate(uint32_t rate_n, uint32_t rate_d)
		{
		}
	} sndchgl;

	struct _audioapi_driver drv = {
		.init = []() -> void {
			PaError err = Pa_Initialize();
			if(err != paNoError) {
				messages << "Portaudio error (init): " << Pa_GetErrorText(err) << std::endl;
				messages << "Audio can't be initialized, audio playback disabled" << std::endl;
				return;
			}
			init_flag = true;

			PaDeviceIndex forcedevice = paNoDevice;
			if(getenv("LSNES_FORCE_AUDIO_OUT")) {
				forcedevice = atoi(getenv("LSNES_FORCE_AUDIO_OUT"));
				messages << "Attempting to force sound output " << forcedevice << std::endl;
			}
			messages << "Detected " << Pa_GetDeviceCount() << " sound output devices." << std::endl;
			for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++)
				messages << "Audio device " << j << ": " << Pa_GetDeviceInfo(j)->name << std::endl;
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
			if(!any_success) {
				messages << "Portaudio: Can't open any sound device, audio disabled" << std::endl;
				audioapi_set_dummy_cb(true);
			}
		},
		.quit = []() -> void {
			if(!init_flag)
				return;
			switch_devices(paNoDevice);
			Pa_Terminate();
			init_flag = false;
		},
		.enable = [](bool _enable) -> void { was_enabled = _enable; },
		.initialized = []() -> bool { return init_flag; },
		.set_device = [](const std::string& dev) -> void {
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
		},
		.get_device = []() -> std::string {
			if(current_device == paNoDevice)
				return "null";
			else {
				std::ostringstream str;
				str << current_device;
				return str.str();
			}
		},
		.get_devices = []() -> std::map<std::string, std::string> {
			std::map<std::string, std::string> ret;
			ret["null"] = "null sound output";
			for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++) {
				std::ostringstream str;
				str << j;
				ret[str.str()] = Pa_GetDeviceInfo(j)->name;
			}
			return ret;
		},
		.name = []() -> const char* { return "Portaudio sound plugin"; }
	};
	struct audioapi_driver _drv(drv);
}
