#include "rrdata.hpp"
#include <set>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "misc.hpp"

//
// XABCDEFXXXXXXXXX
// 0123456789XXXXXX
//
// ABCDEF0123456789XXXXXX

rrdata::instance::instance() throw(std::bad_alloc)
{
	std::string rnd = get_random_hexstring(2 * RRDATA_BYTES);
	memset(bytes, 0, RRDATA_BYTES);
	for(unsigned i = 0; i < 2 * RRDATA_BYTES; i++) {
		unsigned x = rnd[i];
		x = x & 0x1F;
		x = x - x / 16 * 9 - 1;
		bytes[i / 2] = 16 * bytes[i / 2] + x;
	}
}

rrdata::instance::instance(unsigned char* b) throw()
{
	memcpy(bytes, b, RRDATA_BYTES);
}

bool rrdata::instance::operator<(const struct instance& i) const throw()
{
	for(unsigned j = 0; j < RRDATA_BYTES; j++)
		if(bytes[j] < i.bytes[j])
			return true;
		else if(bytes[j] > i.bytes[j]) 
			return false;
	return false;
}

bool rrdata::instance::operator==(const struct instance& i) const throw()
{
	for(unsigned j = 0; j < RRDATA_BYTES; j++)
		if(bytes[j] != i.bytes[j])
			return false;
	return true;
}

const struct rrdata::instance rrdata::instance::operator++(int) throw()
{
	instance i = *this;
	++*this;
	return i;
}

const struct rrdata::instance& rrdata::instance::operator++() throw()
{
	unsigned carry = 1;
	for(unsigned i = 31; i < 32; i--) {
		unsigned newcarry = (bytes[i] == 255 && carry);
		bytes[i] += carry;
		carry = newcarry;
	}
}

namespace
{
	std::set<rrdata::instance> rrset;
	std::ifstream ihandle;
	std::ofstream ohandle;
	bool handle_open;
	std::string current_project;
}

void rrdata::read_base(const std::string& project) throw(std::bad_alloc)
{
	if(project == current_project)
		return;
	std::set<rrdata::instance> new_rrset;
	std::string filename = get_config_path(NULL) + "/" + project + ".rr";
	if(handle_open) {
		ohandle.close();
		handle_open = false;
	}
	ihandle.open(filename.c_str(), std::ios_base::in);
	while(ihandle) {
		unsigned char bytes[RRDATA_BYTES];
		ihandle.read(reinterpret_cast<char*>(bytes), RRDATA_BYTES);
		instance k(bytes);
		//std::cerr << "Loaded symbol: " << k << std::endl;
		new_rrset.insert(k);
	}
	ihandle.close();
	ohandle.open(filename.c_str(), std::ios_base::out | std::ios_base::app);
	if(ohandle)
		handle_open = true;
	rrset = new_rrset;
	current_project = project;
}

void rrdata::close() throw()
{
	current_project = "";
	if(handle_open)
		ohandle.close();
	handle_open = false;
}

void rrdata::add(const struct rrdata::instance& i) throw(std::bad_alloc)
{
	if(rrset.insert(i).second) {
		//std::cerr << "New symbol: " << i << std::endl;
		ohandle.write(reinterpret_cast<const char*>(i.bytes), RRDATA_BYTES);
		ohandle.flush();
	}
}

void rrdata::add_internal() throw(std::bad_alloc)
{
	if(!internal)
		internal = new instance();
	add((*internal)++);
}

namespace
{
	void flush_symbol(std::ostream& strm, const rrdata::instance& base, const rrdata::instance& predicted,
		unsigned count)
	{
		char opcode;
		char buf1[RRDATA_BYTES + 4];
		char buf2[3];
		unsigned bias;
		if(count == 1) {
		 	opcode = 0x00;
			bias = 1;
		} else if(count < 258) {
			opcode = 0x20;
			bias = 2;
		} else if(count < 65794) {
			opcode = 0x40;
			bias = 258;
		} else {
			opcode = 0x60;
			bias = 65794;
		}
		unsigned j;
		for(j = 0; j < 31; j++)
			if(base.bytes[j] != predicted.bytes[j])
				break;
		opcode += j;
		buf1[0] = opcode;
		memcpy(buf1 + 1, base.bytes + j, RRDATA_BYTES - j);
		buf2[0] = (count - bias) >> 16;
		buf2[1] = (count - bias) >> 8;
		buf2[2] = (count - bias);
		memcpy(buf1 + (RRDATA_BYTES - j + 1), buf2 + (3 - (opcode >> 5)), opcode >> 5);
		strm.write(buf1, (RRDATA_BYTES - j + 1) + (opcode >> 5));
		//std::cerr << "Encoding " << count << " symbols starting from " << base << std::endl;
	}
}

uint64_t rrdata::write(std::ostream& strm) throw(std::bad_alloc)
{
	uint64_t count = 0;
	instance last_encode_end;
	memset(last_encode_end.bytes, 0, RRDATA_BYTES);

	instance predicted;
	instance encode_base;
	unsigned encode_count = 0;
	bool first = true;
	for(auto i = rrset.begin(); i != rrset.end(); i++) {
		//std::cerr << "Considering " << *i << std::endl;
		count++;
		if(encode_count == 0) {
			//This is the first symbol.
			encode_base = *i;
			encode_count = 1;
		} else if(predicted == *i && encode_count < 16843009) {
			//Correct prediction.
			encode_count++;
		} else {
			//Failed prediction
			flush_symbol(strm, encode_base, last_encode_end, encode_count);
			last_encode_end = predicted;
			encode_base = *i;
			encode_count = 1;
		}
		predicted = *i;
		++predicted;
	}
	if(encode_count > 0)
		flush_symbol(strm, encode_base, last_encode_end, encode_count);
	return count;
}

uint64_t rrdata::read(std::istream& strm) throw(std::bad_alloc)
{
	uint64_t count = 0;
	instance decoding;
	memset(decoding.bytes, 0, RRDATA_BYTES);
	while(strm) {
		char opcode;
		char buf1[RRDATA_BYTES];
		char buf2[3];
		strm.read(&opcode, 1);
		if(!strm)
			continue;
		unsigned validbytes = (opcode & 0x1F);
		unsigned lengthbytes = (opcode & 0x60) >> 5;
		unsigned repeat = 1;
		strm.read(buf1, RRDATA_BYTES - validbytes);
		memcpy(decoding.bytes + validbytes, buf1, RRDATA_BYTES - validbytes);
		if(lengthbytes > 0)
			strm.read(buf2, lengthbytes);
		if(lengthbytes == 1)
			repeat = 2 + buf2[0];
		if(lengthbytes == 2)
			repeat = 258 + static_cast<unsigned>(buf2[0]) * 256 + buf2[1];
		if(lengthbytes == 3)
			repeat = 65794 + static_cast<unsigned>(buf2[0]) * 65536 + static_cast<unsigned>(buf2[1]) *
				256 + buf2[2];
		//std::cerr << "Decoding " << count << " symbols starting from " << decoding << std::endl;
		for(unsigned i = 0; i < repeat; i++)
			rrdata::add(decoding++);
		count += repeat;
	}
	return count;
}

const char* hexes = "0123456789ABCDEF";

std::ostream& operator<<(std::ostream& os, const struct rrdata::instance& j)
{
	for(unsigned i = 0; i < 32; i++) {
		os << hexes[j.bytes[i] / 16] << hexes[j.bytes[i] % 16];
	}
	return os;
}

rrdata::instance* rrdata::internal;


//DBC0AB8CBAAC6ED4B7781E34057891E8B9D93AAE733DEF764C06957FF705DE00
//DBC0AB8CBAAC6ED4B7781E34057891E8B9D93AAE733DEF764C06957FF705DDF3