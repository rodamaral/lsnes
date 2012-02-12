#include "video/sox.hpp"
#include "library/serialization.hpp"

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
		write64ule(buf, v2);
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
		write64ule(buffer, 0x1C586F532E);		//Magic and header size.
		write_double(buffer + 16, samplerate);
		write32ule(buffer + 24, channels);
		sox_file.write(reinterpret_cast<char*>(buffer), 32);
		if(!sox_file)
			throw std::runtime_error("Can't write audio header");
		samplebuffer.resize(channels);
		databuf.resize(channels << 2);
		samples_dumped = 0;
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
	write64ule(buffer, raw_samples);
	sox_file.write(reinterpret_cast<char*>(buffer), 8);
	if(!sox_file)
		throw std::runtime_error("Can't fixup audio header");
	sox_file.close();
}

void sox_dumper::internal_dump_sample()
{
	for(size_t i = 0; i < samplebuffer.size(); ++i)
		write32ule(&databuf[4 * i], static_cast<uint32_t>(samplebuffer[i]));
	sox_file.write(&databuf[0], databuf.size());
	if(!sox_file)
		throw std::runtime_error("Failed to dump sample");
	samples_dumped++;
}
