#ifndef _movie__hpp__included__
#define _movie__hpp__included__

#include <string>
#include <cstdint>
#include <stdexcept>
#include "core/controllerframe.hpp"
#include "library/movie.hpp"

/**
 * Class encapsulating bridge logic between bsnes interface and movie code.
 */
class movie_logic
{
public:
/**
 * Create new bridge.
 */
	movie_logic() throw();

/**
 * Get the movie instance associated.
 *
 * returns: The movie instance.
 */
	movie& get_movie() throw();

/**
 * Notify about new frame starting.
 *
 * returns: Reset status for the new frame.
 */
	long new_frame_starting(bool dont_poll) throw(std::bad_alloc, std::runtime_error);

/**
 * Poll for input.
 *
 * parameter port: The port number.
 * parameter dev: The controller index.
 * parameter id: Control id.
 * returns: Value for polled input.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error polling for input.
 */
	short input_poll(unsigned port, unsigned dev, unsigned id) throw(std::bad_alloc, std::runtime_error);

/**
 * Called when movie code needs new controls snapshot.
 *
 * parameter subframe: True if this is for subframe update, false if for frame update.
 */
	controller_frame update_controls(bool subframe) throw(std::bad_alloc, std::runtime_error);
private:
	movie mov;
};

#endif
