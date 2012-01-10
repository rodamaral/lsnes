#include "jmd.hpp"

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
	numeric_setting clevel("jmd-compression", 0, 9, 7);

	class jmd_avsnoop : public information_dispatch
	{
	public:
		jmd_avsnoop(const std::string& filename, unsigned level) throw(std::bad_alloc)
			: information_dispatch("dump-jmd")
		{
			enable_send_sound();
			vid_dumper = new jmd_dumper(filename, level);
			have_dumped_frame = false;
			audio_w = 0;
			audio_n = 0;
			video_w = 0;
			video_n = 0;
			maxtc = 0;
			soundrate = get_sound_rate();
			try {
				on_gameinfo(get_gameinfo());
			} catch(std::exception& e) {
				messages << "Can't write gameinfo: " << e.what() << std::endl;
			}
		}

		~jmd_avsnoop() throw()
		{
			delete vid_dumper;
		}

		void on_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
		{
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
			dscr.set_palette(0, 8, 16);
			dscr.reallocate(lrc.left_gap + _frame.width + lrc.right_gap, lrc.top_gap + _frame.height +
				lrc.bottom_gap, false);
			dscr.set_origin(lrc.left_gap, lrc.top_gap);
			dscr.copy_from(_frame, 1, 1);
			rq.run(dscr);

			vid_dumper->video(get_next_video_ts(fps_n, fps_d), dscr.memory, dscr.width, dscr.height);
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			uint64_t ts = get_next_audio_ts();
			if(have_dumped_frame)
				vid_dumper->audio(ts, l, r);
		}

		void on_dump_end()
		{
			vid_dumper->end(maxtc);
		}

		void on_gameinfo(const struct gameinfo_struct& gi)
		{
			std::string authstr;
			for(size_t i = 0; i < gi.get_author_count(); i++) {
				if(i != 0)
					authstr = authstr + ", ";
				authstr = authstr + gi.get_author_short(i);
			}
			vid_dumper->gameinfo(gi.gamename, authstr, 1000000000ULL * gi.length, gi.get_rerecords());
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		uint64_t get_next_video_ts(uint32_t fps_n, uint32_t fps_d)
		{
			uint64_t ret = video_w;
			video_w += (1000000000ULL * fps_d) / fps_n;
			video_n += (1000000000ULL * fps_d) % fps_n;
			if(video_n >= fps_n) {
				video_n -= fps_n;
				video_w++;
			}
			maxtc = (ret > maxtc) ? ret : maxtc;
			return ret;
		}

		uint64_t get_next_audio_ts()
		{
			uint64_t ret = audio_w;
			audio_w += (1000000000ULL * soundrate.second) / soundrate.first;
			audio_n += (1000000000ULL * soundrate.second) % soundrate.first;
			if(audio_n >= soundrate.first) {
				audio_n -= soundrate.first;
				audio_w++;
			}
			maxtc = (ret > maxtc) ? ret : maxtc;
			return ret;
		}

		jmd_dumper* vid_dumper;
		screen dscr;
		unsigned dcounter;
		bool have_dumped_frame;
		uint64_t audio_w;
		uint64_t audio_n;
		uint64_t video_w;
		uint64_t video_n;
		uint64_t maxtc;
		std::pair<uint32_t, uint32_t> soundrate;
	};

	jmd_avsnoop* vid_dumper;

	void startdump(std::string prefix)
	{
		if(prefix == "")
			throw std::runtime_error("Expected filename");
		if(vid_dumper)
			throw std::runtime_error("JMD dumping already in progress");
		unsigned long level2 = (unsigned long)level2;
		try {
			vid_dumper = new jmd_avsnoop(prefix, level2);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			std::ostringstream x;
			x << "Error starting JMD dump: " << e.what();
			throw std::runtime_error(x.str());
		}
		messages << "Dumping to " << prefix << " at level " << level2 << std::endl;
		information_dispatch::do_dumper_update();
	}

	void enddump()
	{
		if(!vid_dumper)
			throw std::runtime_error("No JMD video dump in progress");
		try {
			vid_dumper->on_dump_end();
			messages << "JMD Dump finished" << std::endl;
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			messages << "Error ending JMD dump: " << e.what() << std::endl;
		}
		delete vid_dumper;
		vid_dumper = NULL;
		information_dispatch::do_dumper_update();
	}

	function_ptr_command<const std::string&> jmd_dump("dump-jmd", "Start JMD capture",
		"Syntax: dump-jmd <file>\nStart JMD capture to <file>.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			std::string prefix = t.tail();
			startdump(prefix);
		});

	function_ptr_command<> end_avi("end-jmd", "End JMD capture",
		"Syntax: end-jmd\nEnd a JMD capture.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			enddump();
		});

	class adv_jmd_dumper : public adv_dumper
	{
	public:
		adv_jmd_dumper() : adv_dumper("INTERNAL-JMD") {information_dispatch::do_dumper_update(); }
		~adv_jmd_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			return x;
		}

		bool wants_prefix(const std::string& mode) throw()
		{
			return false;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "JMD";
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
	
	adv_jmd_dumper::~adv_jmd_dumper() throw()
	{
	}
}
