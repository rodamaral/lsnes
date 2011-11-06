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
void do_load_state(struct moviefile& _movie, int lmode);
bool do_load_state(const std::string& filename, int lmode);

extern movie_logic movb;

#endif
