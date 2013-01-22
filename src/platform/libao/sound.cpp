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
#include "library/string.hpp"

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
#include "library/workthread.hpp"
#include <string>
#include <map>
#include <stdexcept>
#include <ao/ao.h>
#include <boost/lexical_cast.hpp>

namespace
{
	std::string current_device = "";
	ao_device* volatile cdev = NULL;
	bool init_flag = false;
	bool was_enabled = false;
	int driver_id = 0;

	class cb_thread : public worker_thread
	{
	public:
		cb_thread() { fire(); }
		void entry()
		{
			int16_t buffer[1024];
			float voicebuf2[512] = {0.0f};
			while(true) {
				audioapi_get_mixed(buffer, 512, true);
				if(!was_enabled)
					memset(buffer, 0, sizeof(buffer));
				ao_device* d = cdev;
				if(d)
					ao_play(d, reinterpret_cast<char*>(buffer), 2048);
				else
					usleep(10000);
				audioapi_put_voice(voicebuf2, 512);
			}
		}
	};

	bool switch_devices(int newdevice, std::string name)
	{
		//If audio is open, close it.
		if(cdev) {
			ao_device* d = cdev;
			cdev = NULL;
			//Wait a bit for the device to close.
			usleep(50000);
			ao_close(d);
			current_device = "";
		}
		//Open new audio.
		if(newdevice != -1) {
			char matrix[128];
			strcpy(matrix, "L,R");
			ao_sample_format sformat;
			ao_option* options = NULL;
			sformat.bits = 16;
			sformat.rate = 48000;
			sformat.channels = 2;
			sformat.byte_format = AO_FMT_NATIVE;
			sformat.matrix = matrix;
			std::string idno = (stringfmt() << driver_id).str();
			ao_append_option(&options, "id", idno.c_str());
			cdev = ao_open_live(newdevice, &sformat, options);
			if(!cdev) {
				int err = errno;
				//Error.
				switch(err) {
				case AO_ENODRIVER:
					throw std::runtime_error("Bad driver ID");
				case AO_ENOTLIVE:
					throw std::runtime_error("Driver doesn't support playback");
				case AO_EBADOPTION:
					throw std::runtime_error("Bad option value");
				case AO_EOPENDEVICE:
					throw std::runtime_error("Can not open device");
				case AO_EFAIL:
					throw std::runtime_error("Unknown error");
				default:
					(stringfmt() << "Error code " << err).throwex();
				}
			}
		}
		if(cdev) {
			audioapi_set_dummy_cb(false);
			audioapi_voice_rate(48000);
			current_device = name;
			messages << "Switched to audio output " << name << std::endl;
		} else {
			audioapi_set_dummy_cb(true);
			current_device = "";
			messages << "Switched to audio output null" << std::endl;
		}
		return true;
	}

	function_ptr_command<const std::string&> x(lsnes_cmd, "libao-set-id", "", "",
		[](const std::string& value) throw(std::bad_alloc, std::runtime_error) {
			driver_id = parse_value<int>(value);
		});

	struct _audioapi_driver drv = {
		.init = []() -> void {
			ao_initialize();
			init_flag = true;
			current_device = "";

			int dcount;
			int defaultid = ao_default_driver_id();
			std::string dname = "";
			ao_info** drvs = ao_driver_info_list(&dcount);
			messages << "Detected " << dcount << " sound output devices." << std::endl;
			for(int j = 0; j < dcount; j++) {
				messages << "Audio device " << drvs[j]->short_name << ": " << drvs[j]->name
					<< std::endl;
				if(j == defaultid)
					dname = drvs[j]->short_name;
			}
			new cb_thread;
			was_enabled = true;
			try {
				switch_devices(defaultid, dname);
			} catch(...) {
				switch_devices(-1, "");
			}
		},
		.quit = []() -> void {
			if(!init_flag)
				return;
			switch_devices(-1, "");
			ao_shutdown();
			init_flag = false;
		},
		.enable = [](bool enable) -> void { was_enabled = enable; },
		.initialized = []() -> bool { return init_flag; },
		.set_device = [](const std::string& dev) -> void {
			if(dev == "null") {
				if(!switch_devices(-1, ""))
					throw std::runtime_error("Failed to switch sound outputs");
			} else {
				int idx = ao_driver_id(dev.c_str());
				if(idx == -1)
					throw std::runtime_error("Invalid device '" + dev + "'");
				try {
					if(!switch_devices(idx, dev))
						throw std::runtime_error("Failed to switch sound outputs");
				} catch(std::exception& e) {
					throw std::runtime_error(std::string("Failed to switch sound outputs: ") +
						e.what());
				}
			}
		},
		.get_device = []() -> std::string {
			if(current_device == "")
				return "null";
			else
				return current_device;
		},
		.get_devices = []() -> std::map<std::string, std::string> {
			std::map<std::string, std::string> ret;
			ret["null"] = "null sound output";
			int dcount;
			ao_info** drvs = ao_driver_info_list(&dcount);
			if(!drvs)
				return ret;
			for(int j = 0; j < dcount; j++)
				if(drvs[j]->type == AO_TYPE_LIVE)
					ret[drvs[j]->short_name] = drvs[j]->name;
			return ret;
		},
		.name = []() -> const char* { return "Libao sound plugin"; }
	};
	audioapi_driver _drv(drv);
}
