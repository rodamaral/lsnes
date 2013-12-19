#include "library/filesys.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include <iostream>

struct stream_statistics
{
	stream_statistics()
	{
		size = length = packets = clusters = 0;
	}
	stream_statistics operator+(const stream_statistics& s) const
	{
		stream_statistics r;
		r.size = size + s.size;
		r.length = length + s.length;
		r.packets = packets + s.packets;
		r.clusters = clusters + s.clusters;
		return r;
	}
	stream_statistics& operator+=(const stream_statistics& s)
	{
		*this = *this + s;
		return *this;
	}
	uint64_t size;
	uint64_t length;
	uint64_t packets;
	uint64_t clusters;
};

stream_statistics dump_stream(filesystem& fs, uint32_t ccluster, uint32_t dcluster)
{
	stream_statistics ss;
	uint32_t ctrl_c = ccluster;
	uint32_t ctrl_o = 0;
	uint32_t data_c = dcluster;
	uint32_t data_o = 0;
	ss.clusters = 2;
	while(true) {
		uint32_t ctrl_uc = ctrl_c;
		uint32_t ctrl_uo = ctrl_o;
		uint32_t data_uc = data_c;
		uint32_t data_uo = data_o;
		char buffer[4];
		size_t r = fs.read_data(ctrl_c, ctrl_o, buffer, 4);
		if(r < 4)
			break;
		uint16_t psize = serialization::u16b(buffer + 0);
		uint16_t plength = 120 * serialization::u8b(buffer + 2);
		if(!serialization::u8b(buffer + 3))
			break;
		r = fs.skip_data(data_c, data_o, psize);
		if(r < psize)
			(stringfmt() << "Unexpected end of data, read " << r << " bytes, expected "
				<< psize).throwex();
		ss.size += psize;
		ss.length += plength;
		ss.packets++;
		if(ctrl_uc != ctrl_c)
			ss.clusters++;
		if(data_uc != data_c)
			ss.clusters++;
		std::cout << "\tPacket: " << psize << " bytes@" << data_uc << ":" << data_uo << "(" << ctrl_uc
			<< ":" << ctrl_uo << "), " << plength << " samples." << std::endl;
	}
	std::cout << "\tTotal of " << ss.size << " bytes in " << ss.packets << " packets, " << ss.length
		<< " samples" << " (" << ss.clusters << " clusters used)" << std::endl;
	return ss;
}

int main(int argc, char** argv)
{
	stream_statistics ss;
	if(argc != 2) {
		std::cerr << "Syntax: lsvsdump <filename>" << std::endl;
		return 1;
	}
	filesystem* fs;
	uint64_t stream_count = 0;
	try {
		fs = new filesystem(argv[1]);
	} catch(std::exception& e) {
		std::cerr << "Can't load .lsvs file '" << argv[1] << "': " << e.what() << std::endl;
		return 1;
	}
	try {
		uint32_t maindir_c = 2;
		uint32_t maindir_o = 0;
		uint32_t maindir_uc;
		uint32_t maindir_uo;
		ss.clusters++;
		while(true) {
			maindir_uc = maindir_c;
			maindir_uo = maindir_o;
			char buffer[16];
			size_t r = fs->read_data(maindir_c, maindir_o, buffer, 16);
			if(r < 16)
				break;
			uint64_t stream_ts = serialization::u64b(buffer);
			uint32_t stream_c = serialization::u32b(buffer + 8);
			uint32_t stream_d = serialization::u32b(buffer + 12);
			if(!stream_c)
				continue;
			std::cout << "Found stream (from " << maindir_uc << ":" << maindir_uo << "): ts=" << stream_ts
				<< ", cluster=" << stream_c << "/" << stream_d << std::endl;
			auto ss2 = dump_stream(*fs, stream_c, stream_d);
			stream_count++;
			ss += ss2;
			if(maindir_c != maindir_uc)
				ss.clusters++;
		}
	} catch(std::exception& e) {
		std::cerr << "Error reading .lsvs file '" << argv[1] << "': " << e.what() << std::endl;
		return 1;
	}
	std::cout << "Totals: " << ss.size << " bytes in " << ss.packets << " packets in " << stream_count <<
		" streams, totalling " << ss.length << " samples. " << ss.clusters << " clusters used." << std::endl;
	delete fs;
}
