#ifndef _interface__disassembler__hpp__included__
#define _interface__disassembler__hpp__included__

#include <functional>
#include <string>
#include <map>
#include <set>

class disassembler
{
public:
	disassembler(const std::string& name);
	virtual ~disassembler();
	virtual std::string disassemble(uint64_t base, std::function<unsigned char()> fetchpc) = 0;
	static disassembler& byname(const std::string& name);
	static std::set<std::string> list();
	template<typename T> static T fetch_le(std::function<unsigned char()> fetchpc);
	template<typename T> static T fetch_be(std::function<unsigned char()> fetchpc);
private:
	std::string name;
	static std::map<std::string, disassembler*>& disasms();
};

#endif
