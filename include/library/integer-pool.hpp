#ifndef _library__integer_pool__hpp__included__
#define _library__integer_pool__hpp__included__

#include <cstdint>
#include <vector>

class integer_pool
{
public:
/**
 * Create a new pool
 */
	integer_pool() throw();
/**
 * Destroy a pool.
 */
	~integer_pool() throw();
/**
 * Draw a number from the pool.
 *
 * Returns: The number drawn.
 * Throws std::bad_alloc: Not enough memory.
 */
	uint64_t operator()() throw(std::bad_alloc);
/**
 * Return a number into the pool.
 *
 * Parameter num: The number to return.
 */
	void operator()(uint64_t num) throw();
/**
 * Temporarily hold an integer.
 */
	class holder
	{
	public:
/**
 * Allocate an integer.
 */
		holder(integer_pool& _pool)
			: pool(_pool)
		{
			i = pool();
			own = true;
		}
/**
 * Release an integer.
 */
		~holder()
		{
			if(own) pool(i);
		}
/**
 * Grab the integer without transferring ownership.
 *
 * Returns: The integer.
 */
		uint64_t operator()()
		{
			return i;
		}
/**
 * Transfer the ownership.
 *
 * Returns: The integer.
 */
		uint64_t commit()
		{
			own = false;
			return i;
		}
	private:
		holder(const holder&);
		holder& operator=(const holder&);
		integer_pool& pool;
		bool own;
		uint64_t i;
	};
private:
	std::vector<uint8_t> _bits;
	uint8_t _bits2;
	uint8_t* bits;
	bool invalid;
};

#endif
