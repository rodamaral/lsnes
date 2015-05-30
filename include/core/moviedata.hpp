#ifndef _moviedata__hpp__included__
#define _moviedata__hpp__included__

#include "core/emustatus.hpp"
#include "core/moviefile.hpp"
#include "core/movie.hpp"
#include "library/rrdata.hpp"

//Load state, always switch to Readwrite.
#define LOAD_STATE_RW 0
//Load state, never switch to Readwrite.
#define LOAD_STATE_RO 1
//Load state, don't muck with movie data.
#define LOAD_STATE_PRESERVE 2
//Load state, ignoring the actual state.
#define LOAD_STATE_MOVIE 3
//Load state, switch to readwrite if previously there and loaded state is at the end.
#define LOAD_STATE_DEFAULT 4
//Load state, switch to readwrite if previously there.
#define LOAD_STATE_CURRENT 5
//No load state, rewind movie to start.
#define LOAD_STATE_BEGINNING 6
//No load state, reload ROM.
#define LOAD_STATE_ROMRELOAD 7
//Load state, loading everything, switch to readwrite if loaded state is at the end.
#define LOAD_STATE_INITIAL 8
//Load state, along with all branches, switch to readwrite if loaded state is at the end.
#define LOAD_STATE_ALLBRANCH 9

#define SAVE_STATE 0
#define SAVE_MOVIE 1

text resolve_relative_path(const text& path);
std::pair<text, text> split_author(const text& author) throw(std::bad_alloc, std::runtime_error);

void do_save_state(const text& filename, int binary) throw(std::bad_alloc, std::runtime_error);
void do_save_movie(const text& filename, int binary) throw(std::bad_alloc, std::runtime_error);
void do_load_rom() throw(std::bad_alloc, std::runtime_error);
void do_load_rewind() throw(std::bad_alloc, std::runtime_error);
void do_load_state(struct moviefile& _movie, int lmode, bool& used);
bool do_load_state(const text& filename, int lmode);
text translate_name_mprefix(text original, int& binary, int save);

extern text last_save;

/**
 * Restore the actual core state from quicksave. Only call in rewind callback.
 *
 * Parameter state: The state to restore.
 * Parameter secs: The seconds counter.
 * Parameter ssecs: The subsecond counter.
 */
void mainloop_restore_state(const dynamic_state& state);

text get_mprefix_for_project();
void set_mprefix_for_project(const text& pfx);
void set_mprefix_for_project(const text& prjid, const text& pfx);

class rrdata
{
public:
	rrdata();
	rrdata_set::instance operator()();
	static text filename(const text& projectid);
private:
	bool init;
	rrdata_set::instance next;
};

#endif
