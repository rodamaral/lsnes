#ifndef _library_triplebuffer__hpp__included__
#define _library_triplebuffer__hpp__included__

#include <functional>
#include <stdexcept>
#include "threads.hpp"

namespace triplebuffer
{
/**
 * Triple-buffering logic.
 */
class logic
{
public:
/**
 * Create a new triple buffer.
 */
	logic() throw();
/**
 * Get read pointer and increment read count by 1.
 *
 * Return value: Read pointer (0-2).
 *
 * Note: If read count is zero, last complete buffer is returned. Otherwise the same buffer as last call is returned.
 */
	unsigned get_read() throw();
/**
 * Put read pointer and decrement read count by 1.
 *
 * Throws std::logic_error: If read count is 0.
 */
	void put_read() throw(std::logic_error);
/**
 * Get write pointer and increment write count by 1.
 *
 * Return value: Write pointer (0-2).
 *
 * Note: If write count is nonzero before call, the returned buffer is the same as last time.
 */
	unsigned get_write() throw();
/**
 * Put write pointer and decrement write count by 1. If write count hits 0, the buffer is marked as last complete
 * buffer.
 *
 * Throws std::logic_error: If write count is 0.
 */
	void put_write() throw(std::logic_error);
/**
 * Call specified function synchronously for last written buffer.
 *
 * The buffer number is passed to specified function.
 */
	void read_last_write_synchronous(std::function<void(unsigned)> fn) throw();
private:
	threads::lock lock;
	unsigned last_complete;		//Number of last completed buffer
	unsigned current_read;		//Number of current read buffer (only valid if count_read > 0).
	unsigned current_write;		//Number of current write buffer (only valid if count_write > 0).
	unsigned count_read;		//Number of readers.
	unsigned count_write;		//Number of writers.
};

/**
 * Triple buffer with objects.
 */
template<typename T>
class triplebuffer
{
public:
/**
 * Create a new triple buffer.
 *
 * Parameter A: Object #1.
 * Parameter B: Object #2.
 * Parameter C: Object #3.
 */
	triplebuffer(T& A, T& B, T& C) throw()
	{
		objs[0] = &A;
		objs[1] = &B;
		objs[2] = &C;
	}
/**
 * Get read pointer and increment read count by 1.
 *
 * Return value: Read pointer.
 *
 * Note: If read count is zero, last complete buffer is returned. Otherwise the same buffer as last call is returned.
 */
	T& get_read() throw() { return *objs[l.get_read()]; }
/**
 * Put read pointer and decrement read count by 1.
 *
 * Throws std::logic_error: If read count is 0.
 */
	void put_read() throw(std::logic_error) { l.put_read(); }
/**
 * Get write pointer and increment write count by 1.
 *
 * Return value: Write pointer.
 *
 * Note: If write count is nonzero before call, the returned buffer is the same as last time.
 */
	T& get_write() throw() { return *objs[l.get_write()]; }
/**
 * Put write pointer and decrement write count by 1. If write count hits 0, the buffer is marked as last complete
 * buffer.
 *
 * Throws std::logic_error: If write count is 0.
 */
	void put_write() throw(std::logic_error) { l.put_write(); }
/**
 * Call specified function synchronously for last written buffer.
 *
 * The buffer itself is passed to the function.
 */
	void read_last_write_synchronous(std::function<void(T&)> fn) throw()
	{
		l.read_last_write_synchronous([this,fn](unsigned x){ fn(*objs[x]); });
	}
private:
	T* objs[3];
	logic l;
};
}

#endif
