#include "core/misc.hpp"
#include "core/rrdata.hpp"

#include <set>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "core/misc.hpp"

rrdata_set::instance next_rrdata()
{
	static bool init = false;
	static rrdata_set::instance inst;
	if(!init)
		inst = rrdata_set::instance(get_random_hexstring(2 * RRDATA_BYTES));
	init = true;
	return inst++;
}

std::string rrdata_filename(const std::string& projectid)
{
	return get_config_path() + "/" + projectid + ".rr";
}

//
// XABCDEFXXXXXXXXX
// 0123456789XXXXXX
//
// ABCDEF0123456789XXXXXX
