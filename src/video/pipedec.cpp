#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/window.hpp"
#include "video/sox.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include <fcntl.h>

#include <iomanip>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <fstream>
#include <zlib.h>

namespace
{
	uint64_t akill = 0;
	double akillfrac = 0;

	std::string get_pipedec_command(const std::string& type)
	{
		auto r = regex("(.*)[\\/][^\\/]+", get_config_path());
		if(!r)
			throw std::runtime_error("Can't read pipedec config file");
		std::ifstream cfg(r[1] + "/pipedec/.pipedec");
		if(!cfg)
			throw std::runtime_error("Can't read pipedec config file");
		std::string line;
		while(std::getline(cfg, line)) {
			if(line.length() >= type.length() && line.substr(0, type.length()) == type)
				return line.substr(type.length());
		}
		throw std::runtime_error("No command found for " + type);
	}

	std::string substitute_cmd(std::string cmd, uint32_t w, uint32_t h, uint32_t fps_n, uint32_t fps_d,
		uint32_t& segid)
	{
		bool perc_flag = false;
		std::string out;
		for(size_t i = 0; i < cmd.length(); i++) {
			char ch = cmd[i];
			if(perc_flag) {
				switch(ch) {
				case 'w':
					out = out + (stringfmt() << w).str();
					break;
				case 'h':
					out = out + (stringfmt() << h).str();
					break;
				case 'n':
					out = out + (stringfmt() << fps_n).str();
					break;
				case 'd':
					out = out + (stringfmt() << fps_d).str();
					break;
				case 'u':
					out = out + (stringfmt() << time(NULL)).str();
					break;
				case 'i':
					out = out + (stringfmt() << segid++).str();
					break;
				case '%':
					out = out + "%";
					break;
				}
				perc_flag = false;
			} else if(ch == '%')
				perc_flag = true;
			else
				out.append(1, ch);
		}
		return out;
	}

	unsigned char hex(char ch)
	{
		switch(ch) {
		case '0':			return 0;
		case '1':			return 1;
		case '2':			return 2;
		case '3':			return 3;
		case '4':			return 4;
		case '5':			return 5;
		case '6':			return 6;
		case '7':			return 7;
		case '8':			return 8;
		case '9':			return 9;
		case 'a':	case 'A':	return 10;
		case 'b':	case 'B':	return 11;
		case 'c':	case 'C':	return 12;
		case 'd':	case 'D':	return 13;
		case 'e':	case 'E':	return 14;
		case 'f':	case 'F':	return 15;
		};
		return 16;
	}

	class pipedec_avsnoop : public information_dispatch
	{
	public:
		pipedec_avsnoop(const std::string& file, bool _upsidedown, bool _bits32, const std::string& mode)
			: information_dispatch("dump-pipedec")
		{
			enable_send_sound();

			cmd = get_pipedec_command("!" + mode + ":");
			video = NULL;
			auto soundrate = get_sound_rate();
			audio = new sox_dumper(file, static_cast<double>(soundrate.first) /
				soundrate.second, 2);
			if(!audio)
				throw std::runtime_error("Can't open output file");
			have_dumped_frame = false;
			upsidedown = _upsidedown;
			bits32 = _bits32;

			last_width = 0;
			last_height = 0;
			last_fps_n = 0;
			last_fps_d = 0;
			std::string tmp2 = get_random_hexstring(8);
			segid = 0;
			for(unsigned i = 0; i < tmp2.length(); i++)
				segid = 16 * segid + hex(tmp2[i]);
		}

		~pipedec_avsnoop() throw()
		{
			delete audio;
			if(video)
				pclose(video);
		}

		void on_frame(struct framebuffer_raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			unsigned r, g, b;
			unsigned short magic = 258;
			if(*reinterpret_cast<char*>(&magic) == 1) { r = 8; g = 16; b = 24; }
			else { r = 16; g = 8; b = 0; }

			if(!render_video_hud(dscr, _frame, 1, 1, r, g, b, 0, 0, 0, 0, NULL)) {
				akill += killed_audio_length(fps_n, fps_d, akillfrac);
				return;
			}
			size_t w = dscr.get_width();
			size_t h = dscr.get_height();

			if(!video || last_width != w || last_height != h || last_fps_n != fps_n ||
				last_fps_d != fps_d) {
				//Segment change.
				tmp.resize(3 * w);
				std::string rcmd = substitute_cmd(cmd, w, h, fps_n, fps_d, segid);
				if(video)
					pclose(video);
				video = popen(rcmd.c_str(), "w");
#if defined(_WIN32) || defined(_WIN64)
				if(video)
					setmode(fileno(video), O_BINARY);
#endif
				if(!video) {
					int err = errno;
					messages << "Error starting a segment (" << err << ")" << std::endl;
					return;
				}
				last_width = w;
				last_height = h;
				last_fps_n = fps_n;
				last_fps_d = fps_d;
			}
			if(!video)
				return;
			for(size_t i = 0; i < h; i++) {
				size_t ri = upsidedown ? (h - i - 1) : i;
				char* data = reinterpret_cast<char*>(dscr.rowptr(ri));
				char* data2 = bits32 ? data : &tmp[0];
				if(!bits32)
					for(size_t i = 0; i < w; i++) {
						tmp[3 * i + 0] = data[4 * i + 0];
						tmp[3 * i + 1] = data[4 * i + 1];
						tmp[3 * i + 2] = data[4 * i + 2];
					}
				if(fwrite(data2, bits32 ? 4 : 3, w, video) < w)
					messages << "Video write error" << std::endl;
			}
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(akill) {
				akill--;
				return;
			}
			if(have_dumped_frame && audio)
				audio->sample(l, r);
		}

		void on_dump_end()
		{
			if(video)
				pclose(video);
			delete audio;
			video = NULL;
			audio = NULL;
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		FILE* video;
		sox_dumper* audio;
		bool have_dumped_frame;
		struct framebuffer<false> dscr;
		bool upsidedown;
		bool bits32;
		std::string cmd;
		std::vector<char> tmp;
		uint32_t last_fps_d;
		uint32_t last_fps_n;
		uint32_t last_height;
		uint32_t last_width;
		uint32_t segid;
	};

	pipedec_avsnoop* vid_dumper;

	class adv_pipedec_dumper : public adv_dumper
	{
	public:
		adv_pipedec_dumper() : adv_dumper("INTERNAL-PIPEDEC")
		{
			information_dispatch::do_dumper_update();
		}
		~adv_pipedec_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			x.insert("RGB24");
			x.insert("vGB24");
			x.insert("RGB32");
			x.insert("vGB32");
			return x;
		}

		unsigned mode_details(const std::string& mode) throw()
		{
			return target_type_file;
		}

		std::string mode_extension(const std::string& mode) throw()
		{
			return "sox";
		}

		std::string name() throw(std::bad_alloc)
		{
			return "PIPEDEC";
		}

		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			return "!" + mode;
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& prefix) throw(std::bad_alloc,
			std::runtime_error)
		{
			bool upsidedown = (mode[0] == 'R');
			bool bits32 = (mode[3] == '3');

			if(prefix == "")
				throw std::runtime_error("Expected filename");
			if(vid_dumper)
				throw std::runtime_error("PIPEDEC dumping already in progress");
			try {
				vid_dumper = new pipedec_avsnoop(prefix, upsidedown, bits32, mode);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting PIPEDEC dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << std::endl;
			information_dispatch::do_dumper_update();
			akill = 0;
			akillfrac = 0;
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No PIPEDEC video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "PIPEDEC Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending PIPEDEC dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;

	adv_pipedec_dumper::~adv_pipedec_dumper() throw()
	{
	}
}
