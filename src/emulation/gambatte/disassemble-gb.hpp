#ifndef _gambatte__disassemble_gb__hpp__included__
#define _gambatte__disassemble_gb__hpp__included__

#include <functional>
#include <cstdint>
#include <string>

std::string disassemble_gb_opcode(uint16_t pc, std::function<uint8_t()> fetchpc, int& addr, uint16_t& opcode);

#endif
