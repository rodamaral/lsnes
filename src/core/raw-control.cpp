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
#include <fstream>
#include <zlib.h>

namespace
{
	class raw_avsnoop : public information_dispatch
	{
	public:
		raw_avsnoop(const std::string& prefix) throw(std::bad_alloc)
			: information_dispatch("dump-raw")
		{
			enable_send_sound();
			video = new std::ofstream(prefix + ".video", std::ios::out | std::ios::binary);
			audio = new std::ofstream(prefix + ".audio", std::ios::out | std::ios::binary);
			if(!*video || !*audio)
				throw std::runtime_error("Can't open output files");
			have_dumped_frame = false;
		}

		~raw_avsnoop() throw()
		{
			delete video;
			delete audio;
		}

		void on_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			if(!video)
				return;
			struct lua_render_context lrc;
			render_queue rq;
			lrc.left_gap = 0;
			lrc.right_gap = 0;
			lrc.bottom_gap = 0;
			lrc.top_gap = 0;
			lrc.queue = &rq;
			lrc.width = _frame.width;
			lrc.height = _frame.height;
			lua_callback_do_video(&lrc);
			dscr.set_palette(16, 8, 0);
			uint32_t hscl = (_frame.width < 400) ? 2 : 1;
			uint32_t vscl = (_frame.height < 400) ? 2 : 1;
			dscr.reallocate(lrc.left_gap + _frame.width * hscl + lrc.right_gap, lrc.top_gap +
				_frame.height * vscl + lrc.bottom_gap, false);
			dscr.set_origin(lrc.left_gap, lrc.top_gap);
			dscr.copy_from(_frame, hscl, vscl);
			rq.run(dscr);
			for(size_t i = 0; i < dscr.height; i++)
				video->write(reinterpret_cast<char*>(dscr.rowptr(i)), 4 * dscr.width);
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(have_dumped_frame && audio) {
				char buffer[4];
				buffer[0] = static_cast<unsigned short>(l);
				buffer[1] = static_cast<unsigned short>(l) >> 8;
				buffer[2] = static_cast<unsigned short>(r);
				buffer[3] = static_cast<unsigned short>(r) >> 8;
				audio->write(buffer, 4);
			}
		}

		void on_dump_end()
		{
			delete video;
			delete audio;
			video = NULL;
			audio = NULL;
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		std::ofstream* audio;
		std::ofstream* video;
		bool have_dumped_frame;
		struct screen dscr;
	};

	raw_avsnoop* vid_dumper;

	void startdump(std::string prefix)
	{
		if(prefix == "")
			throw std::runtime_error("Expected prefix");
		if(vid_dumper)
			throw std::runtime_error("RAW dumping already in progress");
		try {
			vid_dumper = new raw_avsnoop(prefix);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			std::ostringstream x;
			x << "Error starting RAW dump: " << e.what();
			throw std::runtime_error(x.str());
		}
		messages << "Dumping to " << prefix << std::endl;
		information_dispatch::do_dumper_update();
	}

	void enddump()
	{
		if(!vid_dumper)
			throw std::runtime_error("No RAW video dump in progress");
		try {
			vid_dumper->on_dump_end();
			messages << "RAW Dump finished" << std::endl;
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			messages << "Error ending RAW dump: " << e.what() << std::endl;
		}
		delete vid_dumper;
		vid_dumper = NULL;
		information_dispatch::do_dumper_update();
	}

	function_ptr_command<const std::string&> raw_dump("dump-raw", "Start RAW capture",
		"Syntax: dump-raw <prefix>\nStart RAW capture to <prefix>.video and <prefix>.audio.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			std::string prefix = t.tail();
			startdump(prefix);
		});

	function_ptr_command<> end_raw("end-raw", "End RAW capture",
		"Syntax: end-raw\nEnd a RAW capture.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			enddump();
		});

	class adv_raw_dumper : public adv_dumper
	{
	public:
		adv_raw_dumper() : adv_dumper("INTERNAL-RAW") {information_dispatch::do_dumper_update(); }
		~adv_raw_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			return x;
		}

		bool wants_prefix(const std::string& mode) throw()
		{
			return true;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "RAW";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			return "";
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& targetname) throw(std::bad_alloc,
			std::runtime_error)
		{
			startdump(targetname);
		}

		void end() throw()
		{
			enddump();
		}
	} adv;
	
	adv_raw_dumper::~adv_raw_dumper() throw()
	{
	}
}
