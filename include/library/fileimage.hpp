#ifndef _library__fileimage__hpp__included__
#define _library__fileimage__hpp__included__

#include <functional>
#include <cstdint>
#include <list>
#include <vector>
#include "threadtypes.hpp"

class sha256_hasher;

/**
 * Future for SHA-256 computation.
 */
class sha256_future
{
public:
/**
 * Construct a null future, never resolves.
 */
	sha256_future();
/**
 * Construct a future, with value that is immediately resolved.
 */
	sha256_future(const std::string& value);
/**
 * Is the result known?
 */
	bool ready() const;
/**
 * Read the result (or throw error). Waits until result is ready.
 */
	std::string read() const;
/**
 * Copy a future.
 */
	sha256_future(const sha256_future& f);
/**
 * Assign a future.
 */
	sha256_future& operator=(const sha256_future& f);
/**
 * Destroy a future.
 */
	~sha256_future();
private:
/**
 * Create a new future.
 */
	sha256_future(sha256_hasher& h, unsigned id);
/**
 * Resolve a future.
 */
	void resolve(unsigned id, const std::string& hash);
	void resolve_error(unsigned id, const std::string& err);

	friend class sha256_hasher;
	mutable mutex_class mutex;
	mutable cv_class condition;
	bool is_ready;
	unsigned cbid;
	std::string value;
	std::string error;
	sha256_future* prev;
	sha256_future* next;
	sha256_hasher* hasher;
};

/**
 * Class performing SHA-256 hashing.
 */
class sha256_hasher
{
public:
/**
 * Create a new SHA-256 hasher.
 */
	sha256_hasher();
/**
 * Destroy a SHA-256 hasher. Causes all current jobs to fail.
 */
	~sha256_hasher();
/**
 * Set callback.
 */
	void set_callback(std::function<void(uint64_t)> cb);
/**
 * Compute SHA-256 of file.
 */
	sha256_future operator()(const std::string& filename);
/**
 * Thread entrypoint.
 */
	void entrypoint();
private:
	void link(sha256_future& future);
	void unlink(sha256_future& future);
	void send_callback(uint64_t this_completed);
	void send_idle();

	friend class sha256_future;
	struct queue_job
	{
		std::string filename;
		uint64_t size;
		unsigned cbid;
		volatile unsigned interested;
	};
	sha256_hasher(const sha256_hasher&);
	sha256_hasher& operator=(const sha256_hasher&);
	thread_class* hash_thread;
	mutex_class mutex;
	cv_class condition;
	std::list<queue_job> queue;
	std::list<queue_job>::iterator current_job;
	sha256_future* first_future;
	sha256_future* last_future;
	unsigned next_cbid;
	std::function<void(uint64_t)> progresscb;
	bool quitting;
	uint64_t total_work;
};

/**
 * Some loaded data or indication of no data.
 *
 * The loaded images are copied in CoW manner.
 */
struct loaded_image
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
	loaded_image() throw(std::bad_alloc);

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
	loaded_image(sha256_hasher& hasher, const std::string& filename, const std::string& base,
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
 * SHA-256 for the data in this slot if data is valid. If no valid data, this field is "".
 *
 * Note, for file images, this takes a bit of time to fill.
 */
	sha256_future sha_256;
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

#endif
