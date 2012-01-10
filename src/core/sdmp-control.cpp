#include "sdmp.hpp"
#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/lua.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <zlib.h>

namespace
{
	class sdmp_avsnoop : public information_dispatch
	{
	public:
		sdmp_avsnoop(const std::string& prefix, bool ssflag) throw(std::bad_alloc)
			: information_dispatch("dump-sdmp")
		{
			enable_send_sound();
			dumper = new sdump_dumper(prefix, ssflag);
		}

		~sdmp_avsnoop() throw()
		{
			delete dumper;
		}

		void on_raw_frame(const uint32_t* raw, bool hires, bool interlaced, bool overscan, unsigned region)
		{
			unsigned flags = 0;
			dumper->frame(raw, (hires ? SDUMP_FLAG_HIRES : 0) | (interlaced ? SDUMP_FLAG_INTERLACED : 0) |
				(overscan ? SDUMP_FLAG_OVERSCAN : 0) | (region == VIDEO_REGION_PAL ? SDUMP_FLAG_PAL :
				0));
		}

		void on_sample(short l, short r)
		{
			dumper->sample(l, r);
		}

		void on_dump_end()
		{
			dumper->end();
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		sdump_dumper* dumper;
	};

	sdmp_avsnoop* vid_dumper;

	void startdump(bool ss, const std::string& prefix)
	{
		if(prefix == "") {
			if(ss)
				throw std::runtime_error("Expected filename");
			else
				throw std::runtime_error("Expected prefix");
		}
		if(vid_dumper)
			throw std::runtime_error("SDMP Dump already in progress");
		try {
			vid_dumper = new sdmp_avsnoop(prefix, ss);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			std::ostringstream x;
			x << "Error starting SDMP dump: " << e.what();
			throw std::runtime_error(x.str());
		}
		if(ss)
			messages << "Dumping SDMP (SS) to " << prefix << std::endl;
		else
			messages << "Dumping SDMP to " << prefix << std::endl;
		information_dispatch::do_dumper_update();
	}

	void enddump()
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

	function_ptr_command<const std::string&> sdmp_dump("dump-sdmp", "Start sdmp capture",
		"Syntax: dump-sdmp <prefix>\nStart SDMP capture to <prefix>\n",
		[](const std::string& prefix) throw(std::bad_alloc, std::runtime_error) {
			startdump(false, prefix);
		});

	function_ptr_command<const std::string&> sdmp_dumpss("dump-sdmpss", "Start SS sdmp capture",
		"Syntax: dump-sdmpss <file>\nStart SS SDMP capture to <file>\n",
		[](const std::string& prefix) throw(std::bad_alloc, std::runtime_error) {
			startdump(true, prefix);
		});

	function_ptr_command<> end_avi("end-sdmp", "End SDMP capture",
		"Syntax: end-sdmp\nEnd a SDMP capture.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			enddump();
		});

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

		void start(const std::string& mode, const std::string& targetname) throw(std::bad_alloc,
			std::runtime_error)
		{
			startdump((mode == "ss"), targetname);
		}

		void end() throw()
		{
			enddump();
		}
	} adv;
	
	adv_sdmp_dumper::~adv_sdmp_dumper() throw()
	{
	}
}
