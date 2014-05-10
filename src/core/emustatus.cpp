#include "core/emustatus.hpp"

const int _lsnes_status::pause_none = 0;
const int _lsnes_status::pause_normal = 1;
const int _lsnes_status::pause_break = 2;
const uint64_t _lsnes_status::subframe_savepoint = 0xFFFFFFFFFFFFFFFEULL;
const uint64_t _lsnes_status::subframe_video = 0xFFFFFFFFFFFFFFFFULL;
