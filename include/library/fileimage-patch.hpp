#ifndef _library__fileimage_patch__hpp__included__
#define _library__fileimage_patch__hpp__included__

#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace fileimage
{
std::vector<char> patch(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error);

/**
 * ROM patcher.
 */
struct patcher
{
/**
 * Constructor.
 */
	patcher() throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~patcher() throw();
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
}

#endif
