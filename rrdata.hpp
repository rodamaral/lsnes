#ifndef _rrdata__hpp__included__
#define _rrdata__hpp__included__

#define RRDATA_BYTES 32
#include <cstdint>
#include <stdexcept>

class rrdata
{
public:
	struct instance
	{
		instance() throw(std::bad_alloc);
		instance(unsigned char* b) throw();
		unsigned char bytes[RRDATA_BYTES];
		bool operator<(const struct instance& i) const throw();
		bool operator==(const struct instance& i) const throw();
		const struct instance operator++(int) throw();
		const struct instance& operator++() throw();
		
	};

	static void read_base(const std::string& project) throw(std::bad_alloc);
	static void close() throw();
	static void add(const struct instance& i) throw(std::bad_alloc);
	static void add_internal() throw(std::bad_alloc);
	static uint64_t write(std::ostream& strm) throw(std::bad_alloc);
	static uint64_t read(std::istream& strm) throw(std::bad_alloc);
	static struct instance* internal;
};

std::ostream& operator<<(std::ostream& os, const struct rrdata::instance& i);

#endif