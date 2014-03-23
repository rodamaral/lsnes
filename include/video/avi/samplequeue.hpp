#ifndef _avi__samplequeue__hpp__included__
#define _avi__samplequeue__hpp__included__

#include <deque>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include "library/threads.hpp"

/**
 * Sample queue.
 */
class sample_queue
{
public:
/**
 * Construct new sample queue.
 */
	sample_queue();
/**
 * Push samples into queue.
 *
 * Parameter samples: The samples to push
 * Parameter count: The number of samples (assumed mono) to push.
 * Note: This is thread safe.
 */
	void push(const int16_t* samples, size_t count);
/**
 * Pull samples from queue.
 *
 * Parameter samples: The pulled samples are stored here.
 * Parameter count: The number of samples (assumed mono) to pull.
 * Note: This is thread safe.
 * Note: Trying to pull nonexistent samples causes zeros to be pulled.
 */
	void pull(int16_t* samples, size_t count);
/**
 * Get number of available samples.
 *
 * Returns: Number of available samples.
 * Note: This is thread safe.
 */
	size_t available();
private:
	size_t _available();
	std::vector<int16_t> data;
	bool blank;
	size_t rptr;
	size_t wptr;
	size_t size;
	threads::lock mlock;
};

struct frame_object
{
	uint32_t* data;
	uint32_t* odata;
	uint32_t width;
	uint32_t height;
	uint32_t fps_n;
	uint32_t fps_d;
	uint32_t stride;
	bool force_break;
};

#endif
