#ifndef _library__assembler_intrinsics_dymmy__hpp__included__
#define _library__assembler_intrinsics_dymmy__hpp__included__

#include "assembler.hpp"
#include <cstdint>
#include <cstdlib>

namespace assembler_intrinsics
{
	struct dummyarch
	{
		dummyarch(assembler::assembler& _a);
		//Label:
		void label(assembler::label& l);
	private:
		assembler::assembler& a;
	};
}

#endif
