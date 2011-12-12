#ifndef _patchrom__hpp__included__
#define _patchrom__hpp__included__

#include <vector>
#include <stdexcept>

/**
 * Patch a ROM. Autodetects type of patch.
 *
 * Parameter original: The orignal file to patch.
 * Parameter patch: The patch to apply.
 * Parameter offset: Offset to apply.
 * Returns The patched file.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Invalid patch file.
 */
std::vector<char> do_patch_file(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error);

#endif
