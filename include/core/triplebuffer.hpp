#ifndef _triplebuffer__hpp__included__
#define _triplebuffer__hpp__included__

#include "core/window.hpp"

#include <stdexcept>

/**
 * Triple buffering logic.
 */
class triplebuffer_logic
{
public:
/**
 * Create new triple buffer logic.
 */
	triplebuffer_logic() throw(std::bad_alloc);
/**
 * Destructor.
 */
	~triplebuffer_logic() throw();
/**
 * Get index for write buffer.Also starts write cycle.
 *
 * Returns: Write buffer index (0-2).
 */
	unsigned start_write() throw();
/**
 * Notify that write cycle has ended.
 */
	void end_write() throw();
/**
 * Get index for read buffer. Also starts read cycle.
 *
 * Returns: Read buffer index (0-2).
 */
	unsigned start_read() throw();
/**
 * Notify that read cycle has ended.
 */
	void end_read() throw();
private:
	triplebuffer_logic(triplebuffer_logic&);
	triplebuffer_logic& operator=(triplebuffer_logic&);
	mutex* mut;
	unsigned read_active;
	unsigned write_active;
	unsigned read_active_slot;
	unsigned write_active_slot;
	unsigned last_complete_slot;
};

#endif
