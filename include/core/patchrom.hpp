#ifndef _patchrom__hpp__included__
#define _patchrom__hpp__included__

#include <vector>
#include <stdexcept>

/**
 * ROM patcher.
 */
class rom_patcher
{
public:
/**
 * Construct new rom patcher.
 *
 * Parameter original: The original.
 * Parameter size: Estimate for target size. May be 0.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	rom_patcher(const std::vector<char>& original, size_t size = 0) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~rom_patcher() throw();
/**
 * Literial insert.
 *
 * If insertion offset is beyond the end of target, target is extended with zeroes.
 *
 * Parameter pos: Position to insert the data to.
 * Parameter buf: The buffer of data to insert.
 * Parameter bufsize: Size of the buffer.
 * Parameter time: Number of times to insert.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	void literial_insert(size_t pos, const char* buf, size_t bufsize, size_t times = 1) throw(std::bad_alloc);
/**
 * Copy data from source.
 *
 * Data outside source is read as zeroes.
 *
 * Parameter srcpos: Position of data in source.
 * Parameter dstpos: Position of data in destination
 * Parameter size: Size to copy.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	void copy_source(size_t srcpos, size_t dstpos, size_t size) throw(std::bad_alloc);
/**
 * Copy data from destination.
 *
 * Reads the data written by call (like in LZ77 decompression). Data outside target reads as zeroes.
 *
 * Parameter srcpos: Position of data to copy from.
 * Parameter dstpos: Position of data to copy to.
 * Parameter size: Size to copy.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	void copy_destination(size_t srcpos, size_t dstpos, size_t size) throw(std::bad_alloc);
/**
 * Change the size.
 *
 * Parameter size: New size for target.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	void change_size(size_t size) throw(std::bad_alloc);
/**
 * Set apply offset.
 *
 * All passed offsets are adjusted by this amount. Writes to negative offsets are ignored.
 *
 * Parameter _offset: The new offset. Can be negative.
 */
	void set_offset(int32_t _offset) throw();
/**
 * Get the patched output.
 *
 * Returns the output.
 */
	std::vector<char> get_output() throw(std::bad_alloc);
private:
	void do_oob_read_warning() throw();
	void do_oob_write_warning() throw();
	void resize_request(size_t dstpos, size_t reqsize);
	const std::vector<char>& original;
	std::vector<char> target;
	int32_t offset;
	bool oob_write_warning;
	bool oob_read_warning;
};

/**
 * Patch a ROM. Autodetects type of patch.
 *
 * Parameter original: The orignal file to patch.
 * Parameter patch: The patch to apply.
 * Parameter offset: Offset to apply.
 * Returns The patched file.
 *
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Invalid patch file.
 */
std::vector<char> do_patch_file(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error);

#endif
