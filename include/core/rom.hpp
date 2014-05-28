#ifndef _rom__hpp__included__
#define _rom__hpp__included__

#define ROM_SLOT_COUNT 27

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include "core/misc.hpp"
#include "interface/romtype.hpp"
#include "library/fileimage.hpp"

//ROM request.
struct rom_request
{
	//List of core types.
	std::vector<core_type*> cores;
	//Selected core (default core on call).
	bool core_guessed;
	size_t selected;
	//Filename selected (on entry, filename hint).
	bool has_slot[ROM_SLOT_COUNT];
	bool guessed[ROM_SLOT_COUNT];
	std::string filename[ROM_SLOT_COUNT];
	std::string hash[ROM_SLOT_COUNT];
	std::string hashxml[ROM_SLOT_COUNT];
	//Canceled flag.
	bool canceled;
};

/**
 * ROM loaded into memory.
 */
struct loaded_rom
{
/**
 * Create blank ROM
 */
	loaded_rom() throw();
/**
 * Take in ROM filename (or a bundle) and load it to memory.
 *
 * parameter file: The file to load
 * parameter tmpprefer: The core name to prefer.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM file failed.
 */
	loaded_rom(const std::string& file, const std::string& tmpprefer = "") throw(std::bad_alloc,
		std::runtime_error);
/**
 * Take a ROM and load it.
 */
	loaded_rom(const std::string& file, const std::string& core, const std::string& type,
		const std::string& region);
/**
 * Load a multi-file ROM.
 */
	loaded_rom(const std::string file[ROM_SLOT_COUNT], const std::string& core, const std::string& type,
		const std::string& region);
/**
 * Take in ROM filename and load it to memory with specified type.
 *
 * parameter file: The file to load
 * parameter ctype: The core type to use.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM file failed.
 */
	loaded_rom(const std::string& file, core_type& ctype) throw(std::bad_alloc, std::runtime_error);
/**
 * ROM type
 */
	core_type* rtype;
/**
 * ROM region (this is the currently active region).
 */
	core_region* region;
/**
 * ROM original region (this is the region ROM is loaded as).
 */
	core_region* orig_region;
/**
 * Loaded main ROM
 */
	fileimage::image romimg[ROM_SLOT_COUNT];
/**
 * Loaded main ROM XML
 */
	fileimage::image romxml[ROM_SLOT_COUNT];
/**
 * MSU-1 base.
 */
	std::string msu1_base;
/**
 * Load filename.
 */
	std::string load_filename;
/**
 * Switches the active cartridge to this cartridge. The compatiblity between selected region and original region
 * is checked. Region is updated after cartridge has been loaded.
 *
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Switching cartridges failed.
 */
	void load(std::map<std::string, std::string>& settings, uint64_t rtc_sec, uint64_t rtc_subsec)
		throw(std::bad_alloc, std::runtime_error);
/**
 * Saves core state into buffer. WARNING: This takes emulated time.
 *
 * returns: The saved state.
 * throws std::bad_alloc: Not enough memory.
 */
	std::vector<char> save_core_state(bool nochecksum = false) throw(std::bad_alloc, std::runtime_error);

/**
 * Loads core state from buffer.
 *
 * parameter buf: The buffer containing the state.
 * throws std::runtime_error: Loading state failed.
 */
	void load_core_state(const std::vector<char>& buf, bool nochecksum = false) throw(std::runtime_error);

/**
 * Is file a gamepak?
 *
 * parameter filename: The file to probe.
 * retruns: True if gamepak, false if not.
 * throws std::runtime_error: No such file.
 */
	static bool is_gamepak(const std::string& filename) throw(std::bad_alloc, std::runtime_error);
};

/**
 * Get major type and region of loaded ROM.
 *
 * returns: Tuple (ROM type, ROM region) of currently loaded ROM.
 */
std::pair<core_type*, core_region*> get_current_rom_info() throw();

/**
 * Read SRAMs from command-line and and load the files.
 *
 * parameter cmdline: Command line
 * returns: The loaded SRAM contents.
 * throws std::bad_alloc: Out of memory.
 * throws std::runtime_error: Failed to load.
 */
std::map<std::string, std::vector<char>> load_sram_commandline(const std::vector<std::string>& cmdline)
	throw(std::bad_alloc, std::runtime_error);

/**
 * Set the hasher callback.
 */
void set_hasher_callback(std::function<void(uint64_t, uint64_t)> cb);

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
void try_guess_roms(rom_request& req);
void record_filehash(const std::string& file, uint64_t prefix, const std::string& hash);
std::string try_to_guess_rom(const std::string& hint, const std::string& hash, const std::string& xhash,
	core_type& type, unsigned i);


//Map of preferred cores for each extension and type.
extern std::map<std::string, core_type*> preferred_core;
//Main hasher
extern fileimage::hash lsnes_image_hasher;


#endif
