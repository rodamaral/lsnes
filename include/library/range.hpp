#ifndef _library__range__hpp__included__
#define _library__range__hpp__included__

#include <cstdint>

/**
 * A range.
 *
 * These ranges wrap around. All range can't be represented.
 */
class range
{
public:
/**
 * Make range [low, high)
 */
	static range make_b(uint32_t low, uint32_t high);
/**
 * Make range [low, low + size)
 */
	static range make_s(uint32_t low, uint32_t size) { return make_b(low, low + size); }
/**
 * Make range [0, size)
 */
	static range make_w(uint32_t size) { return make_b(0, size); }
/**
 * Offset range.
 *
 * Parameter o: Offset to add to both ends of range.
 */
	range operator+(uint32_t o) const throw() { range x = *this; x += o; return x; }
	range& operator+=(uint32_t o) throw();
	range operator-(uint32_t o) const throw() { return *this + (-o); }
	range& operator-=(uint32_t o) throw() { return *this += (-o); }
/**
 * Intersect range.
 *
 * If there are multiple overlaps, the one starting nearer to start of first operand is chose.
 *
 * Parameter b: Another range to intersect.
 */
	range operator&(const range& b) const throw() { range x = *this; x &= b; return x; }
	range& operator&=(const range& b) throw();
/**
 * Return lower limit of range
 */
	uint32_t low() const throw() { return _low; }
/**
 * Return upper limit of range
 */
	uint32_t high() const throw() { return _high; }
/**
 * Return size of range
 */
	uint32_t size() const throw() { return _high - _low; }
/**
 * Number in range?
 */
	bool in(uint32_t x) const throw();
private:
	range(uint32_t l, uint32_t h);
	uint32_t _low;		//Inclusive.
	uint32_t _high;		//Exclusive.
};

#endif
