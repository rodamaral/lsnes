#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/random.hpp"
#include "core/messages.hpp"
#include "core/rom.hpp"
#include "video/sox.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "library/hex.hpp"
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

	class pipedec_dump_obj : public dumper_base
	{
	public:
		pipedec_dump_obj(master_dumper& _mdumper, dumper_factory_base& _fbase, const std::string& mode,
			const std::string& prefix)
			: dumper_base(_mdumper, _fbase), mdumper(_mdumper)
		{
			bool _upsidedown = (mode[0] != 'v');
			bool _swap = (mode[2] == 'R');
			bool _bits32 = (mode[3] == '3');

			if(prefix == "")
				throw std::runtime_error("Expected filename");
			try {
				cmd = get_pipedec_command("!" + mode + ":");
				video = NULL;
				auto r = mdumper.get_rate();
				audio = new sox_dumper(prefix, static_cast<double>(r.first) / r.second, 2);
				if(!audio)
					throw std::runtime_error("Can't open output file");
				have_dumped_frame = false;
				upsidedown = _upsidedown;
				bits32 = _bits32;
				swap = _swap;
	
				last_width = 0;
				last_height = 0;
				last_fps_n = 0;
				last_fps_d = 0;
				segid = hex::from<uint32_t>(get_random_hexstring(8));
				mdumper.add_dumper(*this);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting PIPEDEC dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << std::endl;
		}
		~pipedec_dump_obj() throw()
		{
			mdumper.drop_dumper(*this);
			delete audio;
			if(video)
				pclose(video);
			messages << "PIPEDEC Dump finished" << std::endl;
		}
		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			if(!render_video_hud(dscr, _frame, fps_n, fps_d, 1, 1, 0, 0, 0, 0, NULL))
				return;
			size_t w = dscr.get_width();
			size_t h = dscr.get_height();
			uint32_t stride = dscr.get_stride();

			if(!video || last_width != w || last_height != h || last_fps_n != fps_n ||
				last_fps_d != fps_d) {
				//Segment change.
				tmp.resize(4 * stride + 16);
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
			uint32_t alignment = (16 - reinterpret_cast<size_t>(&tmp[0])) % 16;
			char* data2 = &tmp[alignment];
			for(size_t i = 0; i < h; i++) {
				size_t ri = upsidedown ? (h - i - 1) : i;
				char* data = reinterpret_cast<char*>(dscr.rowptr(ri));
				if(bits32)
					if(swap)
						framebuffer::copy_swap4(reinterpret_cast<uint8_t*>(data2),
							reinterpret_cast<uint32_t*>(data), stride);
					else
						memcpy(data2, data, 4 * stride);
				else
					if(swap)
						framebuffer::copy_drop4s(reinterpret_cast<uint8_t*>(data2),
							reinterpret_cast<uint32_t*>(data), stride);
					else
						framebuffer::copy_drop4(reinterpret_cast<uint8_t*>(data2),
							reinterpret_cast<uint32_t*>(data), stride);

				if(fwrite(data2, bits32 ? 4 : 3, w, video) < w)
					messages << "Video write error" << std::endl;
			}
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(have_dumped_frame && audio)
				audio->sample(l, r);
		}
		void on_rate_change(uint32_t n, uint32_t d)
		{
			messages << "Pipedec: Changing sound rate mid-dump not supported." << std::endl;
		}
		void on_gameinfo_change(const master_dumper::gameinfo& gi)
		{
			//Do nothing.
		}
		void on_end()
		{
			delete this;
		}
	private:
		master_dumper& mdumper;
		FILE* video;
		sox_dumper* audio;
		bool have_dumped_frame;
		struct framebuffer::fb<false> dscr;
		bool upsidedown;
		bool bits32;
		bool swap;
		std::string cmd;
		std::vector<char> tmp;
		uint32_t last_fps_d;
		uint32_t last_fps_n;
		uint32_t last_height;
		uint32_t last_width;
		uint32_t segid;
	};

	class adv_pipedec_dumper : public dumper_factory_base
	{
	public:
		adv_pipedec_dumper() : dumper_factory_base("INTERNAL-PIPEDEC")
		{
			ctor_notify();
		}
		~adv_pipedec_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			x.insert("RGB24");
			x.insert("vGB24");
			x.insert("RGB32");
			x.insert("vGB32");
			x.insert("BGR24");
			x.insert("vGR24");
			x.insert("BGR32");
			x.insert("vGR32");
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
		pipedec_dump_obj* start(master_dumper& _mdumper, const std::string& mode, const std::string& prefix)
			throw(std::bad_alloc, std::runtime_error)
		{
			return new pipedec_dump_obj(_mdumper, *this, mode, prefix);
		}
	} adv;

	adv_pipedec_dumper::~adv_pipedec_dumper() throw()
	{
	}
}
