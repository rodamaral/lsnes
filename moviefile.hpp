#ifndef _moviefile__hpp__included__
#define _moviefile__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include <map>
#include "controllerdata.hpp"
#include "rom.hpp"


/**
 * \brief Parsed representation of movie file
 *
 * This structure gives parsed representationg of movie file, as result of decoding or for encoding.
 */
struct moviefile
{
/**
 * \brief Construct empty movie
 *
 * This constructor construct movie structure with default settings.
 *
 * \throws std::bad_alloc Not enough memory.
 */
	moviefile() throw(std::bad_alloc);

/**
 * \brief Load a movie/savestate file
 *
 * This constructor loads a movie/savestate file and fills structure accordingly.
 *
 * \param filename The file to load.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Can't load the movie file
 */
	moviefile(const std::string& filename) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Save a movie or savestate.
 *
 * Reads this movie structure and saves it into file.
 *
 * \param filename The file to save to.
 * \param compression The compression level 0-9. 0 is uncompressed.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Can't save the movie file.
 */
	void save(const std::string& filename, unsigned compression) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief What is the ROM type and region?
 */
	gametype_t gametype;
/**
 * \brief What's in port #1?
 */
	porttype_t port1;
/**
 * \brief What's in port #2?
 */
	porttype_t port2;
/**
 * \brief Emulator Core version string.
 */
	std::string coreversion;
/**
 * \brief Name of the game
 */
	std::string gamename;
/**
 * \brief Project ID (used to identify if two movies are from the same project).
 */
	std::string projectid;
/**
 * \brief Rerecord count (only loaded).
 */
	std::string rerecords;
/**
 * \brief SHA-256 of main ROM (empty string if none).
 */
	std::string rom_sha256;			//SHA-256 of main ROM.
/**
 * \brief SHA-256 of main ROM XML (empty string if none).
 */
	std::string romxml_sha256;		//SHA-256 of main ROM XML.
/**
 * \brief SHA-256 of slot A ROM (empty string if none).
 */
	std::string slota_sha256;		//SHA-256 of SLOT A ROM.
/**
 * \brief SHA-256 of slot A XML (empty string if none).
 */
	std::string slotaxml_sha256;		//SHA-256 of SLOT A XML.
/**
 * \brief SHA-256 of slot B ROM (empty string if none).
 */
	std::string slotb_sha256;		//SHA-256 of SLOT B ROM.
/**
 * \brief SHA-256 of slot B XML (empty string if none).
 */
	std::string slotbxml_sha256;		//SHA-256 of SLOT B XML.
/**
 * \brief Authors of the run, first in each pair is full name, second is nickname.
 */
	std::vector<std::pair<std::string, std::string>> authors;
/**
 * \brief Contents of SRAM on time of initial powerup.
 */
	std::map<std::string, std::vector<char>> movie_sram;
/**
 * \brief True if savestate, false if movie.
 */
	bool is_savestate;
/**
 * \brief Contents of SRAM on time of savestate (if is_savestate is true).
 */
	std::map<std::string, std::vector<char>> sram;
/**
 * \brief Core savestate (if is_savestate is true).
 */
	std::vector<char> savestate;		//Savestate to load (if is_savestate is true).
/**
 * \brief Host memory (if is_savestate is true).
 */
	std::vector<char> host_memory;
/**
 * \brief Screenshot (if is_savestate is true).
 */
	std::vector<char> screenshot;
/**
 * \brief State of movie code (if is_savestate is true).
 */
	std::vector<char> movie_state;
/**
 * \brief Input for each (sub)frame.
 */
	std::vector<controls_t> input;		//Input for each frame.

/**
 * \brief Get number of frames in movie.
 *
 * \return Number of frames.
 */
	uint64_t get_frame_count() throw();

/**
 * \brief Get length of the movie
 *
 * \return Length of the movie in nanoseconds.
 */
	uint64_t get_movie_length() throw();
};

#endif
