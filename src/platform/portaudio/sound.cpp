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
	uint32_t audio_record_freq = 0;
	PaDeviceIndex current_device_rec = paNoDevice;
	PaDeviceIndex current_device_play = paNoDevice;
	bool stereo = false;
	bool istereo = false;
	PaStream* st = NULL;
	bool init_flag = false;
	bool was_enabled = false;

	uint64_t first_ts;
	uint64_t frames;

	int audiocb(const void *input, void *output, unsigned long frame_count,
		const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void* user)
	{
		if(!first_ts)
			first_ts = get_utime();
		frames += frame_count;
		int16_t* _output = reinterpret_cast<int16_t*>(output);
		const int16_t* _input = reinterpret_cast<const int16_t*>(input);
		const unsigned voice_blocksize = 256;
		int16_t voicebuf[voice_blocksize];
		float voicebuf2[voice_blocksize];
		size_t ptr = 0;
		size_t iptr = 0;
		while(frame_count > 0) {
			unsigned bsize = min(voice_blocksize / 2, static_cast<unsigned>(frame_count));
			if(output) {
				audioapi_get_mixed(voicebuf, bsize, stereo);
				if(was_enabled)
					for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
						_output[ptr++] = voicebuf[i];
				else
					for(size_t i = 0; i < bsize * (stereo ? 2 : 1); i++)
						_output[ptr++] = 0;
			}
			if(input) {
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

	PaStreamParameters* get_output_parameters(PaDeviceIndex idx_in, PaDeviceIndex idx_out)
	{
		static PaStreamParameters output;
		if(idx_out == paNoDevice)
			return NULL;
		const PaDeviceInfo* inf = Pa_GetDeviceInfo(idx_out);
		if(!inf || !inf->maxOutputChannels)
			return NULL;
		memset(&output, 0, sizeof(output));
		output.device = idx_out;
		output.channelCount = (inf->maxOutputChannels > 1) ? 2 : 1;
		output.sampleFormat = paInt16;
		output.suggestedLatency = inf->defaultLowOutputLatency;
		output.hostApiSpecificStreamInfo = NULL;
		return &output;
	}

	PaStreamParameters* get_input_parameters(PaDeviceIndex idx_in, PaDeviceIndex idx_out)
	{
		static PaStreamParameters input;
		if(idx_in == paNoDevice)
			return NULL;
		const PaDeviceInfo* inf = Pa_GetDeviceInfo(idx_in);
		if(!inf || !inf->maxInputChannels)
			return NULL;
		const PaHostApiInfo* host = Pa_GetHostApiInfo(inf->hostApi);
		if(idx_in == idx_out && host && host->type == paALSA && (!strcmp(inf->name, "default") ||
			!strcmp(inf->name, "sysdefault")))
			//These things are blacklisted for full-duplex because Portaudio is buggy with these.
			return NULL;
		memset(&input, 0, sizeof(input));
		input.device = idx_in;
		input.channelCount = (inf->maxInputChannels > 1) ? 2 : 1;
		input.sampleFormat = paInt16;
		input.suggestedLatency = inf->defaultLowInputLatency;
		input.hostApiSpecificStreamInfo = NULL;
		return &input;
	}
	
	bool dispose_stream(PaStream* s)
	{
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
		return true;
	}
	
	bool close_devices_for_change()
	{
		bool status = true;
		if(st)
			status &= dispose_stream(st);
		st = NULL;
		current_device_rec = paNoDevice;
		current_device_play = paNoDevice;
		audio_playback_freq = 0;
		audio_record_freq = 0;
		audioapi_voice_rate(0, 0);
		return status;
	}

	unsigned switch_devices(PaDeviceIndex new_in, PaDeviceIndex new_out)
	{
		const PaStreamInfo* si;
		if(new_in == current_device_rec && new_out == current_device_play)
			return ((current_device_rec != paNoDevice) ? 1 : 0) | 
				((current_device_play != paNoDevice) ? 2 : 0);
		close_devices_for_change();
		if(new_in == paNoDevice && new_out == paNoDevice) {
			messages << "Sound devices closed." << std::endl;
			return 0;
		}
		const PaDeviceInfo* inf = Pa_GetDeviceInfo(new_out);
		if(!inf)
			inf = Pa_GetDeviceInfo(new_in);
		if(!inf)
			return 0;
		PaStreamParameters* input = get_input_parameters(new_in, new_out);
		PaStreamParameters* output = get_output_parameters(new_in, new_out);
		PaError err = Pa_OpenStream(&st, input, output, inf->defaultSampleRate, 0, 0, audiocb, NULL);
		if(err != paNoError) {
			messages << "Portaudio: error (open): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(0, 0);
			goto end_routine;
		}

		frames = 0;
		first_ts = 0;
		stereo = (output && output->channelCount == 2);
		istereo = (input && input->channelCount == 2);
		err = Pa_StartStream(st);
		if(err != paNoError) {
			messages << "Portaudio error (start): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(0, 0);
			goto end_routine;
		}
		si = Pa_GetStreamInfo(st);
		audio_playback_freq = output ? si->sampleRate : 0;
		audio_record_freq = input ? si->sampleRate : 0;
		audioapi_voice_rate(audio_record_freq, audio_playback_freq);
	end_routine:
		if(audio_record_freq)
			messages << "Portaudio: Input: " << audio_record_freq << "Hz "
				<< (istereo ? "Stereo" : "Mono") << " on '" << Pa_GetDeviceInfo(new_in)->name
				<< std::endl;
		else
			messages << "Portaudio: No sound input" << std::endl;
		if(audio_playback_freq)
			messages << "Portaudio: Output: " << audio_playback_freq << "Hz "
				<< (istereo ? "Stereo" : "Mono") << " on '" << Pa_GetDeviceInfo(new_out)->name
				<< std::endl;
		else
			messages << "Portaudio: No sound output" << std::endl;
		current_device_play = audio_playback_freq ? new_out : paNoDevice;
		current_device_rec = audio_record_freq ? new_in : paNoDevice;
		return ((current_device_rec != paNoDevice) ? 1 : 0) | 
			((current_device_play != paNoDevice) ? 2 : 0);
	}

	bool initial_switch_devices(PaDeviceIndex force_in, PaDeviceIndex force_out)
	{
		if(force_in != paNoDevice && force_out != paNoDevice) {
			//Both are being forced. Just try to open.
			unsigned r = switch_devices(force_in, force_out);
			return (r != 0);
		} else if(force_in != paNoDevice) {
			//Input forcing, but no output. Prefer full dupex.
			unsigned r = switch_devices(force_in, force_in);
			PaDeviceIndex out;
			bool tmp;
			switch(r) {
			case 0:
			case 1:
				tmp = (r != 0);
				//We didn't get output open on that. Scan for output device.
				for(out = 0; out < Pa_GetDeviceCount(); out++) {
					if(out == force_in)
						continue;	//Don't retry that.
					r = switch_devices(tmp ? force_in : paNoDevice, out);
					if(r == 3)
						return true;	//Got it open.
				}
				//We can't get output.
				return (switch_devices(force_in, paNoDevice) != 0);
			case 2:
			case 3:
				//We got output open. This is enough success.
				return true;
			}
		} else if(force_out != paNoDevice) {
			//Output forcing, but no input. Prefer full dupex.
			unsigned r = switch_devices(force_out, force_out);
			PaDeviceIndex in;
			bool tmp;
			switch(r) {
			case 0:
			case 2:
				tmp = (r != 0);
				//We didn't get input open on that. Scan for input device.
				for(in = 0; in < Pa_GetDeviceCount(); in++) {
					if(in == force_out)
						continue;	//Don't retry that.
					r = switch_devices(in, tmp ? force_out : paNoDevice);
					if(r == 3)
						return true;	//Got it open.
				}
				//We can't get input.
				return (switch_devices(paNoDevice, force_out) != 0);
			case 1:
			case 3:
				//We got input open. This is enough success.
				return true;
			}
		} else {
			//Neither is forced.
			unsigned r;
			PaDeviceIndex idx;
			PaDeviceIndex sidx;
			PaDeviceIndex pidx;
			PaDeviceIndex ridx;
			bool o_r = true;
			bool o_p = true;
			for(idx = 0; idx < Pa_GetDeviceCount(); idx++) {
				r = switch_devices(o_r ? idx : ridx, o_p ? idx : pidx);
				if(r & 1) {
					o_r = false;
					ridx = idx;
				}
				if(r & 2) {
					o_p = false;
					pidx = idx;
				}
				if(r == 3)
					return true;
			}
			//Get output open if possible.
			if(!o_p)
				return (switch_devices(paNoDevice, pidx) != 0);
			return false;
		}
	}
	
	struct _audioapi_driver drv = {
		.init = []() -> void {
			PaError err = Pa_Initialize();
			if(err != paNoError) {
				messages << "Portaudio error (init): " << Pa_GetErrorText(err) << std::endl;
				messages << "Audio can't be initialized, audio playback disabled" << std::endl;
				return;
			}
			init_flag = true;

			PaDeviceIndex forcedevice_in = paNoDevice;
			PaDeviceIndex forcedevice_out = paNoDevice;
			if(getenv("LSNES_FORCE_AUDIO_OUT")) {
				forcedevice_out = atoi(getenv("LSNES_FORCE_AUDIO_OUT"));
				messages << "Attempting to force sound output " << forcedevice_out << std::endl;
			}
			if(getenv("LSNES_FORCE_AUDIO_IN")) {
				forcedevice_in = atoi(getenv("LSNES_FORCE_AUDIO_IN"));
				messages << "Attempting to force sound input " << forcedevice_in << std::endl;
			}
			messages << "Detected " << Pa_GetDeviceCount() << " sound devices." << std::endl;
			for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++)
				messages << "Audio device " << j << ": " << Pa_GetDeviceInfo(j)->name << std::endl;
			was_enabled = true;
			bool any_success = initial_switch_devices(forcedevice_in, forcedevice_out);
			if(!any_success)
				messages << "Portaudio: Can't open any sound device, audio disabled" << std::endl;
		},
		.quit = []() -> void {
			if(!init_flag)
				return;
			if(st) {
				Pa_StopStream(st);
				Pa_CloseStream(st);
			}
			Pa_Terminate();
			init_flag = false;
		},
		.enable = [](bool _enable) -> void { was_enabled = _enable; },
		.initialized = []() -> bool { return init_flag; },
		.set_device = [](const std::string& dev, bool rec) -> void {
			bool failed = false;
			PaDeviceIndex idx;
			if(dev == "null") {
				idx = paNoDevice;
			} else {
				try {
					idx = boost::lexical_cast<PaDeviceIndex>(dev);
					if(idx < 0 || !Pa_GetDeviceInfo(idx))
						throw std::runtime_error("foo");
				} catch(std::exception& e) {
					throw std::runtime_error("Invalid device '" + dev + "'");
				}
			}
			if(rec) 
				failed = ((switch_devices(idx, current_device_play) & 2) == 0);
			else
				failed = ((switch_devices(current_device_rec, idx) & 1) == 0);
		},
		.get_device = [](bool rec) -> std::string {
			if(rec)
				if(current_device_rec == paNoDevice)
					return "null";
				else
					return (stringfmt() << current_device_rec).str();
			else
				if(current_device_play == paNoDevice)
					return "null";
				else
					return (stringfmt() << current_device_play).str();
		},
		.get_devices = [](bool rec) -> std::map<std::string, std::string> {
			std::map<std::string, std::string> ret;
			ret["null"] = rec ? "null sound input" : "null sound output";
			for(PaDeviceIndex j = 0; j < Pa_GetDeviceCount(); j++) {
				std::ostringstream str;
				str << j;
				auto devinfo = Pa_GetDeviceInfo(j);
				if(rec && devinfo->maxInputChannels)
					ret[str.str()] = devinfo->name;
				if(!rec && devinfo->maxOutputChannels)
					ret[str.str()] = devinfo->name;
			}
			return ret;
		},
		.name = []() -> const char* { return "Portaudio sound plugin"; }
	};
	struct audioapi_driver _drv(drv);
}
