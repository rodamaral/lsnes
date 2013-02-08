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
	//Input stream info.
	uint32_t current_rfreq = 0;
	PaDeviceIndex current_rdev = paNoDevice;
	bool flag_rstereo = false;
	PaStream* stream_r = NULL;
	//Output stream info.
	uint32_t current_pfreq = 0;
	PaDeviceIndex current_pdev = paNoDevice;
	bool flag_pstereo = false;
	PaStream* stream_p = NULL;
	//Some global info.
	bool init_flag = false;
	bool was_enabled = false;

	int audiocb(const void *input, void *output, unsigned long frame_count,
		const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void* user)
	{
		const unsigned voice_blocksize = 256;
		if(output && current_pfreq) {
			int16_t voicebuf[voice_blocksize];
			unsigned long pframe_count = frame_count;
			int16_t* _output = reinterpret_cast<int16_t*>(output);
			size_t ptr = 0;
			while(pframe_count > 0) {
				unsigned bsize = min(voice_blocksize / 2, static_cast<unsigned>(pframe_count));
				audioapi_get_mixed(voicebuf, bsize, flag_pstereo);
				unsigned limit = bsize * (flag_pstereo ? 2 : 1);
				if(was_enabled)
					for(size_t i = 0; i < limit; i++)
						_output[ptr++] = voicebuf[i];
				else
					for(size_t i = 0; i < limit; i++)
						_output[ptr++] = 0;
				pframe_count -= bsize;
			}
		}
		if(input && current_rfreq) {
			const int16_t* _input = reinterpret_cast<const int16_t*>(input);
			unsigned long rframe_count = frame_count;
			float voicebuf2[voice_blocksize];
			size_t iptr = 0;
			while(rframe_count > 0) {
				unsigned bsize = min(voice_blocksize / 2, static_cast<unsigned>(rframe_count));
				if(flag_rstereo)
					for(size_t i = 0; i < bsize; i++) {
						float l = _input[iptr++];
						float r = _input[iptr++];
						voicebuf2[i] = (l + r) / 2;
					}
				else
					for(size_t i = 0; i < bsize; i++)
						voicebuf2[i] = _input[iptr++];
				audioapi_put_voice(voicebuf2, bsize);
				rframe_count -= bsize;
			}
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
		if(!s)
			return true;
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

	bool check_indev(PaDeviceIndex dev)
	{
		const PaDeviceInfo* inf = NULL;
		if(dev != paNoDevice)
			inf = Pa_GetDeviceInfo(dev);
		if(dev != paNoDevice && (!inf || !inf->maxInputChannels)) {
			messages << "Portaudio: Trying to switch to invalid input device" << std::endl;
			return false;
		}
		return true;
	}

	bool check_outdev(PaDeviceIndex dev)
	{
		const PaDeviceInfo* inf = NULL;
		if(dev != paNoDevice)
			inf = Pa_GetDeviceInfo(dev);
		if(dev != paNoDevice && (!inf || !inf->maxOutputChannels)) {
			messages << "Portaudio: Trying to switch to invalid output device" << std::endl;
			return false;
		}
		return true;
	}

	void close_output()
	{
		dispose_stream(stream_p);
		stream_p = NULL;
		current_pdev = paNoDevice;
		current_pfreq = 0;
		audioapi_voice_rate(current_rfreq, current_pfreq);
	}

	void close_input()
	{
		dispose_stream(stream_r);
		stream_r = NULL;
		current_rdev = paNoDevice;
		current_rfreq = 0;
		audioapi_voice_rate(current_rfreq, current_pfreq);
	}

	void close_full()
	{
		dispose_stream(stream_p);
		stream_r = NULL;
		current_rdev = paNoDevice;
		current_rfreq = 0;
		stream_r = NULL;
		current_rdev = paNoDevice;
		current_rfreq = 0;
		audioapi_voice_rate(current_rfreq, current_pfreq);
	}
	
	void close_all()
	{
		if(current_rdev == current_pdev && current_rdev != paNoDevice)
			close_full();
		else {
			close_input();
			close_output();
		}
	}

	void print_status(unsigned flags)
	{
		if(flags & 2) {
			if(current_pfreq)
				messages << "Portaudio: Output: " << current_pfreq << "Hz "
					<< (flag_pstereo ? "Stereo" : "Mono") << " on '"
					<< Pa_GetDeviceInfo(current_pdev)->name << std::endl;
				else
				messages << "Portaudio: No sound output" << std::endl;
		}
		if(flags & 1) {
			if(current_rfreq)
				messages << "Portaudio: Input: " << current_rfreq << "Hz "
					<< (flag_rstereo ? "Stereo" : "Mono") << " on '"
					<< Pa_GetDeviceInfo(current_rdev)->name << std::endl;
				else
				messages << "Portaudio: No sound input" << std::endl;
		}
	}
	
	//Switch output device only in split-duplex configuration.
	unsigned switch_devices_outonly(PaDeviceIndex dev)
	{
		if(!check_outdev(dev))
			return 1;
		dispose_stream(stream_p);
		close_output();
		if(dev == paNoDevice) {
			messages << "Sound output closed." << std::endl;
			return 3;
		}
		PaStreamParameters* output = get_output_parameters(current_rdev, dev);
		auto inf = Pa_GetDeviceInfo(dev);
		PaError err = Pa_OpenStream(&stream_p, NULL, output, inf->defaultSampleRate, 0, 0, audiocb, NULL);
		if(err != paNoError) {
			messages << "Portaudio: error (open): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(current_rfreq, current_pfreq);
			return 1;
		}
		flag_pstereo = (output && output->channelCount == 2);
		err = Pa_StartStream(stream_p);
		if(err != paNoError) {
			messages << "Portaudio error (start): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(current_rfreq, current_pfreq);
			return 1;
		}
		const PaStreamInfo* si = Pa_GetStreamInfo(stream_p);
		current_pfreq = output ? si->sampleRate : 0;
		current_pdev = dev;
		audioapi_voice_rate(current_rfreq, current_pfreq);
		print_status(2);
		return 3;
	}

	//Switch input device only in split-duplex configuration.
	unsigned switch_devices_inonly(PaDeviceIndex dev)
	{
		if(!check_indev(dev))
			return 2;
		close_input();
		if(dev == paNoDevice) {
			messages << "Sound input closed." << std::endl;
			return 3;
		}
		PaStreamParameters* input = get_input_parameters(dev, current_pdev);
		auto inf = Pa_GetDeviceInfo(dev);
		PaError err = Pa_OpenStream(&stream_r, input, NULL, inf->defaultSampleRate, 0, 0, audiocb, NULL);
		if(err != paNoError) {
			messages << "Portaudio: error (open): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(current_rfreq, current_pfreq);
			return 2;
		}
		flag_rstereo = (input && input->channelCount == 2);
		err = Pa_StartStream(stream_r);
		if(err != paNoError) {
			messages << "Portaudio error (start): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(current_rfreq, current_pfreq);
			return 2;
		}
		const PaStreamInfo* si = Pa_GetStreamInfo(stream_r);
		current_rfreq = input ? si->sampleRate : 0;
		current_rdev = dev;
		audioapi_voice_rate(current_rfreq, current_pfreq);
		print_status(1);
		return 3;
	}

	//Switch to split-duplex configuration.
	unsigned switch_devices_split(PaDeviceIndex rdev, PaDeviceIndex pdev)
	{
		if(rdev == pdev)
			std::cerr << "switch_devices_split: rdev==pdev!" << std::endl;
		unsigned status = 3;
		if(!check_indev(rdev) || !check_outdev(pdev))
			return 0;
		close_all();
		status &= switch_devices_outonly(pdev);
		status &= switch_devices_inonly(rdev);
		return status;
	}

	//Switch to full-duplex configuration.
	unsigned switch_devices_full(PaDeviceIndex dev)
	{
		unsigned status = 0;
		if(!check_indev(dev) || !check_outdev(dev))
			return 0;
		close_all();
		if(dev == paNoDevice) {
			messages << "Sound devices closed." << std::endl;
			return 3;
		}
		PaStreamParameters* input = get_input_parameters(dev, dev);
		PaStreamParameters* output = get_input_parameters(dev, dev);
		auto inf = Pa_GetDeviceInfo(dev);
		PaError err = Pa_OpenStream(&stream_r, input, output, inf->defaultSampleRate, 0, 0, audiocb, NULL);
		if(err != paNoError) {
			messages << "Portaudio: error (open): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(current_rfreq, current_pfreq);
			return 0;
		}
		flag_rstereo = (input && input->channelCount == 2);
		flag_pstereo = (output && output->channelCount == 2);
		err = Pa_StartStream(stream_r);
		if(err != paNoError) {
			messages << "Portaudio error (start): " << Pa_GetErrorText(err) << std::endl;
			audioapi_voice_rate(current_rfreq, current_pfreq);
			return 0;
		}
		stream_p = stream_r;
		const PaStreamInfo* si = Pa_GetStreamInfo(stream_r);
		current_rfreq = input ? si->sampleRate : 0;
		si = Pa_GetStreamInfo(stream_p);
		current_pfreq = output ? si->sampleRate : 0;
		current_rfreq = input ? si->sampleRate : 0;
		current_rdev = dev;
		current_pdev = dev;
		audioapi_voice_rate(current_rfreq, current_pfreq);
		print_status(3);
		return 3;
	}

	unsigned switch_devices(PaDeviceIndex new_in, PaDeviceIndex new_out)
	{
		bool cur_full_duplex = (current_pdev == current_rdev);
		bool new_full_duplex = (new_in == new_out);
		bool switch_in = (new_in != current_rdev);
		bool switch_out = (new_out != current_pdev);
		if(!switch_in && !switch_out)
			return 3;
		else if(!switch_in && !cur_full_duplex && !new_full_duplex)
			return switch_devices_outonly(new_out);
		else if(!switch_out && !cur_full_duplex && !new_full_duplex)
			return switch_devices_inonly(new_in);
		else if(!new_full_duplex)
			return switch_devices_split(new_in, new_out);
		else
			return switch_devices_full(new_out);
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
				PaDeviceIndex in, out;
				if(!o_r)
					in = ridx;
				else if(check_indev(idx))
					in = idx;
				else
					in = paNoDevice;
				if(!o_p)
					out = pidx;
				else if(check_outdev(idx))
					out = idx;
				else
					out = paNoDevice;
				r = switch_devices(in, out);
				if((r & 1) && in != paNoDevice) {
					o_r = false;
					ridx = idx;
				}
				if((r & 2) && (out != paNoDevice)) {
					o_p = false;
					pidx = idx;
				}
				if(r == 3 && (in != paNoDevice) && (out != paNoDevice))
					return true;
			}
			//Get output open if possible.
			if(!o_p)
				return (switch_devices(paNoDevice, pidx) != 0);
			return false;
		}
	}

	PaDeviceIndex output_to_index(const std::string& dev)
	{
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
		return idx;
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
			switch_devices(paNoDevice, paNoDevice);
			Pa_Terminate();
			init_flag = false;
		},
		.enable = [](bool _enable) -> void { was_enabled = _enable; },
		.initialized = []() -> bool { return init_flag; },
		.set_device = [](const std::string& pdev, const std::string& rdev) -> void {
			bool failed = false;
			PaDeviceIndex pidx = output_to_index(pdev);
			PaDeviceIndex ridx = output_to_index(rdev);
			auto ret = switch_devices(ridx, pidx);
			if(ret == 1)
				throw std::runtime_error("Can't change output device");
			if(ret == 2)
				throw std::runtime_error("Can't change input device");
			if(ret == 0)
				throw std::runtime_error("Can't change audio devices");
		},
		.get_device = [](bool rec) -> std::string {
			if(rec)
				if(current_rdev == paNoDevice)
					return "null";
				else
					return (stringfmt() << current_rdev).str();
			else
				if(current_pdev == paNoDevice)
					return "null";
				else
					return (stringfmt() << current_pdev).str();
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

