#include "core/misc.hpp"
#include "core/rrdata.hpp"

#include <set>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

rrdata_set rrdata;

rrdata_set::instance random_rrdata()
{
	return rrdata_set::instance(get_random_hexstring(2 * RRDATA_BYTES));
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
