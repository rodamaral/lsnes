#ifndef _library__running_executable__hpp__included__
#define _library__running_executable__hpp__included__

#include <string>

/**
 * Get full path of current executable.
 *
 * Returns: Full path of current executable.
 * Throws std::runtime_error: Operation not supported!
 */
std::string running_executable();

#endif
