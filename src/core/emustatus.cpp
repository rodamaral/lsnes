#include "core/emustatus.hpp"

namespace
{
	_lsnes_status statusA;
	_lsnes_status statusB;
	_lsnes_status statusC;

	struct init {
		init()
		{
			statusA.valid = false;
			statusB.valid = false;
			statusC.valid = false;
		}
	} _init;
}

const int _lsnes_status::pause_none = 0;
const int _lsnes_status::pause_normal = 1;
const int _lsnes_status::pause_break = 2;
const uint64_t _lsnes_status::subframe_savepoint = 0xFFFFFFFFFFFFFFFEULL;
const uint64_t _lsnes_status::subframe_video = 0xFFFFFFFFFFFFFFFFULL;


triplebuffer::triplebuffer<_lsnes_status> lsnes_status(statusA, statusB, statusC);
