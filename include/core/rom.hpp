#ifndef _rom__hpp__included__
#define _rom__hpp__included__

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include "core/misc.hpp"
#include "interface/romtype.hpp"

/**
 * Some loaded data or indication of no data.
 */
struct loaded_slot
{
/**
 * Construct empty slot.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	loaded_slot() throw(std::bad_alloc);

/**
 * This constructor construct slot by reading data from file. If filename is "", constructs an empty slot.
 *
 * parameter filename: The filename to read. If "", empty slot is constructed.
 * parameter base: Base filename to interpret the filename against. If "", no base filename is used.
 * parameter imginfo: Image information.
 * parameter xml_flag: If set, always keep trailing NUL.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the data.
 */
	loaded_slot(const std::string& filename, const std::string& base, const struct core_romimage_info& imginfo,
		bool xml_flag = false) throw(std::bad_alloc, std::runtime_error);

/**
 * This method patches this slot using specified IPS patch.
 *
 * parameter patch: The patch to apply
 * parameter offset: The amount to add to the offsets in the IPS file. Parts with offsets below zero are not patched.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Bad IPS patch.
 */
	void patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error);
/**
 * Is this filename?
 */
	bool filename_flag;
/**
 * Is this slot XML slot?
 */
	bool xml;
/**
 * If this slot is blank, this is set to false, data is undefined and sha256 is "". Otherwise this is set to true,
 * data to apporiate data, and sha256 to hash of data.
 */
	bool valid;
/**
 * The actual data for this slot.
 */
	std::vector<char> data;
/**
 * SHA-256 for the data in this slot if data is valid. If no valid data, this field is "".
 */
	std::string sha_256;
/**
 * Get pointer to loaded data
 *
 * returns: Pointer to loaded data, or NULL if slot is blank.
 */
	operator const char*() const throw()
	{
		return valid ? reinterpret_cast<const char*>(&data[0]) : NULL;
	}
/**
 * Get pointer to loaded data
 *
 * returns: Pointer to loaded data, or NULL if slot is blank.
 */
	operator const uint8_t*() const throw()
	{
		return valid ? reinterpret_cast<const uint8_t*>(&data[0]) : NULL;
	}
/**
 * Get size of slot
 *
 * returns: The number of bytes in slot, or 0 if slot is blank.
 */
	operator unsigned() const throw()
	{
		return valid ? data.size() : 0;
	}
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
	loaded_slot romimg[27];
/**
 * Loaded main ROM XML
 */
	loaded_slot romxml[27];
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
	std::vector<char> save_core_state(bool nochecksum = false) throw(std::bad_alloc);

/**
 * Loads core state from buffer.
 *
 * parameter buf: The buffer containing the state.
 * throws std::runtime_error: Loading state failed.
 */
	void load_core_state(const std::vector<char>& buf, bool nochecksum = false) throw(std::runtime_error);
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

//Map of preferred cores for each extension and type.
extern std::map<std::string, core_type*> preferred_core;
//Preferred overall core.
extern std::string preferred_core_default;
//Currently active ROM.
extern std::string current_romfile;

#endif
