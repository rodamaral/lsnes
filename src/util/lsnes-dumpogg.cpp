#include "library/ogg.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char** argv)
{
	if(argc != 2) {
		std::cerr << "Filename needed." << std::endl;
		return 1;
	}
	std::ifstream s(argv[1], std::ios_base::binary);
	if(!s) {
		std::cerr << "Can't open '" << argv[1] << "'" << std::endl;
		return 2;
	}
	ogg::stream_reader_iostreams r(s);
	ogg::page p;
	while(r.get_page(p)) {
		std::cout << "Ogg page: Stream " << p.get_stream() << " sequence " << p.get_sequence()
			<< " Flags: " << (p.get_continue() ? "CONTINUE " : "")
			<< (p.get_bos() ? "BOS " : "") << (p.get_eos() ? "EOS " : "")
			<< "granulepos=" << p.get_granulepos() << std::endl;
		size_t pc = p.get_packet_count();
		for(size_t i = 0; i < pc; i++) {
			auto pp = p.get_packet(i);
			std::cout << "Packet #" << i << ": " << pp.second << " bytes";
			if(i == 0 && p.get_continue())
				std::cout << " <continued>";
			if(i + 1 == pc && p.get_last_packet_incomplete())
				std::cout << " <incomplete>";
			std::cout << std::endl;
		}
	}
	std::cout << "End of Ogg stream." << std::endl;
	return 0;
}
