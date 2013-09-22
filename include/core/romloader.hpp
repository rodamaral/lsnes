#ifndef _romloader__hpp__included__
#define _romloader__hpp__included__

struct romload_request
{
	//Pack file to load. Overrides everything else.
	std::string packfile;
	//Single file to load to default slot.
	std::string singlefile;
	//Core and system. May be blank.
	std::string core;
	std::string system;
	std::string region;
	//Files to load.
	std::string files[ROM_SLOT_COUNT];
};

bool load_null_rom();
bool _load_new_rom(const romload_request& req);
bool reload_active_rom();
regex_results get_argument(const std::vector<std::string>& cmdline, const std::string& regexp);
std::string get_requested_core(const std::vector<std::string>& cmdline);
loaded_rom construct_rom(const std::string& movie_filename, const std::vector<std::string>& cmdline);

#endif
