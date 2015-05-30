#ifndef _mbranch__hpp__included__
#define _mbranch__hpp__included__

#define MBRANCH_IMPORT_TEXT 0
#define MBRANCH_IMPORT_BINARY 1
#define MBRANCH_IMPORT_MOVIE 2

class emulator_dispatch;
class status_updater;
class movie_logic;

struct movie_branches
{
	movie_branches(movie_logic& _mlogic, emulator_dispatch& _dispatch, status_updater& _supdater);
	text name(const text& internal);
	std::set<text> enumerate();
	text get();
	void set(const text& branch);
	void _new(const text& branch, const text& from);
	void rename(const text& oldn, const text& newn);
	void _delete(const text& branch);
	std::set<text> _movie_branches(const text& filename);
	void import_branch(const text& filename, const text& ibranch, const text& branchname,
		int mode);
	void export_branch(const text& filename, const text& branchname, bool binary);
private:
	movie_logic& mlogic;
	emulator_dispatch& edispatch;
	status_updater& supdater;
};

#endif
