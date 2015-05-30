#ifndef _interface__disassembler__hpp__included__
#define _interface__disassembler__hpp__included__

#include <functional>
#include <string>
#include <map>
#include <set>
#include "library/text.hpp"

class disassembler
{
public:
	disassembler(const text& name);
	virtual ~disassembler();
	virtual text disassemble(uint64_t base, std::function<unsigned char()> fetchpc) = 0;
	static disassembler& byname(const text& name);
	static std::set<text> list();
	template<typename T> static T fetch_le(std::function<unsigned char()> fetchpc);
	template<typename T> static T fetch_be(std::function<unsigned char()> fetchpc);
private:
	text name;
	static std::map<text, disassembler*>& disasms();
};

#endif
