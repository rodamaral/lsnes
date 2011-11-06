#ifndef _sdmp__hpp__included__
#define _sdmp__hpp__included__

#include <string>
#include <fstream>
#include <cstdint>

#define SDUMP_FLAG_HIRES 1
#define SDUMP_FLAG_INTERLACED 2
#define SDUMP_FLAG_OVERSCAN 4
#define SDUMP_FLAG_PAL 8

class sdump_dumper
{
public:
	sdump_dumper(const std::string& prefix, bool ssflag);
	~sdump_dumper() throw();
	void frame(const uint32_t* rawdata, unsigned flags);
	void sample(short l, short r);
	void end();
private:
	std::string oprefix;
	bool sdump_ss;
	uint64_t ssize;
	uint64_t next_seq;
	bool sdump_iopen;
	std::ofstream out;
};

#endif