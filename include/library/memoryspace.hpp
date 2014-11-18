#ifndef _library__memoryspace__hpp__included__
#define _library__memoryspace__hpp__included__

#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <cstring>
#include "threads.hpp"
#include "arch-detect.hpp"


/**
 * A whole memory space.
 */
class memory_space
{
public:
/**
 * Information about region of memory.
 */
	struct region
	{
/**
 * Destructor.
 */
		virtual ~region() throw();
/**
 * Name of the region (mainly for debugging and showing to user).
 */
		std::string name;
/**
 * Base address of the region.
 */
		uint64_t base;
/**
 * Size of the region.
 */
		uint64_t size;
/**
 * Last address in region.
 */
		uint64_t last_address() const throw() { return base + size - 1; }
/**
 * Endianess of region (-1 => little, 0 => host, 1 => big).
 */
		int endian;
/**
 * Read-only flag.
 */
		bool readonly;
/**
 * Special flag.
 *
 * Signals that this region is not RAM/ROM-like, but is special I/O region where reads and writes might not be
 * consistent.
 */
		bool special;
/**
 * Direct mapping for the region. If not NULL, read/write will not be used, instead all operations directly
 * manipulate this buffer (faster). Must be NULL for special regions.
 */
		unsigned char* direct_map;
/**
 * Read from region.
 *
 * Parameter offset: Offset to start the read.
 * Parameter buffer: Buffer to store the data to.
 * Parameter tsize: Amount to read.
 *
 * Note: The default implementation reads from direct_map.
 * Note: Needs to be overridden if direct_map is NULL.
 * Note: Must be able to read the whole region at once.
 */
		virtual void read(uint64_t offset, void* buffer, size_t tsize);
/**
 * Write to region (writes to readonly regions are ignored).
 *
 * Parameter offset: Offset to start the write.
 * Parameter buffer: Buffer to read the data from.
 * Parameter tsize: Amount to write.
 * Returns: True on success, false on failure.
 *
 * Note: The default implementation writes to direct_map if available and readwrite. Otherwise fails.
 * Note: Must be able to write the whole region at once.
 */
		virtual bool write(uint64_t offset, const void* buffer, size_t tsize);
	};
/**
 * Direct-mapped region.
 */
	struct region_direct : public region
	{
/**
 * Create new direct-mapped region.
 *
 * Parameter name: Name of the region.
 * Parameter base: Base address of the region.
 * Parameter endian: Endianess of the region.
 * Parameter memory: Memory backing the region.
 * Parameter size: Size of the region.
 * Parameter _readonly: If true, region is readonly.
 */
		region_direct(const std::string& name, uint64_t base, int endian, unsigned char* memory,
			size_t size, bool _readonly = false);
/**
 * Destructor.
 */
		~region_direct() throw();
	};
/**
 * Get system endianess.
 */
#ifdef ARCH_IS_I386
	static int get_system_endian() throw() { return -1; }
#else
	static int get_system_endian() throw() { if(!sysendian) sysendian = _get_system_endian(); return sysendian; }
#endif
/**
 * Do native unaligned reads work?
 */
#ifdef ARCH_IS_I386
	static int can_read_unaligned() throw() { return true; }
#else
	static int can_read_unaligned() throw() { return false; }
#endif
/**
 * Lookup region covering address.
 *
 * Parameter address: The address to look up.
 * Returns: The region/offset pair, or NULL/0 if that address is unmapped.
 */
	std::pair<region*, uint64_t> lookup(uint64_t address);
/**
 * Lookup region covering linear address.
 *
 * Parameter linear: The linear address to look up.
 * Returns: The region/offset pair, or NULL/0 if that address is unmapped.
 */
	std::pair<region*, uint64_t> lookup_linear(uint64_t linear);
/**
 * Lookup region with specified index..
 *
 * Parameter n: The index to look up.
 * Returns: The region, or NULL if index is invalid.
 */
	region* lookup_n(size_t n);
/**
 * Get number of regions.
 */
	size_t get_region_count() { return u_regions.size(); }
/**
 * Get linear RAM size.
 *
 * Returns: The linear RAM size in bytes.
 */
	uint64_t get_linear_size() { return linear_size; }
/**
 * Get list of all regions in memory space.
 */
	std::list<region*> get_regions();
/**
 * Set list of all regions in memory space.
 */
	void set_regions(const std::list<region*>& regions);
/**
 * Read an element (primitive type) from memory.
 *
 * Parameter address: The address to read.
 * Returns: The read value.
 */
	template<typename T> T read(uint64_t address);
/**
 * Write an element (primitive type) to memory.
 *
 * Parameter address: The address to write.
 * Parameter value: The value to write.
 * Returns: True on success, false on failure.
 */
	template<typename T> bool write(uint64_t address, T value);
/**
 * Get physical mapping of region of memory.
 *
 * Parameter base: The base address.
 * Parameter size: The size of region.
 * Returns: Pointer to base on success, NULL on failure (range isn't backed by memory)
 */
	char* get_physical_mapping(uint64_t base, uint64_t size);
/**
 * Read a byte range (not across regions).
 *
 * Parameter address: Base address to start the read from.
 * Parameter buffer: Buffer to store the data to.
 * Parameter bsize: Size of buffer.
 */
	void read_range(uint64_t address, void* buffer, size_t bsize);
/**
 * Write a byte range (not across regions).
 *
 * Parameter address: Base address to start the write from.
 * Parameter buffer: Buffer to read the data from.
 * Parameter bsize: Size of buffer.
 * Returns: True on success, false on failure.
 */
	bool write_range(uint64_t address, const void* buffer, size_t bsize);
/**
 * Read an element (primitive type) from memory.
 *
 * Parameter linear: The linear address to read.
 * Returns: The read value.
 */
	template<typename T> T read_linear(uint64_t linear);
/**
 * Write an element (primitive type) to memory.
 *
 * Parameter linear: The linear address to write.
 * Parameter value: The value to write.
 * Returns: True on success, false on failure.
 */
	template<typename T> bool write_linear(uint64_t linear, T value);
/**
 * Read a byte range (not across regions).
 *
 * Parameter linear: Base linear address to start the read from.
 * Parameter buffer: Buffer to store the data to.
 * Parameter bsize: Size of buffer.
 */
	void read_range_linear(uint64_t linear, void* buffer, size_t bsize);
/**
 * Write a byte range (not across regions).
 *
 * Parameter linear: Base linear address to start the write from.
 * Parameter buffer: Buffer to read the data from.
 * Parameter bsize: Size of buffer.
 * Returns: True on success, false on failure.
 */
	bool write_range_linear(uint64_t linear, const void* buffer, size_t bsize);
/**
 * Read complete linear memory.
 *
 * Parameter buffer: Buffer to store to (get_linear_size() bytes).
 */
	void read_all_linear_memory(uint8_t* buffer);
/**
 * Get textual identifier for mapped address.
 *
 * Parameter addr: The address to get textual form for.
 * Returns: The textual address.
 */
	std::string address_to_textual(uint64_t addr);
private:
	threads::lock mlock;
	std::vector<region*> u_regions;
	std::vector<region*> u_lregions;
	std::vector<uint64_t> linear_bases;
	uint64_t linear_size;
	static int _get_system_endian();
	static int sysendian;
};

/**
 * Calculate span of strided request.
 *
 * Parameter base: The base address.
 * Parameter size: The size of each row.
 * Parameter rows: Number of raows.
 * Parameter stride: The stride.
 * Returns: (low, high) of access bounds. low > high if no memory is accessed.
 */
std::pair<uint64_t, uint64_t> memoryspace_row_bounds(uint64_t base, uint64_t size, uint64_t rows,
	uint64_t stride);

/**
 * Does strided request fit in certain bounds?
 *
 * Parameter base: The base address.
 * Parameter size: The size of each row.
 * Parameter rows: Number of raows.
 * Parameter stride: The stride.
 * Parameter limit: The size of area.
 * Returns: True if entiere access stays below limit, false otherwise.
 */
bool memoryspace_row_limited(uint64_t base, uint64_t size, uint64_t rows, uint64_t stride, uint64_t limit);

#endif
