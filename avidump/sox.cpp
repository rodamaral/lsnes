#include "sox.hpp"
#include <iostream>

namespace
{
	void write_double(uint8_t* buf, double v)
	{
		unsigned mag = 1023;
		while(v >= 2) {
			mag++;
			v /= 2;
		}
		while(v < 1) {
			mag--;
			v *= 2;
		}
		uint64_t v2 = mag;
		v -= 1;
		for(unsigned i = 0; i < 52; i++) {
			v *= 2;
			v2 = 2 * v2 + ((v >= 1) ? 1 : 0);
			if(v >= 1)
				v -= 1;
		}
		buf[0] = v2;
		buf[1] = v2 >> 8;
		buf[2] = v2 >> 16;
		buf[3] = v2 >> 24;
		buf[4] = v2 >> 32;
		buf[5] = v2 >> 40;
		buf[6] = v2 >> 48;
		buf[7] = v2 >> 56;
	}
}

sox_dumper::sox_dumper(const std::string& filename, double samplerate, uint32_t channels) throw(std::bad_alloc,
		std::runtime_error)
{
	sox_file.open(filename.c_str(), std::ios::out | std::ios::binary);
	if(!sox_file)
		throw std::runtime_error("Can't open sox file for output");
	try {
		uint8_t buffer[32] = {0};
		buffer[0] = 0x2E;	//Magic.
		buffer[1] = 0x53;	//Magic.
		buffer[2] = 0x6F;	//Magic.
		buffer[3] = 0x58;	//Magic.
		buffer[4] = 0x1C;	//Header size.
		write_double(buffer + 16, samplerate);
		buffer[24] = channels;
		buffer[25] = channels >> 8;
		buffer[26] = channels >> 16;
		buffer[27] = channels >> 24;
		sox_file.write(reinterpret_cast<char*>(buffer), 32);
		if(!sox_file)
			throw std::runtime_error("Can't write audio header");
		samplebuffer.resize(channels);
		databuf.resize(channels << 2);
	} catch(...) {
		sox_file.close();
		throw;
	}
}

sox_dumper::~sox_dumper() throw()
{
	try {
		close();
	} catch(...) {
	}
}

void sox_dumper::close() throw(std::bad_alloc, std::runtime_error)
{
	sox_file.seekp(8, std::ios::beg);
	uint8_t buffer[8];
	uint64_t raw_samples = samples_dumped * samplebuffer.size();
	buffer[0] = raw_samples;
	buffer[1] = raw_samples >> 8;
	buffer[2] = raw_samples >> 16;
	buffer[3] = raw_samples >> 24;
	buffer[4] = raw_samples >> 32;
	buffer[5] = raw_samples >> 40;
	buffer[6] = raw_samples >> 48;
	buffer[7] = raw_samples >> 56;
	sox_file.write(reinterpret_cast<char*>(buffer), 8);
	if(!sox_file)
		throw std::runtime_error("Can't fixup audio header");
	sox_file.close();
}

void sox_dumper::internal_dump_sample()
{
	for(size_t i = 0; i < samplebuffer.size(); ++i) {
		uint32_t v = samplebuffer[i];
		databuf[4 * i + 0] = v;
		databuf[4 * i + 1] = v >> 8;
		databuf[4 * i + 2] = v >> 16;
		databuf[4 * i + 3] = v >> 24;
	}
	sox_file.write(&databuf[0], databuf.size());
	if(!sox_file)
		throw std::runtime_error("Failed to dump sample");
	samples_dumped++;
}
