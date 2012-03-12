#include "lsnes.hpp"
//#include <snes/snes.hpp>
#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "interface/core.hpp"
#include "library/serialization.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <zlib.h>
#include <sstream>
#include <fstream>
#include <stdexcept>

#define CUTOFF 2100000000
#define SDUMP_FLAG_HIRES 1
#define SDUMP_FLAG_INTERLACED 2
#define SDUMP_FLAG_OVERSCAN 4
#define SDUMP_FLAG_PAL 8

namespace
{
	class sdmp_avsnoop : public information_dispatch
	{
	public:
		sdmp_avsnoop(const std::string& prefix, bool ssflag) throw(std::bad_alloc)
			: information_dispatch("dump-sdmp")
		{
			enable_send_sound();
			oprefix = prefix;
			sdump_ss = ssflag;
			ssize = 0;
			next_seq = 0;
			sdump_iopen = false;
		}

		~sdmp_avsnoop() throw()
		{
			try {
				if(sdump_iopen)
					out.close();
			} catch(...) {
			}
		}

		void on_raw_frame(const uint32_t* raw, bool hires, bool interlaced, bool overscan, unsigned region)
		{
			unsigned flags = 0;
			flags |= (hires ? SDUMP_FLAG_HIRES : 0);
			flags |= (interlaced ? SDUMP_FLAG_INTERLACED : 0);
			flags |= (overscan ? SDUMP_FLAG_OVERSCAN : 0);
			flags |= (region == VIDEO_REGION_PAL ? SDUMP_FLAG_PAL : 0);
			unsigned char tbuffer[2049];
			if(!sdump_iopen || (ssize > CUTOFF && !sdump_ss)) {
				std::cerr << "Starting new segment" << std::endl;
				if(sdump_iopen)
					out.close();
				std::ostringstream str;
				if(sdump_ss)
					str << oprefix;
				else
					str << oprefix << "_" << std::setw(4) << std::setfill('0') << (next_seq++)
						<< ".sdmp";
				std::string str2 = str.str();
				out.open(str2.c_str(), std::ios::out | std::ios::binary);
				if(!out)
					throw std::runtime_error("Failed to open '" + str2 + "'");
				sdump_iopen = true;
				write32ube(tbuffer, 0x53444D50U);
				write32ube(tbuffer + 4, emucore_get_video_rate().first);
				write32ube(tbuffer + 8, emucore_get_audio_rate().first);
				out.write(reinterpret_cast<char*>(tbuffer), 12);
				if(!out)
					throw std::runtime_error("Failed to write header to '" + str2 + "'");
				ssize = 12;
			}
			tbuffer[0] = flags;
			for(unsigned i = 0; i < 512; i++) {
				for(unsigned j = 0; j < 512; j++)
					write32ube(tbuffer + (4 * j + 1), raw[512 * i + j]);
				out.write(reinterpret_cast<char*>(tbuffer + (i ? 1 : 0)), i ? 2048 : 2049);
			}
			if(!out)
				throw std::runtime_error("Failed to write frame");
			ssize += 1048577;
		}

		void on_sample(short l, short r)
		{
			if(!sdump_iopen)
				return;
			unsigned char pkt[5];
			pkt[0] = 16;
			write16sbe(pkt + 1, l);
			write16sbe(pkt + 3, r);
			out.write(reinterpret_cast<char*>(pkt), 5);
			if(!out)
				throw std::runtime_error("Failed to write sample");
			ssize += 5;
		}

		void on_dump_end()
		{
			if(sdump_iopen)
				out.close();
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		std::string oprefix;
		bool sdump_ss;
		uint64_t ssize;
		uint64_t next_seq;
		bool sdump_iopen;
		std::ofstream out;
	};

	sdmp_avsnoop* vid_dumper;

	class adv_sdmp_dumper : public adv_dumper
	{
	public:
		adv_sdmp_dumper() : adv_dumper("INTERNAL-SDMP") { information_dispatch::do_dumper_update(); }
		~adv_sdmp_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			x.insert("ss");
			x.insert("ms");
			return x;
		}

		bool wants_prefix(const std::string& mode) throw()
		{
			return (mode != "ss");
		}

		std::string name() throw(std::bad_alloc)
		{
			return "SDMP";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			return (mode == "ss" ? "Single-Segment" : "Multi-Segment");
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& prefix) throw(std::bad_alloc,
			std::runtime_error)
		{
			if(prefix == "") {
				if(mode == "ss")
					throw std::runtime_error("Expected filename");
				else
					throw std::runtime_error("Expected prefix");
			}
			if(vid_dumper)
				throw std::runtime_error("SDMP Dump already in progress");
			try {
				vid_dumper = new sdmp_avsnoop(prefix, mode == "ss");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting SDMP dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			if(mode == "ss")
				messages << "Dumping SDMP (SS) to " << prefix << std::endl;
			else
				messages << "Dumping SDMP to " << prefix << std::endl;
			information_dispatch::do_dumper_update();
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No SDMP video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "SDMP Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending SDMP dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;
	
	adv_sdmp_dumper::~adv_sdmp_dumper() throw()
	{
	}
}
