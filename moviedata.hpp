#ifndef _moviedata__hpp__included__
#define _moviedata__hpp__included__

#include "moviefile.hpp"
#include "movie.hpp"

#define LOAD_STATE_RW 0
#define LOAD_STATE_RO 1
#define LOAD_STATE_PRESERVE 2
#define LOAD_STATE_MOVIE 3
#define LOAD_STATE_DEFAULT 4
#define SAVE_STATE 0
#define SAVE_MOVIE 1

extern struct moviefile our_movie;
extern struct loaded_rom* our_rom;
extern bool system_corrupt;
std::vector<char>& get_host_memory();
movie& get_movie();

void do_save_state(window* win, const std::string& filename) throw(std::bad_alloc, std::runtime_error);
void do_save_movie(window* win, const std::string& filename) throw(std::bad_alloc, std::runtime_error);
void do_load_state(window* win, struct moviefile& _movie, int lmode);
void do_load_state(window* win, const std::string& filename, int lmode);


extern movie_logic movb;

#endif
