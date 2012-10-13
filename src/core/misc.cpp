#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/threaddebug.hpp"
#include "core/window.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <boost/filesystem.hpp>

namespace
{
	std::string rseed;
	uint64_t rcounter = 0;
	bool reached_main_flag;

	std::string get_random_hexstring_64(size_t index)
	{
		std::ostringstream str;
		str << rseed << " " << time(NULL) << " " << (rcounter++) << " " << index;
		std::string s = str.str();
		std::vector<char> x;
		x.resize(s.length());
		std::copy(s.begin(), s.end(), x.begin());
		return sha256::hash(reinterpret_cast<uint8_t*>(&x[0]), x.size());
	}

	std::string collect_identifying_information()
	{
		//TODO: Collect as much identifying information as possible.
		std::ostringstream str;
		time_t told = time(NULL);
		time_t tnew;
		uint64_t loops = 0;
		uint64_t base = 0;
		int cnt = 0;
		while(cnt < 3) {
			tnew = time(NULL);
			if(tnew > told) {
				told = tnew;
				cnt++;
				str << (loops - base) << " ";
				base = loops;
			}
			loops++;
		}
		return str.str();
	}
}

std::string get_random_hexstring(size_t length) throw(std::bad_alloc)
{
	std::string out;
	for(size_t i = 0; i < length; i += 64)
		out = out + get_random_hexstring_64(i);
	return out.substr(0, length);
}

void set_random_seed(const std::string& seed) throw(std::bad_alloc)
{
	std::ostringstream str;
	str << seed.length() << " " << seed;
	rseed = str.str();
}

void set_random_seed() throw(std::bad_alloc)
{
	//Try /dev/urandom first.
	{
		std::ifstream r("/dev/urandom", std::ios::binary);
		if(r.is_open()) {
			char buf[64];
			r.read(buf, 64);
			std::string s(buf, 64);
			set_random_seed(s);
			return;
		}
	}
	//Fall back to time.
	std::ostringstream str;
	str << collect_identifying_information() << " " << time(NULL);
	set_random_seed(str.str());
}


struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline) throw(std::bad_alloc,
	std::runtime_error)
{
	std::string f;
	regex_results optp;
	for(auto i : cmdline) {
		if(!(optp = regex("--rom=(.+)", i)))
			continue;
		f = optp[1];
	}

	struct loaded_rom r;
	try {
		r = loaded_rom(f);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		throw std::runtime_error(std::string("Can't load ROM: ") + e.what());
	}

	std::string not_present = "N/A";
	for(size_t i = 0; i < sizeof(r.romimg)/sizeof(r.romimg[0]); i++) {
		std::string romname = "UNKNOWN ROM";
		std::string xmlname = "UNKNOWN XML";
		if(i < r.rtype->get_image_count()) {
			romname = (r.rtype->get_image_info(i).hname == "ROM") ? std::string("ROM") :
				(r.rtype->get_image_info(i).hname + " ROM");
			xmlname = r.rtype->get_image_info(i).hname + " XML";
		}
		if(r.romimg[i].valid)	messages << romname << " hash: " << r.romimg[i].sha256 << std::endl;
		if(r.romxml[i].valid)	messages << xmlname << " hash: " << r.romxml[i].sha256 << std::endl;
	}
	return r;
}

void dump_region_map() throw(std::bad_alloc)
{
	std::vector<struct memory_region> regions = get_regions();
	for(auto i : regions) {
		std::ostringstream x;
		x << std::setfill('0') << std::setw(16) << std::hex << i.baseaddr << "-";
		x << std::setfill('0') << std::setw(16) << std::hex << i.lastaddr << " ";
		x << std::setfill('0') << std::setw(16) << std::hex << i.size << " ";
		messages << x.str() << (i.readonly ? "R-" : "RW") << (i.native_endian ? 'N' : 'L')
			<< (i.iospace ? 'I' : 'M') << " " << i.region_name << std::endl;
	}
}

void fatal_error() throw()
{
	platform::fatal_error();
	std::cout << "PANIC: Fatal error, can't continue." << std::endl;
	exit(1);
}

std::string get_config_path() throw(std::bad_alloc)
{
	const char* tmp;
	std::string basedir;
	if((tmp = getenv("APPDATA"))) {
		//If $APPDATA exists, it is the base directory
		basedir = tmp;
	} else if((tmp = getenv("XDG_CONFIG_HOME"))) {
		//If $XDG_CONFIG_HOME exists, it is the base directory
		basedir = tmp;
	} else if((tmp = getenv("HOME"))) {
		//If $HOME exists, the base directory is '.config' there.
		basedir = std::string(tmp) + "/.config";
	} else {
		//Last chance: Return current directory.
		return ".";
	}
	//Try to create 'lsnes'. If it exists (or is created) and is directory, great. Otherwise error out.
	std::string lsnes_path = basedir + "/lsnes";
	boost::filesystem::path p(lsnes_path);
	if(!boost::filesystem::create_directories(p) && !boost::filesystem::is_directory(p)) {
		messages << "FATAL: Can't create configuration directory '" << lsnes_path << "'" << std::endl;
		fatal_error();
	}
	//Yes, this is racy, but portability is more important than being absolutely correct...
	std::string tfile = lsnes_path + "/test";
	remove(tfile.c_str());
	FILE* x;
	if(!(x = fopen(tfile.c_str(), "w+"))) {
		messages << "FATAL: Configuration directory '" << lsnes_path << "' is not writable" << std::endl;
		fatal_error();
	}
	fclose(x);
	remove(tfile.c_str());
	return lsnes_path;
}

extern const char* lsnesrc_file;

void create_lsnesrc()
{
	std::string rcfile = get_config_path() + "/lsnes.rc";
	std::ifstream x(rcfile.c_str());
	if(x) {
		x.close();
		return;
	}
	std::ofstream y(rcfile.c_str());
	if(!y) {
		messages << "FATAL: lsnes.rc (" << rcfile << ") doesn't exist nor it can be created" << std::endl;
		fatal_error();
	}
	y.write(lsnesrc_file, strlen(lsnesrc_file));
	y.close();
}


void OOM_panic()
{
	messages << "FATAL: Out of memory!" << std::endl;
	fatal_error();
}

std::ostream& _messages()
{
	return platform::out();
}

uint32_t gcd(uint32_t a, uint32_t b) throw()
{
	if(b == 0)
		return a;
	else
		return gcd(b, a % b);
}

std::string format_address(void* addr)
{
	unsigned long x = (unsigned long)addr;
	std::ostringstream y;
	y << "0x" << std::hex << std::setfill('0') << std::setw(2 * sizeof(unsigned long)) << x;
	return y.str();
}

bool in_global_ctors()
{
	return !reached_main_flag;
}

void reached_main()
{
	reached_main_flag = true;
	init_threaded_malloc();
	lsnes_cmd.set_oom_panic(OOM_panic);
	lsnes_cmd.set_output(_messages());
}

std::string bsnes_core_version;
