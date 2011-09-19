#ifndef _moviefile__hpp__included__
#define _moviefile__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include <map>
#include "controllerdata.hpp"
#include "rom.hpp"


/**
 * This structure gives parsed representationg of movie file, as result of decoding or for encoding.
 */
struct moviefile
{
/**
 * This constructor construct movie structure with default settings.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	moviefile() throw(std::bad_alloc);

/**
 * This constructor loads a movie/savestate file and fills structure accordingly.
 *
 * parameter filename: The file to load.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the movie file
 */
	moviefile(const std::string& filename) throw(std::bad_alloc, std::runtime_error);

/**
 * Reads this movie structure and saves it into file.
 *
 * parameter filename: The file to save to.
 * parameter compression: The compression level 0-9. 0 is uncompressed.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't save the movie file.
 */
	void save(const std::string& filename, unsigned compression) throw(std::bad_alloc, std::runtime_error);

/**
 * Force loading as corrupt.
 */
	bool force_corrupt;
/**
 * What is the ROM type and region?
 */
	gametype_t gametype;
/**
 * What's in port #1?
 */
	porttype_t port1;
/**
 * What's in port #2?
 */
	porttype_t port2;
/**
 * Emulator Core version string.
 */
	std::string coreversion;
/**
 * Name of the game
 */
	std::string gamename;
/**
 * Project ID (used to identify if two movies are from the same project).
 */
	std::string projectid;
/**
 * Rerecord count (only saved).
 */
	std::string rerecords;
/**
 * SHA-256 of main ROM (empty string if none).
 */
	std::string rom_sha256;			//SHA-256 of main ROM.
/**
 * SHA-256 of main ROM XML (empty string if none).
 */
	std::string romxml_sha256;		//SHA-256 of main ROM XML.
/**
 * SHA-256 of slot A ROM (empty string if none).
 */
	std::string slota_sha256;		//SHA-256 of SLOT A ROM.
/**
 * SHA-256 of slot A XML (empty string if none).
 */
	std::string slotaxml_sha256;		//SHA-256 of SLOT A XML.
/**
 * SHA-256 of slot B ROM (empty string if none).
 */
	std::string slotb_sha256;		//SHA-256 of SLOT B ROM.
/**
 * SHA-256 of slot B XML (empty string if none).
 */
	std::string slotbxml_sha256;		//SHA-256 of SLOT B XML.
/**
 * Authors of the run, first in each pair is full name, second is nickname.
 */
	std::vector<std::pair<std::string, std::string>> authors;
/**
 * Contents of SRAM on time of initial powerup.
 */
	std::map<std::string, std::vector<char>> movie_sram;
/**
 * True if savestate, false if movie.
 */
	bool is_savestate;
/**
 * Contents of SRAM on time of savestate (if is_savestate is true).
 */
	std::map<std::string, std::vector<char>> sram;
/**
 * Core savestate (if is_savestate is true).
 */
	std::vector<char> savestate;		//Savestate to load (if is_savestate is true).
/**
 * Host memory (if is_savestate is true).
 */
	std::vector<char> host_memory;
/**
 * Screenshot (if is_savestate is true).
 */
	std::vector<char> screenshot;
/**
 * State of movie code (if is_savestate is true).
 */
	std::vector<char> movie_state;
/**
 * Compressed rrdata.
 */
	std::vector<char> c_rrdata;
/**
 * Input for each (sub)frame.
 */
	std::vector<controls_t> input;		//Input for each frame.

/**
 * Get number of frames in movie.
 *
 * returns: Number of frames.
 */
	uint64_t get_frame_count() throw();

/**
 * Get length of the movie
 *
 * parameter framebias: Number of frames to subtract.
 * returns: Length of the movie in nanoseconds.
 */
	uint64_t get_movie_length(uint64_t framebias = 0) throw();
};

#endif
