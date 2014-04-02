#include "range.hpp"
#include "minmax.hpp"

range range::make_b(uint32_t l, uint32_t h)
{
	return range(l, h);
}

range::range(uint32_t l, uint32_t h)
{
	_low = l;
	_high = h;
}

range& range::operator+=(uint32_t o) throw()
{
	_low += o;
	_high += o;

	return *this;
}

range& range::operator&=(const range& b) throw()
{
	//Offset ranges down.
	uint32_t offset = _low;
	uint32_t L = _high - _low;
	uint32_t A = b._low - _low;
	uint32_t B = b._high - _low;

	bool asl = (A < L);	//There is initial overlap between ranges.
	bool asb = (A <= B);	//Range 2 does not warp around.

	//If there is initial overlap, the range bottom is A+offset, otherwise 0+offset.
	_low = offset + (asl ? A : 0);
	//If asl EQU asb, then range top is min(L, B). Otherwise if asl, it is L, otherwise 0.
	_high = offset + ((asl == asb) ? min(L, B) : (asl ? L : 0));

	return *this;
}

bool range::in(uint32_t x) const throw()
{
	return ((x - _low) < (_high - _low));
}
