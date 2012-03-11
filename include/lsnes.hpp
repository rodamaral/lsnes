#ifndef lsnes__hpp__included__
#define lsnes__hpp__included__

#include <iostream>

#define STILL_HERE do { std::cerr << "Still here at file " << __FILE__ << " line " << __LINE__ << "." << std::endl; } \
	while(0)

#define DEBUGGER
#ifdef BSNES_IS_COMPAT
#define PROFILE_COMPATIBILITY
#else
#define PROFILE_ACCURACY
#endif

extern std::string lsnes_version;
extern std::string lsnes_git_revision;

#endif
