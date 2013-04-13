#ifndef _moviedata__hpp__included__
#define _moviedata__hpp__included__

#include "moviefile.hpp"
#include "movie.hpp"

#define LOAD_STATE_RW 0
#define LOAD_STATE_RO 1
#define LOAD_STATE_PRESERVE 2
#define LOAD_STATE_MOVIE 3
#define LOAD_STATE_DEFAULT 4
#define LOAD_STATE_CURRENT 5
#define LOAD_STATE_BEGINNING 6
#define LOAD_STATE_ROMRELOAD 7
#define LOAD_STATE_INITIAL 8
#define SAVE_STATE 0
#define SAVE_MOVIE 1

extern struct moviefile our_movie;
extern struct loaded_rom* our_rom;
extern bool system_corrupt;
std::vector<char>& get_host_memory();
movie& get_movie();

std::pair<std::string, std::string> split_author(const std::string& author) throw(std::bad_alloc,
	std::runtime_error);

void do_save_state(const std::string& filename) throw(std::bad_alloc, std::runtime_error);
void do_save_movie(const std::string& filename) throw(std::bad_alloc, std::runtime_error);
void do_load_beginning(bool reloading = false) throw(std::bad_alloc, std::runtime_error);
void do_load_state(struct moviefile& _movie, int lmode);
bool do_load_state(const std::string& filename, int lmode);
std::string translate_name_mprefix(std::string original, bool forio = false);

extern std::string last_save;
extern movie_logic movb;

/**
 * Restore the actual core state from quicksave. Only call in rewind callback.
 *
 * Parameter state: The state to restore.
 * Parameter secs: The seconds counter.
 * Parameter ssecs: The subsecond counter.
 */
void mainloop_restore_state(const std::vector<char>& state, uint64_t secs, uint64_t ssecs);

std::string get_mprefix_for_project();
void set_mprefix_for_project(const std::string& pfx);
void set_mprefix_for_project(const std::string& prjid, const std::string& pfx);


#endif
