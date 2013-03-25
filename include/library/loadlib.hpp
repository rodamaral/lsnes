#ifndef _library__loadlib__hpp__included__
#define _library__loadlib__hpp__included__

#include <string>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

/**
 * A loaded library.
 */
class loaded_library
{
public:
/**
 * Load a new library.
 *
 * Parameter filename: The name of file.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Error loading shared library.
 */
	loaded_library(const std::string& filename) throw(std::bad_alloc, std::runtime_error);
/**
 * Unload a library.
 */
	~loaded_library() throw();
/**
 * Look up a symbol.
 *
 * Parameter symbol: The symbol to look up.
 * Returns: The symbol value.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Error looking up the symbol.
 */
	void* operator[](const std::string& symbol) throw(std::bad_alloc, std::runtime_error);
/**
 * See what libraries are called on this platform.
 *
 * Returns: The name of library.
 */
	static const std::string& call_library() throw();
/**
 * See what standard library extension is on this platform.
 *
 * Returns: The extension of library.
 */
	static const std::string& call_library_ext() throw();
private:
	loaded_library(const loaded_library&);
	loaded_library& operator=(const loaded_library&);
#if defined(_WIN32) || defined(_WIN64)
	HMODULE handle;
#elif !defined(NO_DLFCN)
	void* handle;
#endif
};

#endif
