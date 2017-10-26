#include "assembler.hpp"
#include "serialization.hpp"
#include "string.hpp"
#include "hex.hpp"
#include <functional>
#include <cstring>
#include <iostream>
#include <fstream>
#if !defined(_WIN32) && !defined(_WIN64)
#define _USE_BSD
#include <unistd.h>
#include <sys/mman.h>
#else
#include <windows.h>
#endif

#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

namespace assembler
{
label_list::operator label&()
{
	labels.push_back(label());
	return labels.back();
}

label_list::operator label*()
{
	labels.push_back(label());
	return &labels.back();
}

label& label_list::external(void* addr)
{
	labels.push_back(label());
	labels.back() = label(addr);
	return labels.back();
}

assembler::assembler()
{
}

void assembler::_label(label& l)
{
	l.set(data.size());
}

void assembler::_label(label& l, const std::string& globalname)
{
	l.set(data.size());
	globals[globalname] = &l;
}

void assembler::byte(uint8_t b)
{
	data.push_back(b);
}

void assembler::byte(std::initializer_list<uint8_t> b)
{
	for(auto i : b)
		data.push_back(i);
}

void assembler::byte(const uint8_t* b, size_t l)
{
	for(size_t i = 0; i < l; i++)
		data.push_back(b[i]);
}

void assembler::relocation(std::function<void(uint8_t* location, size_t target, size_t source)> promise,
	const label& target)
{
	reloc r;
	r.promise = promise;
	r.target = &target;
	r.source = data.size();
	relocs.push_back(r);
}

void assembler::align(size_t multiple)
{
	if(!multiple)
		return;
	while(data.size() % multiple)
		data.push_back(0);
}

void assembler::pad(size_t amount)
{
	for(size_t i = 0; i < amount; i++)
		data.push_back(0);
}

size_t assembler::size()
{
	return data.size();
}

std::map<std::string, void*> assembler::flush(void* base)
{
	memcpy(base, &data[0], data.size());
	for(auto i : relocs) {
		i.promise((uint8_t*)base + i.source, i.target->resolve((addr_t)base), (addr_t)base + i.source);
	}
	std::map<std::string, void*> ret;
	for(auto i : globals) {
		ret[i.first] = reinterpret_cast<void*>(i.second->resolve((addr_t)base));
	}
	return ret;
}

void assembler::dump(const std::string& basename, const std::string& name, void* base,
	std::map<std::string, void*> map)
{
	static unsigned x = 0;
	std::string realbase = (stringfmt() << basename << "." << (x++)).str();
	//Dump symbol map.
	std::ofstream symbols1(realbase + ".syms");
	symbols1 << hex::to<size_t>((size_t)base, false) << " __base(" << name << ")" << std::endl;
	for(auto i : map)
		symbols1 << hex::to<size_t>((size_t)i.second, false) << " " << i.first << std::endl;
	symbols1 << hex::to<size_t>((size_t)base + data.size(), false) << " __end" << std::endl;
	symbols1.close();

	//Dump generated binary.
	std::ofstream symbols2(realbase + ".bin", std::ios::binary);
	for(unsigned i = 0; i < data.size(); i++)
		symbols2 << ((char*)base)[i];
	symbols2.close();
}

void i386_reloc_rel8(uint8_t* location, size_t target, size_t source)
{
	int8_t d = target - source - 1;
	if(source + d + 1 != target) {
		std::cerr << "Out-of-range offset: " << d << std::endl;
		throw std::runtime_error("Relative reference (8-bit) out of range");
	}
	serialization::s8l(location, d);
}

void i386_reloc_rel16(uint8_t* location, size_t target, size_t source)
{
	int16_t d = target - source - 2;
	if(source + d + 2 != target) {
		std::cerr << "Out-of-range offset: " << d << std::endl;
		throw std::runtime_error("Relative reference (16-bit) out of range");
	}
	serialization::s32l(location, d);
}

void i386_reloc_rel32(uint8_t* location, size_t target, size_t source)
{
	int32_t d = target - source - 4;
	if(source + d + 4 != target) {
		std::cerr << "Out-of-range offset: " << d << std::endl;
		throw std::runtime_error("Relative reference (32-bit) out of range");
	}
	serialization::s32l(location, d);
}

void i386_reloc_abs32(uint8_t* location, size_t target, size_t source)
{
	if(target > 0xFFFFFFFFU)
		throw std::runtime_error("Absolute reference out of range");
	serialization::u32l(location, target);
}

void i386_reloc_abs64(uint8_t* location, size_t target, size_t source)
{
	serialization::u64l(location, target);
}

uint8_t i386_modrm(uint8_t reg, uint8_t mod, uint8_t rm)
{
	return (mod & 3) * 64 + (reg & 7) * 8 + (rm & 7);
}

uint8_t i386_sib(uint8_t base, uint8_t index, uint8_t scale)
{
	return (scale & 3) * 64 + (index & 7) * 8 + (base & 7);
}


dynamic_code::dynamic_code(size_t size)
{
#if !defined(_WIN32) && !defined(_WIN64)
	asize = (size + getpagesize() - 1) / getpagesize() * getpagesize();
	base = (uint8_t*)mmap(NULL, asize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(base == MAP_FAILED)
		throw std::runtime_error("Failed to allocate memory for routine");
#else
	asize = (size + 4095) >> 12 << 12;  //Windows is always i386/amd64, right?
	base = VirtualAlloc(NULL, asize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if(!base)
		throw std::runtime_error("Failed to allocate memory for routine");
#endif
}

dynamic_code::~dynamic_code()
{
#if !defined(_WIN32) && !defined(_WIN64)
	munmap(base, asize);
#else
	VirtualFree(base, 0, MEM_RELEASE);
#endif
}

void dynamic_code::commit()
{
#if !defined(_WIN32) && !defined(_WIN64)
	if(mprotect(base, asize, PROT_READ | PROT_EXEC) < 0)
		throw std::runtime_error("Failed to mark routine as executable");
#else
	long unsigned dummy;
	if(!VirtualProtect(base, asize, PAGE_EXECUTE_READ, &dummy))
		throw std::runtime_error("Failed to mark routine as executable");
#endif
}

uint8_t* dynamic_code::pointer() throw()
{
	return reinterpret_cast<uint8_t*>(base);
}

}
