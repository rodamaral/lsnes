#include "assembler-intrinsics-dummy.hpp"
#include <stdexcept>

namespace assembler_intrinsics
{
	dummyarch::dummyarch(assembler::assembler& _a)
		: a(_a)
	{
	}

	//Label:
	void dummyarch::label(assembler::label& l)
	{
		a._label(l);
	}
}
