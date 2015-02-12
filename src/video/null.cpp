#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/messages.hpp"
#include "core/rom.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"

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
	void deleter_fn(void* f)
	{
		delete reinterpret_cast<std::ofstream*>(f);
	}

	class null_dump_obj : public dumper_base
	{
	public:
		null_dump_obj(master_dumper& _mdumper, dumper_factory_base& _fbase, const std::string& mode,
			const std::string& prefix)
			: dumper_base(_mdumper, _fbase), mdumper(_mdumper)
		{
			try {
				mdumper.add_dumper(*this);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting NULL dump: " << e.what();
				throw std::runtime_error(x.str());
			}
		}
		~null_dump_obj() throw()
		{
			mdumper.drop_dumper(*this);
		}
		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			//Do nothing.
		}
		void on_sample(short l, short r)
		{
			//Do nothing.
		}
		void on_rate_change(uint32_t n, uint32_t d)
		{
			//Do nothing.
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
	};

	class adv_null_dumper : public dumper_factory_base
	{
	public:
		adv_null_dumper() : dumper_factory_base("INTERNAL-NULL")
		{
			ctor_notify();
		}
		~adv_null_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			return x;
		}
		unsigned mode_details(const std::string& mode) throw()
		{
			return target_type_special;
		}
		std::string mode_extension(const std::string& mode) throw()
		{
			return "";	//Nothing interesting.
		}
		std::string name() throw(std::bad_alloc)
		{
			return "NULL";
		}
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			return "";
		}
		null_dump_obj* start(master_dumper& _mdumper, const std::string& mode, const std::string& prefix)
			throw(std::bad_alloc, std::runtime_error)
		{
			return new null_dump_obj(_mdumper, *this, mode, prefix);
		}
		bool hidden() const { return true; }
	} adv;

	adv_null_dumper::~adv_null_dumper() throw()
	{
	}
}
