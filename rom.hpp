#ifndef _rom__hpp__included__
#define _rom__hpp__included__

#include <string>
#include "window.hpp"
#include <map>
#include <vector>
#include <stdexcept>
#include "misc.hpp"

/**
 * \brief Region of ROM.
 */
enum rom_region
{
/**
 * \brief Autodetect region
 */
	REGION_AUTO = 0,
/**
 * \brief (force) PAL region
 */
	REGION_PAL,
/**
 * \brief (force) NTSC region
 */
	REGION_NTSC
};

/**
 * \brief Major type of ROM
 */
enum rom_type
{
/**
 * \brief Ordinary SNES ROM
 */
	ROMTYPE_SNES,				//ROM is Ordinary SNES ROM.
/**
 * \brief BS-X Slotted ROM.
 */
	ROMTYPE_BSXSLOTTED,
/**
 * \brief BS-X (non-slotted) ROM.
 */
	ROMTYPE_BSX,
/**
 * \brief Sufami Turbo ROM.
 */
	ROMTYPE_SUFAMITURBO,
/**
 * \brief Super Game Boy ROM.
 */
	ROMTYPE_SGB,
/**
 * \brief No ROM.
 */
	ROMTYPE_NONE
};

/**
 * \brief Type of ROM and region
 *
 * This enumeration enumerates possible ROM types and regions for those.
 */
enum gametype_t
{
/**
 * \brief NTSC-region SNES game
 */
	GT_SNES_NTSC = 0,
/**
 * \brief PAL-region SNES game
 */
	GT_SNES_PAL = 1,
/**
 * \brief NTSC-region BSX slotted game
 */
	GT_BSX_SLOTTED = 2,
/**
 * \brief NTSC-region BSX (non-slotted) game
 */
	GT_BSX = 3,
/**
 * \brief NTSC-region sufami turbo game
 */
	GT_SUFAMITURBO = 4,
/**
 * \brief NTSC-region Super Game Boy game
 */
	GT_SGB_NTSC = 5,
/**
 * \brief PAL-region Super Game Boy game
 */
	GT_SGB_PAL = 6,
/**
 * \brief Invalid game type
 */
	GT_INVALID = 7
};

/**
 * \brief Translations between diffrent representations of type.
 */
class gtype
{
public:
	static std::string tostring(rom_type rtype, rom_region region) throw(std::bad_alloc, std::runtime_error);
	static std::string tostring(gametype_t gametype) throw(std::bad_alloc, std::runtime_error);
	static gametype_t togametype(rom_type rtype, rom_region region) throw(std::bad_alloc, std::runtime_error);
	static gametype_t togametype(const std::string& gametype) throw(std::bad_alloc, std::runtime_error);
	static rom_type toromtype(const std::string& gametype) throw(std::bad_alloc, std::runtime_error);
	static rom_type toromtype(gametype_t gametype) throw();
	static rom_region toromregion(const std::string& gametype) throw(std::bad_alloc, std::runtime_error);
	static rom_region toromregion(gametype_t gametype) throw();
};

/**
 * \brief Filenames associated with ROM.
 *
 * This structure gives all files associated with given ROM image.
 */
struct rom_files
{
/**
 * \brief Construct defaults
 */
	rom_files() throw();

/**
 * \brief Read files from command line arguments.
 *
 * Reads the filenames out of command line arguments given. Also supports bundle files.
 *
 * \param cmdline The commmand line
 * \throws std::bad_alloc Not enough memory
 * \throws std::runtime_error Failed to load ROM filenames.
 */
	rom_files(const std::vector<std::string>& cmdline, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Resolve relative references.
 */
	void resolve_relative() throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Base ROM image
 *
 * The file to look other ROM files relative to. May be blank.
 */
	std::string base_file;
/**
 * \brief Major ROM type.
 */
	enum rom_type rtype;
/**
 * \brief Game region
 */
	enum rom_region region;
/**
 * \brief Relative filename of main ROM file.
 */
	std::string rom;
/**
 * \brief Relative filename of main ROM XML file.
 */
	std::string rom_xml;
/**
 * \brief Relative filename of slot A ROM file (non-SNES only).
 */
	std::string slota;
/**
 * \brief Relative filename of slot A XML file (non-SNES only).
 */
	std::string slota_xml;
/**
 * \brief Relative filename of slot B ROM file (Sufami Turbo only).
 */
	std::string slotb;
/**
 * \brief Relative filename of slot B XML file (Sufami Turbo only).
 */
	std::string slotb_xml;
};

/**
 * \brief Loaded data
 *
 * Some loaded data or indication of no data.
 */
struct loaded_slot
{
/**
 * \brief Construct empty slot.
 * \throws std::bad_alloc Not enough memory.
 */
	loaded_slot() throw(std::bad_alloc);

/**
 * \brief Read a slot
 *
 * This constructor construct slot by reading data from file. If filename is "", constructs an empty slot.
 *
 * \param filename The filename to read. If "", empty slot is constructed.
 * \param base Base filename to interpret the filename against. If "", no base filename is used.
 * \param xml_flag If set, always keep trailing NUL.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Can't load the data.
 */
	loaded_slot(const std::string& filename, const std::string& base, bool xml_flag = false) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief IPS-patch a slot
 *
 * This method patches this slot using specified IPS patch.
 *
 * \param patch The patch to apply
 * \param offset The amount to add to the offsets in the IPS file. Parts with offsets below zero are not patched.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Bad IPS patch.
 */
	void patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Is this slot XML slot?
 */
	bool xml;
/**
 * \brief True if this slot has valid data
 *
 * If this slot is blank, this is set to false, data is undefined and sha256 is "". Otherwise this is set to true,
 * data to apporiate data, and sha256 to hash of data.
 */
	bool valid;
/**
 * \brief The actual data for this slot.
 */
	std::vector<char> data;
/**
 * \brief SHA-256 for the data in this slot.
 *
 * SHA-256 for the data in this slot if data is valid. If no valid data, this field is "".
 */
	std::string sha256;
/**
 * \brief Get pointer to loaded data
 * \return Pointer to loaded data, or NULL if slot is blank.
 */
	operator const char*() const throw()
	{
		return valid ? reinterpret_cast<const char*>(&data[0]) : NULL;
	}
/**
 * \brief Get pointer to loaded data
 * \return Pointer to loaded data, or NULL if slot is blank.
 */
	operator const uint8_t*() const throw()
	{
		return valid ? reinterpret_cast<const uint8_t*>(&data[0]) : NULL;
	}
/**
 * \brief Get size of slot
 * \return The number of bytes in slot, or 0 if slot is blank.
 */
	operator unsigned() const throw()
	{
		return valid ? data.size() : 0;
	}
};

/**
 * \brief ROM loaded into memory.
 */
struct loaded_rom
{
/**
 * \brief Create blank ROM
 */
	loaded_rom() throw();
/**
 * \brief Load specified ROM files into memory.
 *
 * Takes in collection of ROM filenames and loads them into memory.
 *
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Loading ROM files failed.
 */
	loaded_rom(const rom_files& files, window* win) throw(std::bad_alloc, std::runtime_error);
/**
 * \brief ROM type
 */
	enum rom_type rtype;
/**
 * \brief ROM region
 */
	enum rom_region region;
/**
 * \brief ROM original region
 */
	enum rom_region orig_region;
/**
 * \brief Loaded main ROM
 */
	loaded_slot rom;
/**
 * \brief Loaded main ROM XML
 */
	loaded_slot rom_xml;
/**
 * \brief Loaded slot A ROM
 */
	loaded_slot slota;
/**
 * \brief Loaded slot A XML
 */
	loaded_slot slota_xml;
/**
 * \brief Loaded slot B ROM
 */
	loaded_slot slotb;
/**
 * \brief Loaded slot B XML
 */
	loaded_slot slotb_xml;

/**
 * \brief Patch the ROM.
 */
	void do_patch(const std::vector<std::string>& cmdline, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Load this ROM into "SNES".
 *
 * Switches the active cartridge to this cartridge. The compatiblity between selected region and original region
 * is checked. Region is updated after cartridge has been loaded.
 *
 * \throw std::bad_alloc Not enough memory
 * \throw std::runtime_error Switching cartridges failed.
 */
	void load() throw(std::bad_alloc, std::runtime_error);
};

int recognize_commandline_rom(enum rom_type major, const std::string& romname) throw(std::bad_alloc);
rom_type recognize_platform(unsigned long flags) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Name a sub-ROM.
 */
std::string name_subrom(enum rom_type major, unsigned romnumber) throw(std::bad_alloc);

/**
 * \brief Get major type and region of loaded ROM.
 * \return rom type and region of current ROM.
 */
std::pair<enum rom_type, enum rom_region> get_current_rom_info() throw();

/**
 * \brief Save all SRAMs
 *
 * Take current values of all SRAMs in current system and save their contents.
 *
 * \return Saved SRAM contents.
 * \throws std::bad_alloc Out of memory.
 */
std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc);

/**
 * \brief Load all SRAMs
 *
 * Write contents of saved SRAMs into current system SRAMs.
 *
 * \param sram Saved SRAM contents.
 * \throws std::bad_alloc Out of memory.
 */
void load_sram(std::map<std::string, std::vector<char>>& sram, window* win) throw(std::bad_alloc);

/**
 * \brief Load SRAMs specified on command-line
 *
 * Read SRAMs from command-line and and load the files.
 *
 * \param cmdline Command line
 * \return The loaded SRAM contents.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Failed to load.
 */
std::map<std::string, std::vector<char>> load_sram_commandline(const std::vector<std::string>& cmdline)
	throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Emulate a frame
 */
void emulate_frame() throw();

/**
 * \brief Reset the SNES
 */
void reset_snes() throw();

/**
 * \brief Save core state into buffer
 *
 * Saves core state into buffer. WARNING: This takes emulated time.
 *
 * \return The saved state.
 * \throws std::bad_alloc Not enough memory.
 */
std::vector<char> save_core_state() throw(std::bad_alloc);

/**
 * \brief Restore core state from buffer.
 *
 * Loads core state from buffer.
 *
 * \param buf The buffer containing the state.
 * \throws std::runtime_error Loading state failed.
 */
void load_core_state(const std::vector<char>& buf) throw(std::runtime_error);

/**
 * \brief Read index file.
 *
 * Read index of ROMs and add ROMs found to content-searchable storage.
 *
 * \param filename The filename of index file.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Loading index failed.
 */
void load_index_file(const std::string& filename) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Lookup absolute filename by hash.
 *
 * Search all indices, looking for file with specified SHA-256 (specifying hash of "" results "").
 *
 * \param hash The hash of file.
 * \return Absolute filename.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Not found.
 */
std::string lookup_file_by_sha256(const std::string& hash) throw(std::bad_alloc, std::runtime_error);

#endif
