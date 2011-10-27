#include "lsnes.hpp"
#include <snes/snes.hpp>
#include "sdump.hpp"
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include "avsnoop.hpp"
#include "command.hpp"
#include "misc.hpp"

#define CUTOFF 2100000000

namespace
{
	bool sdump_in_progress;
	std::string oprefix;
	std::ofstream out;
	uint32_t ssize;
	uint32_t next_seq;
	bool sdump_iopen;
	bool sdump_ss;

	class dummy_avsnoop : public av_snooper
	{
	public:
		dummy_avsnoop() throw(std::bad_alloc)
		{
		}

		~dummy_avsnoop() throw()
		{
		}

		void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d) throw(std::bad_alloc,
			std::runtime_error)
		{
		}

		void sample(short l, short r) throw(std::bad_alloc, std::runtime_error)
		{
		}

		void end() throw(std::bad_alloc, std::runtime_error)
		{
			if(sdump_iopen)
				out.close();
			if(sdump_in_progress)
				sdump_in_progress = false;
		}

		void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
		authors, double gametime, const std::string& rerecords) throw(std::bad_alloc, std::runtime_error)
		{
		}
	};
	dummy_avsnoop* snooper;

	function_ptr_command<const std::string&> jmd_dump("dump-sdmp", "Start sdmp capture",
		"Syntax: dump-sdmp <prefix>\nStart SDMP capture to <prefix>\n",
		[](const std::string& prefix) throw(std::bad_alloc, std::runtime_error) {
			if(prefix == "")
				throw std::runtime_error("Expected filename");
			sdump_open(prefix, false);
			messages << "Dumping to " << prefix << std::endl;
		});

	function_ptr_command<const std::string&> jmd_dumpss("dump-sdmpss", "Start SS sdmp capture",
		"Syntax: dump-sdmpss <file>\nStart SS SDMP capture to <file>\n",
		[](const std::string& prefix) throw(std::bad_alloc, std::runtime_error) {
			if(prefix == "")
				throw std::runtime_error("Expected filename");
			sdump_open(prefix, true);
			messages << "Dumping to " << prefix << std::endl;
		});

	function_ptr_command<> end_avi("end-sdmp", "End SDMP capture",
		"Syntax: end-sdmp\nEnd a SDMP capture.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			sdump_close();
			messages << "Dump finished" << std::endl;
		});
}

void sdump_open(const std::string& prefix, bool ss)
{
	if(sdump_in_progress)
		throw std::runtime_error("Dump already in progress");
	oprefix = prefix;
	snooper = new dummy_avsnoop;
	sdump_ss = ss;
	sdump_in_progress = true;
	ssize = 0;
	next_seq = 0;
	sdump_iopen = false;
}

void sdump_close()
{
	if(!sdump_in_progress)
		throw std::runtime_error("No dump in progress");
	if(sdump_iopen)
		out.close();
	sdump_in_progress = false;
	delete snooper;
}

void sdump_frame(const uint32_t* buffer, unsigned flags)
{
	flags &= 0xF;
	unsigned char tbuffer[2049];
	if(!sdump_in_progress)
		return;
	if(!sdump_iopen || (ssize > CUTOFF && !sdump_ss)) {
		std::cerr << "Starting new segment" << std::endl;
		if(sdump_iopen)
			out.close();
		std::ostringstream str;
		if(sdump_ss)
			str << oprefix;
		else
			str << oprefix << "_" << std::setw(4) << std::setfill('0') << (next_seq++) << ".sdmp";
		std::string str2 = str.str();
		out.open(str2.c_str(), std::ios::out | std::ios::binary);
		if(!out)
			throw std::runtime_error("Failed to open '" + str2 + "'");
		sdump_iopen = true;
		tbuffer[0] = 'S';
		tbuffer[1] = 'D';
		tbuffer[2] = 'M';
		tbuffer[3] = 'P';
		uint32_t apufreq = SNES::system.apu_frequency();
		uint32_t cpufreq = SNES::system.cpu_frequency();
		tbuffer[4] = cpufreq >> 24;
		tbuffer[5] = cpufreq >> 16;
		tbuffer[6] = cpufreq >> 8;
		tbuffer[7] = cpufreq;
		tbuffer[8] = apufreq >> 24;
		tbuffer[9] = apufreq >> 16;
		tbuffer[10] = apufreq >> 8;
		tbuffer[11] = apufreq;
		out.write(reinterpret_cast<char*>(tbuffer), 12);
		if(!out)
			throw std::runtime_error("Failed to write header to '" + str2 + "'");
		ssize = 12;
	}
	tbuffer[0] = flags;
	for(unsigned i = 0; i < 512; i++) {
		for(unsigned j = 0; j < 512; j++) {
			tbuffer[4 * j + 1] = buffer[512 * i + j] >> 24;
			tbuffer[4 * j + 2] = buffer[512 * i + j] >> 16;
			tbuffer[4 * j + 3] = buffer[512 * i + j] >> 8;
			tbuffer[4 * j + 4] = buffer[512 * i + j];
		}
		out.write(reinterpret_cast<char*>(tbuffer + (i ? 1 : 0)), i ? 2048 : 2049);
	}
	if(!out)
		throw std::runtime_error("Failed to write frame");
	ssize += 1048577;
}

void sdump_sample(short left, short right)
{
	if(!sdump_in_progress || !sdump_iopen)
		return;
	unsigned char pkt[5];
	pkt[0] = 16;
	pkt[1] = static_cast<unsigned short>(left) >> 8;
	pkt[2] = static_cast<unsigned short>(left);
	pkt[3] = static_cast<unsigned short>(right) >> 8;
	pkt[4] = static_cast<unsigned short>(right);
	out.write(reinterpret_cast<char*>(pkt), 5);
	if(!out)
		throw std::runtime_error("Failed to write sample");
	ssize += 5;
}
