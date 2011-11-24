#include "core/moviefile.hpp"
#include "core/rrdata.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

std::string name_romtype(rom_type r)
{
	if(r == ROMTYPE_SNES)		return "SNES";
	if(r == ROMTYPE_BSX)		return "BS-X (non-slotted)";
	if(r == ROMTYPE_BSXSLOTTED)	return "BS-X (slotted)";
	if(r == ROMTYPE_SUFAMITURBO)	return "Sufami turbo";
	if(r == ROMTYPE_SGB)		return "SGB";
	return "Unknown";
}

std::string name_region(rom_region r)
{
	if(r == REGION_PAL)		return "PAL";
	if(r == REGION_NTSC)		return "NTSC";
	return "Unknown";
}

std::string name_porttype(porttype_t r)
{
	if(r == PT_NONE)		return "No device";
	if(r == PT_GAMEPAD)		return "Gamepad";
	if(r == PT_MULTITAP)		return "Multitap";
	if(r == PT_MOUSE)		return "Mouse";
	if(r == PT_SUPERSCOPE)		return "Super Scope";
	if(r == PT_JUSTIFIER)		return "Justifier";
	if(r == PT_JUSTIFIERS)		return "2 Justifiers";
	return "Unknown";
}

std::string escape_string(std::string x)
{
	std::ostringstream out;
	for(size_t i = 0; i < x.length(); i++)
		if((x[i] < 0 || x[i] > 31) && x[i] != '\\')
			out << x[i];
		else {
			unsigned char y = static_cast<unsigned char>(x[i]);
			out << "\\" << ('0' + static_cast<char>((y >> 6) & 7))
				<< ('0' + static_cast<char>((y >> 3) & 7))
				<< ('0' + static_cast<char>(y & 7));
		}
	return out.str();
}

int main(int argc, char** argv)
{
	if(argc != 2) {
		std::cerr << "Syntax: " << argv[0] << " <moviefile>" << std::endl;
		return 1;
	}
	try {
		uint64_t starting_point = 0;
		struct moviefile m(argv[1]);
		rom_type rtype = gtype::toromtype(m.gametype);
		rom_region reg = gtype::toromregion(m.gametype);
		std::cout << "Console: " << name_romtype(rtype) << std::endl;
		std::cout << "Region: " << name_region(reg) << std::endl;
		std::cout << "Port #1: " << name_porttype(m.port1) << std::endl;
		std::cout << "Port #2: " << name_porttype(m.port1) << std::endl;
		std::cout << "Used emulator core: " << escape_string(m.coreversion) << std::endl;
		if(m.gamename != "")
			std::cout << "Game name: " << escape_string(m.gamename) << std::endl;
		else
			std::cout << "No game name available" << std::endl;
		std::cout << "Project ID: " << escape_string(m.projectid) << std::endl;
		if(m.rom_sha256 != "") {
			std::cout << "ROM checksum: " << escape_string(m.rom_sha256) << std::endl;
			if(m.romxml_sha256 != "")
				std::cout << "ROM XML checksum: " << escape_string(m.romxml_sha256) << std::endl;
			else
				std::cout << "Using default settings for ROM" << std::endl;
		} else
			std::cout << "No main ROM present" << std::endl;
		if(m.slota_sha256 != "") {
			std::cout << "BS/ST-A/DMG checksum: " << escape_string(m.slota_sha256) << std::endl;
			if(m.slotaxml_sha256 != "")
				std::cout << "BS/ST-A/DMG XML checksum: " << escape_string(m.slotaxml_sha256)
					<< std::endl;
			else
				std::cout << "Using default settings for BS/ST-A/DMG" << std::endl;
		} else
			std::cout << "No BS/ST-A/DMG present" << std::endl;
		if(m.slotb_sha256 != "") {
			std::cout << "ST-B checksum: " << escape_string(m.slotb_sha256) << std::endl;
			if(m.slotbxml_sha256 != "")
				std::cout << "ST-B XML checksum: " << escape_string(m.slotbxml_sha256)
					<< std::endl;
			else
				std::cout << "Using default settings for BS/ST-A/DMG" << std::endl;
		} else
			std::cout << "No ST-B present" << std::endl;
		for(auto i = m.authors.begin(); i != m.authors.end(); i++) {
			if(i->first != "" && i->second != "")
				std::cout << "Author: " << escape_string(i->first) << " (" << escape_string(i->second)
					<< ")" << std::endl;
			else if(i->first != "" && i->second == "")
				std::cout << "Author: " << escape_string(i->first) << std::endl;
			else if(i->first == "" && i->second != "")
				std::cout << "Author: (" << escape_string(i->second) << ")" << std::endl;
		}
		if(m.authors.size() == 0)
			std::cout << "No author info available" << std::endl;
		if(m.is_savestate) {
			std::cout << "Movie starts from savestate" << std::endl;
			for(auto i = m.movie_sram.begin(); i != m.movie_sram.end(); i++)
				std::cout << "Start SRAM: " << escape_string(i->first) << ": [" << i->second.size()
					<< " bytes of binary data]" << std::endl;
			if(!m.movie_sram.size())
				std::cout << "No starting SRAM" << std::endl;
			for(auto i = m.sram.begin(); i != m.sram.end(); i++)
				std::cout << "Saved SRAM: " << escape_string(i->first) << ": [" << i->second.size()
					<< " bytes of binary data]" << std::endl;
			if(!m.sram.size())
				std::cout << "No saved SRAM" << std::endl;
			std::cout << "Savestate: [" << m.savestate.size() << " bytes of binary data]" << std::endl;
			std::cout << "Host memory: [" << m.host_memory.size() << " bytes of binary data]"
				<< std::endl;
			std::cout << "Screenshot: [" << m.screenshot.size() << " bytes of binary data]" << std::endl;
			if(m.screenshot.size() >= 4) {
				uint16_t a = static_cast<uint8_t>(m.screenshot[0]);
				uint16_t b = static_cast<uint8_t>(m.screenshot[1]);
				std::cout << "Screenshot size: " << (a * 256 + b) << "*" << (m.screenshot.size() - 2)
					/ (a * 256  + b) / 3 << std::endl;
			} else
				std::cout << "Screenshot is corrupt" << std::endl;
			starting_point = m.save_frame;
			std::cout << "Starting frame: " << starting_point << std::endl;
			std::cout << "Lag frames so far: " << m.lagged_frames << std::endl;
		} else if(m.movie_sram.size() > 0) {
			std::cout << "Movie starts from SRAM" << std::endl;
			for(auto i = m.movie_sram.begin(); i != m.movie_sram.end(); i++)
				std::cout << "Start SRAM: " << escape_string(i->first) << " (" << i->second.size()
					<< " bytes)" << std::endl;
		} else
			std::cout << "Movie starts from clean state" << std::endl;
		if(m.get_frame_count() > starting_point)
			std::cout << "Movie frame count: " << (m.get_frame_count() - starting_point) << std::endl;
		else
			std::cout << "Movie frame count: 0" << std::endl;
		uint64_t length = m.get_movie_length(starting_point);
		{
			std::ostringstream x;
			if(length >= 3600000000000ULL) {
				x << (length / 3600000000000ULL) << ":";
				length %= 3600000000000ULL;
			}
			x << std::setw(2) << std::setfill('0') << (length / 60000000000ULL) << ":";
			length %= 60000000000ULL;
			x << std::setw(2) << std::setfill('0') << (length / 1000000000ULL) << ".";
			length %= 1000000000ULL;
			x << std::setw(3) << std::setfill('0') << (length + 500000) / 1000000;
			std::cout << "Movie length: " << x.str() << std::endl;
		}
		std::cout << "Rerecord count: " << rrdata::count(m.c_rrdata) << std::endl;
	} catch(std::exception& e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}