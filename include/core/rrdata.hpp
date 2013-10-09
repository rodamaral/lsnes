#ifndef _rrdata__hpp__included__
#define _rrdata__hpp__included__

#include "library/rrdata.hpp"

extern rrdata_set rrdata;

rrdata_set::instance random_rrdata();
std::string rrdata_filename(const std::string& projectid);

#endif