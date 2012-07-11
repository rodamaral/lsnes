#ifndef _slot__hpp__included__
#define _slot__hpp__included__

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

std::vector<char> do_patch_file(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error);

/**
 * ROM patcher.
 */
struct rom_patcher
{
/**
 * Constructor.
 */
	rom_patcher() throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~rom_patcher() throw();
/**
 * Identify patch.
 *
 * Parameter patch: The patch.
 * Returns: True if my format, false if not.
 */
	virtual bool identify(const std::vector<char>& patch) throw() = 0;
/**
 * Do the patch.
 */
	virtual void dopatch(std::vector<char>& out, const std::vector<char>& original,
		const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error) = 0;
};

#endif
