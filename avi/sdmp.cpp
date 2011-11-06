#include "lsnes.hpp"
#include <snes/snes.hpp>

#include "sdmp.hpp"

#include <sstream>
#include <iomanip>
#include <stdexcept>

#define CUTOFF 2100000000

sdump_dumper::sdump_dumper(const std::string& prefix, bool ss)
{
	oprefix = prefix;
	sdump_ss = ss;
	ssize = 0;
	next_seq = 0;
	sdump_iopen = false;
}

sdump_dumper::~sdump_dumper() throw()
{
	try {
		end();
	} catch(...) {
	}
}

void sdump_dumper::frame(const uint32_t* buffer, unsigned flags)
{
	flags &= 0xF;
	unsigned char tbuffer[2049];
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

void sdump_dumper::sample(short left, short right)
{
	if(!sdump_iopen)
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

void sdump_dumper::end()
{
	if(sdump_iopen)
		out.close();
}
