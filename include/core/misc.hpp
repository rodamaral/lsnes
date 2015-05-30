#ifndef _misc__hpp__included__
#define _misc__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include "library/string.hpp"
#include "library/memtracker.hpp"

/**
 * \brief Fatal error.
 *
 * Fatal error.
 */
void fatal_error() throw();

/**
 * \brief Get path to config directory.
 *
 * \return The config directory path.
 * \throw std::bad_alloc Not enough memory.
 */
text get_config_path() throw(std::bad_alloc);

/**
 * \brief Panic on OOM.
 */
void OOM_panic();


uint32_t gcd(uint32_t a, uint32_t b) throw();

/**
 * Return hexadecimal representation of address
 */
text format_address(void* addr);

/**
 * Get state of running global ctors flag.
 */
bool in_global_ctors();
/**
 * Clear the global ctors flag.
 */
void reached_main();

/**
 * Clean up filename from dangerous chars
 */
text safe_filename(const text& str);

/**
 * Mangle some characters ()|/
 */
text mangle_name(const text& orig);

/**
 * Return a new temporary file. The file will be created.
 */
text get_temp_file();

#endif
