#ifndef _library__fileimage__hpp__included__
#define _library__fileimage__hpp__included__

#include <functional>
#include <cstdint>
#include <list>
#include <vector>
#include "threadtypes.hpp"

namespace fileimage
{
class hash;

/**
 * Future for SHA-256 computation.
 */
class hashval
{
public:
/**
 * Construct a null future, never resolves.
 */
	hashval();
/**
 * Construct a future, with value that is immediately resolved.
 */
	hashval(const std::string& value, uint64_t _prefix = 0);
/**
 * Is the result known?
 */
	bool ready() const;
/**
 * Read the result (or throw error). Waits until result is ready.
 */
	std::string read() const;
/**
 * Read the prefix value. Waits until result is ready.
 */
	uint64_t prefix() const;
/**
 * Copy a future.
 */
	hashval(const hashval& f);
/**
 * Assign a future.
 */
	hashval& operator=(const hashval& f);
/**
 * Destroy a future.
 */
	~hashval();
private:
/**
 * Create a new future.
 */
	hashval(hash& h, unsigned id);
/**
 * Resolve a future.
 */
	void resolve(unsigned id, const std::string& hash, uint64_t _prefix);
	void resolve_error(unsigned id, const std::string& err);

	friend class hash;
	mutable mutex_class mutex;
	mutable cv_class condition;
	bool is_ready;
	unsigned cbid;
	uint64_t prefixv;
	std::string value;
	std::string error;
	hashval* prev;
	hashval* next;
	hash* hasher;
};

/**
 * Class performing SHA-256 hashing.
 */
class hash
{
public:
/**
 * Create a new SHA-256 hasher.
 */
	hash();
/**
 * Destroy a SHA-256 hasher. Causes all current jobs to fail.
 */
	~hash();
/**
 * Set callback.
 */
	void set_callback(std::function<void(uint64_t, uint64_t)> cb);
/**
 * Compute SHA-256 of file.
 */
	hashval operator()(const std::string& filename, uint64_t prefixlen = 0);
/**
 * Compute SHA-256 of file.
 */
	hashval operator()(const std::string& filename, std::function<uint64_t(uint64_t)> prefixlen);
/**
 * Thread entrypoint.
 */
	void entrypoint();
private:
	void link(hashval& future);
	void unlink(hashval& future);
	void send_callback(uint64_t this_completed);
	void send_idle();

	friend class hashval;
	struct queue_job
	{
		std::string filename;
		uint64_t prefix;
		uint64_t size;
		unsigned cbid;
		volatile unsigned interested;
	};
	hash(const hash&);
	hash& operator=(const hash&);
	thread_class* hash_thread;
	mutex_class mutex;
	cv_class condition;
	std::list<queue_job> queue;
	std::list<queue_job>::iterator current_job;
	hashval* first_future;
	hashval* last_future;
	unsigned next_cbid;
	std::function<void(uint64_t, uint64_t)> progresscb;
	bool quitting;
	uint64_t total_work;
	uint64_t work_size;
};

/**
 * Some loaded data or indication of no data.
 *
 * The loaded images are copied in CoW manner.
 */
struct image
{
/**
 * Information about image to load.
 */
	struct info
	{
		enum _type
		{
			IT_NONE,	//Only used in type field of image.
			IT_MEMORY,
			IT_MARKUP,
			IT_FILE
		};
		_type type;
		unsigned headersize;
	};
/**
 * Construct empty image.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	image() throw(std::bad_alloc);

/**
 * This constructor construct slot by reading data from file. If filename is "", constructs an empty slot.
 *
 * parameter hasher: Hasher to use.
 * parameter filename: The filename to read. If "", empty slot is constructed.
 * parameter base: Base filename to interpret the filename against. If "", no base filename is used.
 * parameter imginfo: Image information.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the data.
 */
	image(hash& hasher, const std::string& filename, const std::string& base,
		const struct info& imginfo) throw(std::bad_alloc, std::runtime_error);

/**
 * This method patches this slot using specified IPS patch.
 *
 * If the object was shared, this breaks the sharing, with this object gaining dedicated copy.
 *
 * parameter patch: The patch to apply
 * parameter offset: The amount to add to the offsets in the IPS file. Parts with offsets below zero are not patched.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Bad IPS patch, or trying to patch file image.
 */
	void patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error);
/**
 * Type.
 */
	info::_type type;
/**
 * Filename this is loaded from.
 */
	std::string filename;
/**
 * ROM name hint.
 */
	std::string namehint;
/**
 * The actual data for this slot.
 */
	std::shared_ptr<std::vector<char>> data;
/**
 * Number of bytes stripped when loading.
 */
	uint64_t stripped;
/**
 * SHA-256 for the data in this slot if data is valid. If no valid data, this field is "".
 *
 * Note, for file images, this takes a bit of time to fill.
 */
	hashval sha_256;
/**
 * Get pointer to loaded data
 *
 * returns: Pointer to loaded data, or NULL if slot is blank.
 */
	operator const char*() const throw()
	{
		return data ? reinterpret_cast<const char*>(&(*data)[0]) : NULL;
	}
/**
 * Get pointer to loaded data
 *
 * returns: Pointer to loaded data, or NULL if slot is blank.
 */
	operator const uint8_t*() const throw()
	{
		return data ? reinterpret_cast<const uint8_t*>(&(*data)[0]) : NULL;
	}
/**
 * Get size of slot
 *
 * returns: The number of bytes in slot, or 0 if slot is blank.
 */
	operator unsigned() const throw()
	{
		return data ? data->size() : 0;
	}
};

/**
 * Get headersize function.
 */
std::function<uint64_t(uint64_t)> std_headersize_fn(uint64_t hdrsize);
}
#endif
