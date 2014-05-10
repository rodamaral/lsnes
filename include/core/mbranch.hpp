#ifndef _mbranch__hpp__included__
#define _mbranch__hpp__included__

#define MBRANCH_IMPORT_TEXT 0
#define MBRANCH_IMPORT_BINARY 1
#define MBRANCH_IMPORT_MOVIE 2

struct movie_branches
{
	movie_branches(movie_logic* _mlogic);
	std::string name(const std::string& internal);
	std::set<std::string> enumerate();
	std::string get();
	void set(const std::string& branch);
	void _new(const std::string& branch, const std::string& from);
	void rename(const std::string& oldn, const std::string& newn);
	void _delete(const std::string& branch);
	std::set<std::string> _movie_branches(const std::string& filename);
	void import(const std::string& filename, const std::string& ibranch, const std::string& branchname, int mode);
	void _export(const std::string& filename, const std::string& branchname, bool binary);
private:
	movie_logic& mlogic;
};

#endif
