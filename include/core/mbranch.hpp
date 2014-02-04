#ifndef _mbranch__hpp__included__
#define _mbranch__hpp__included__

#define MBRANCH_IMPORT_TEXT 0
#define MBRANCH_IMPORT_BINARY 1
#define MBRANCH_IMPORT_MOVIE 2

std::string mbranch_name(const std::string& internal);
std::set<std::string> mbranch_enumerate();
std::string mbranch_get();
void mbranch_set(const std::string& branch);
void mbranch_new(const std::string& branch, const std::string& from);
void mbranch_rename(const std::string& oldn, const std::string& newn);
void mbranch_delete(const std::string& branch);
std::set<std::string> mbranch_movie_branches(const std::string& filename);
void mbranch_import(const std::string& filename, const std::string& ibranch, const std::string& branchname, int mode);
void mbranch_export(const std::string& filename, const std::string& branchname, bool binary);

#endif
