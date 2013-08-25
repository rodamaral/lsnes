#ifndef _moviefile__hpp__included__
#define _moviefile__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include <map>
#include "core/controllerframe.hpp"
#include "core/rom.hpp"
#include "core/subtitles.hpp"


/**
 * This structure gives parsed representationg of movie file, as result of decoding or for encoding.
 */
struct moviefile
{
/**
 * Brief information
 */
	struct brief_info
	{
		brief_info(const std::string& filename);
		std::string sysregion;
		std::string corename;
		std::string projectid;
		uint64_t current_frame;
		uint64_t rerecords;
	private:
		void binary_io(std::istream& s);
	};
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
 * parameter romtype: Type of ROM.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the movie file
 */
	moviefile(const std::string& filename, core_type& romtype) throw(std::bad_alloc, std::runtime_error);

/**
 * Reads this movie structure and saves it into file.
 *
 * parameter filename: The file to save to.
 * parameter compression: The compression level 0-9. 0 is uncompressed.
 * parameter binary: Save in binary form if true.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't save the movie file.
 */
	void save(const std::string& filename, unsigned compression, bool binary) throw(std::bad_alloc,
		std::runtime_error);

/**
 * Force loading as corrupt.
 */
	bool force_corrupt;
/**
 * What is the ROM type and region?
 */
	core_sysregion* gametype;
/**
 * Settings.
 */
	std::map<std::string, std::string> settings;
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
 * SHA-256 of ROM (empty string if none).
 */
	std::string romimg_sha256[27];
/**
 * SHA-256 of ROM XML (empty string if none).
 */
	std::string romxml_sha256[27];
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
 * Anchoring core savestate (if not empty).
 */
	std::vector<char> anchor_savestate;
/**
 * Host memory (if is_savestate is true).
 */
	std::vector<char> host_memory;
/**
 * Screenshot (if is_savestate is true).
 */
	std::vector<char> screenshot;
/**
 * Current frame (if is_savestate is true).
 */
	uint64_t save_frame;
/**
 * Number of lagged frames (if is_savestate is true).
 */
	uint64_t lagged_frames;
/**
 * Poll counters (if is_savestate is true).
 */
	std::vector<uint32_t> pollcounters;
/**
 * Poll flag.
 */
	unsigned poll_flag;
/**
 * Compressed rrdata.
 */
	std::vector<char> c_rrdata;
/**
 * Input for each (sub)frame.
 */
	controller_frame_vector input;		//Input for each frame.
/**
 * Current RTC second.
 */
	int64_t rtc_second;
/**
 * Current RTC subsecond.
 */
	int64_t rtc_subsecond;
/**
 * Movie starting RTC second.
 */
	int64_t movie_rtc_second;
/**
 * Movie starting RTC subsecond.
 */
	int64_t movie_rtc_subsecond;
/**
 * Start paused flag.
 */
	bool start_paused;
/**
 * Lazy project create flag.
 */
	bool lazy_project_create;
/**
 * Subtitles.
 */
	std::map<moviefile_subtiming, std::string> subtitles;
/**
 * Active macros at savestate.
 */
	std::map<std::string, uint64_t> active_macros;
/**
 * Get number of frames in movie.
 *
 * returns: Number of frames.
 */
	uint64_t get_frame_count() throw();
/**
 * Get length of the movie
 *
 * returns: Length of the movie in nanoseconds.
 */
	uint64_t get_movie_length() throw();
private:
	void binary_io(std::ostream& stream) throw(std::bad_alloc, std::runtime_error);
	void binary_io(std::istream& stream, struct core_type& romtype) throw(std::bad_alloc, std::runtime_error);
};

#endif
