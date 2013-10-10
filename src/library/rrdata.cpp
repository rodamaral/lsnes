#include "rrdata.hpp"
#include <cstring>
#include <limits>
#include <cassert>

#define MAXRUN 16843009

namespace
{
	const char* hexes = "0123456789ABCDEF";
}

rrdata_set::instance::instance() throw()
{
	memset(bytes, 0, RRDATA_BYTES);
}

rrdata_set::instance::instance(const unsigned char* b) throw()
{
	memcpy(bytes, b, RRDATA_BYTES);
}

rrdata_set::instance::instance(const std::string& id) throw()
{
	memset(bytes, 0, RRDATA_BYTES);
	for(unsigned i = 0; i < id.length() && i < 2 * RRDATA_BYTES; i++) {
		unsigned h;
		char ch = id[i];
		if(ch >= '0' && ch <= '9')
			h = ch - '0';
		else if(ch >= 'A' && ch <= 'F')
			h = ch - 'A' + 10;
		else if(ch >= 'a' && ch <= 'f')
			h = ch - 'a' + 10;
		bytes[i / 2] = bytes[i / 2] * 16 + h;
	}
}

bool rrdata_set::instance::operator<(const struct instance& i) const throw()
{
	for(unsigned j = 0; j < RRDATA_BYTES; j++)
		if(bytes[j] < i.bytes[j])
			return true;
		else if(bytes[j] > i.bytes[j])
			return false;
	return false;
}

bool rrdata_set::instance::operator==(const struct instance& i) const throw()
{
	for(unsigned j = 0; j < RRDATA_BYTES; j++)
		if(bytes[j] != i.bytes[j])
			return false;
	return true;
}

const struct rrdata_set::instance rrdata_set::instance::operator++(int) throw()
{
	instance i = *this;
	++*this;
	return i;
}

struct rrdata_set::instance& rrdata_set::instance::operator++() throw()
{
	unsigned carry = 1;
	for(unsigned i = RRDATA_BYTES - 1; i < RRDATA_BYTES; i--) {
		unsigned newcarry = (bytes[i] == 255 && carry);
		bytes[i] += carry;
		carry = newcarry;
	}
	return *this;
}

struct rrdata_set::instance rrdata_set::instance::operator+(unsigned inc) const throw()
{
	rrdata_set::instance n = *this;
	unsigned carry = inc;
	for(unsigned i = RRDATA_BYTES - 1; i < RRDATA_BYTES; i--) {
		unsigned newcarry = ((unsigned)n.bytes[i] + carry) >> 8;
		if(newcarry == 0 && carry > 255)
			newcarry = (1U << (8 * sizeof(unsigned) - 8));
		n.bytes[i] += carry;
		carry = newcarry;
	}
	return n;
}

unsigned rrdata_set::instance::operator-(const struct instance& m) const throw()
{
	unsigned result = 0;
	uint8_t diff[RRDATA_BYTES] = {0};
	unsigned borrow = 0;
	for(unsigned i = RRDATA_BYTES - 1; i < RRDATA_BYTES; i--) {
		diff[i] = bytes[i] - m.bytes[i] - borrow;
		borrow = ((unsigned)m.bytes[i] + borrow > (unsigned)bytes[i]) ? 1 : 0;
	}
	for(unsigned i = 0; i < RRDATA_BYTES; i++) {
		if((result << 8 >> 8) != result)
			return std::numeric_limits<unsigned>::max();
		result <<= 8;
		result |= diff[i];
	}
	return result;
}

rrdata_set::rrdata_set() throw()
{
	rcount = 0;
	lazy_mode = false;
	handle_open = false;
}

void rrdata_set::read_base(const std::string& projectfile, bool lazy) throw(std::bad_alloc)
{
	if(projectfile == current_projectfile && (!lazy_mode || lazy))
		return;
	if(lazy) {
		std::set<std::pair<instance, instance>> new_rrset;
		data = new_rrset;
		current_projectfile = projectfile;
		lazy_mode = true;
		if(handle_open)
			ohandle.close();
		handle_open = false;
		return;
	}
	std::set<std::pair<instance, instance>> new_rrset;
	uint64_t new_count = 0;
	if(projectfile == current_projectfile) {
		new_rrset = data;
		new_count = rcount;
	}
	std::string filename = projectfile;
	if(handle_open) {
		ohandle.close();
		handle_open = false;
	}
	std::ifstream ihandle(filename.c_str(), std::ios_base::in | std::ios_base::binary);
	while(ihandle) {
		unsigned char bytes[RRDATA_BYTES];
		ihandle.read(reinterpret_cast<char*>(bytes), RRDATA_BYTES);
		instance k(bytes);
		//std::cerr << "Loaded symbol: " << k << std::endl;
		_add(k, k + 1, new_rrset, new_count);
	}
	ihandle.close();
	ohandle.open(filename.c_str(), std::ios_base::out | std::ios_base::app | std::ios_base::binary);
	if(ohandle)
		handle_open = true;
	if(projectfile == current_projectfile && lazy_mode && !lazy) {
		//Finish the project creation, write all.
		for(auto i : data) {
			instance tmp = i.first;
			while(tmp != i.second) {
				ohandle.write(reinterpret_cast<const char*>(tmp.bytes), RRDATA_BYTES);
				++tmp;
			}
			ohandle.flush();
		}
	}
	data = new_rrset;
	rcount = new_count;
	current_projectfile = projectfile;
	lazy_mode = lazy;
}

void rrdata_set::close() throw()
{
	current_projectfile = "";
	if(handle_open)
		ohandle.close();
	handle_open = false;
}

void rrdata_set::add(const struct rrdata_set::instance& i) throw(std::bad_alloc)
{
	if(_add(i) && handle_open) {
		//std::cerr << "New symbol: " << i << std::endl;
		ohandle.write(reinterpret_cast<const char*>(i.bytes), RRDATA_BYTES);
		ohandle.flush();
	}
}

void rrdata_set::add_internal() throw(std::bad_alloc)
{
	add(internal++);
}

namespace
{
	void flush_symbol(std::vector<char>& strm, const rrdata_set::instance& base,
		const rrdata_set::instance& predicted, unsigned count)
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
		for(size_t s = 0; s < (RRDATA_BYTES - j + 1) + (opcode >> 5); s++)
			strm.push_back(buf1[s]);
		//std::cerr << "Encoding " << count << " symbols starting from " << base << std::endl;
	}

	uint64_t symbols_in_interval(const rrdata_set::instance& b, const rrdata_set::instance& e) throw()
	{
		uint64_t c = 0;
		rrdata_set::instance x = b;
		while(x != e) {
			unsigned diff = e - x;
			x = x + diff;
			c = c + diff;
		}
		return c;
	}
}

uint64_t rrdata_set::write(std::vector<char>& strm) throw(std::bad_alloc)
{
	strm.clear();
	uint64_t scount = 0;
	instance last_encode_end;
	memset(last_encode_end.bytes, 0, RRDATA_BYTES);

	instance predicted;
	instance encode_base;
	unsigned encode_count = 0;
	for(auto i : data) {
		//std::cerr << "Considering " << *i << std::endl;
		encode_base = i.first;
		while(encode_base != i.second) {
			unsigned syms = i.second - encode_base;
			if(syms > MAXRUN)
				syms = MAXRUN;
			flush_symbol(strm, encode_base, predicted, syms);
			scount += syms;
			encode_base = encode_base + syms;
			predicted = encode_base;
		}
	}
	if(scount)
		return scount - 1;
	else
		return 0;
}

uint64_t rrdata_set::read(std::vector<char>& strm, bool dummy) throw(std::bad_alloc)
{
	uint64_t scount = 0;
	instance decoding;
	uint64_t ptr = 0;
	memset(decoding.bytes, 0, RRDATA_BYTES);
	while(ptr < strm.size()) {
		char opcode;
		unsigned char buf1[RRDATA_BYTES];
		unsigned char buf2[3];
		opcode = strm[ptr++];
		unsigned validbytes = (opcode & 0x1F);
		unsigned lengthbytes = (opcode & 0x60) >> 5;
		unsigned repeat = 1;
		memcpy(buf1, &strm[ptr], RRDATA_BYTES - validbytes);
		ptr += (RRDATA_BYTES - validbytes);
		memcpy(decoding.bytes + validbytes, buf1, RRDATA_BYTES - validbytes);
		if(lengthbytes > 0) {
			memcpy(buf2, &strm[ptr], lengthbytes);
			ptr += lengthbytes;
		}
		if(lengthbytes == 1)
			repeat = 2 + static_cast<unsigned>(buf2[0]);
		if(lengthbytes == 2)
			repeat = 258 + static_cast<unsigned>(buf2[0]) * 256 + buf2[1];
		if(lengthbytes == 3)
			repeat = 65794 + static_cast<unsigned>(buf2[0]) * 65536 + static_cast<unsigned>(buf2[1]) *
				256 + buf2[2];
		//std::cerr << "Decoding " << repeat << " symbols starting from " << decoding << std::endl;
		if(!dummy) {
			bool any = false;
			if(!_in_set(decoding, decoding + repeat))
				for(unsigned i = 0; i < repeat; i++) {
					//TODO: Optimize this.
					instance n = decoding + i;
					if(!_in_set(n) && handle_open) {
						ohandle.write(reinterpret_cast<const char*>(n.bytes), RRDATA_BYTES);
						any = true;
					}
				}
			if(any)
				ohandle.flush();
			rrdata_set::_add(decoding, decoding + repeat);
		}
		decoding = decoding + repeat;
		scount += repeat;
	}
	if(scount)
		return scount - 1;
	else
		return 0;
}

uint64_t rrdata_set::count(std::vector<char>& strm) throw(std::bad_alloc)
{
	return read(strm, true);
}

uint64_t rrdata_set::count() throw()
{
	uint64_t c = rcount;
	if(c)
		return c - 1;
	else
		return 0;
}

void rrdata_set::set_internal(const instance& b) throw()
{
	internal = b;
}

std::ostream& operator<<(std::ostream& os, const struct rrdata_set::instance& j)
{
	for(unsigned i = 0; i < 32; i++) {
		os << hexes[j.bytes[i] / 16] << hexes[j.bytes[i] % 16];
	}
	return os;
}

bool rrdata_set::_add(const instance& b)
{
	uint64_t c = rcount;
	_add(b, b + 1);
	return (c != rcount);
}

void rrdata_set::_add(const instance& b, const instance& e)
{
	_add(b, e, data, rcount);
}

void rrdata_set::_add(const instance& b, const instance& e, std::set<std::pair<instance, instance>>& set,
	uint64_t& cnt)
{
	//Special case: Nothing.
	if(set.empty()) {
		set.insert(std::make_pair(b, e));
		cnt += symbols_in_interval(b, e);
		return;
	}
	//Just insert it.
	auto itr = set.lower_bound(std::make_pair(b, e));
	if(itr != set.end() && itr->first == b && itr->second == e)
		return;
	set.insert(std::make_pair(b, e));
	cnt += symbols_in_interval(b, e);
	itr = set.lower_bound(std::make_pair(b, e));
	auto itr1 = itr;
	auto itr2 = itr;
	if(itr1 != set.begin()) itr1--;
	itr2++;
	bool have1 = (itr1 != itr);
	instance rangebase = b;
	//If the thing is entierely in itr1, undo the add.
	if(have1 && b >= itr1->first && e <= itr1->second) {
		cnt -= symbols_in_interval(b, e);
		set.erase(itr);
		return;
	}
	//Attach the thing to itr1 if appropriate.
	if(have1 && b <= itr1->second) {
		cnt -= symbols_in_interval(b, itr1->second);
		rangebase = itr1->first;
		set.insert(std::make_pair(itr1->first, e));
		auto tmp = set.lower_bound(std::make_pair(itr1->first, e));
		set.erase(itr1);
		set.erase(itr);
		itr = tmp;
		have1 = false;
	}
	while(itr2 != set.end()) {
		if(e < itr2->first)
			break;	//Nothing to merge anymore.
		if(e >= itr2->second && (rangebase != itr2->first || e != itr2->second)) {
			//This entiere range is subsumed.
			cnt -= symbols_in_interval(itr2->first, itr2->second);
			auto tmp = itr2;
			itr2++;
			set.erase(tmp);
		} else if(e < itr2->second) {
			//Combines with range.
			cnt -= symbols_in_interval(itr2->first, e);
			if(rangebase != itr2->first) {
				set.insert(std::make_pair(rangebase, itr2->second));
				set.erase(itr2);
			}
			set.erase(itr);
			break;
		}
	}
}

bool rrdata_set::_in_set(const instance& b, const instance& e)
{
	if(b == e)
		return true;
	if(data.empty())
		return false;
	auto itr = data.lower_bound(std::make_pair(b, e));
	if(itr == data.end()) {
		//If there is anything, it must be the last node.
		auto r = *data.rbegin();
		return (r.first <= b && r.second >= e);
	} else {
		//It may be this node or the previous one.
		if(itr->first <= b && itr->second >= e)
			return true;
		itr--;
		return (itr->first <= b && itr->second >= e);
	}
}

std::string rrdata_set::debug_dump()
{
	std::ostringstream x;
	x << rcount << "[";
	for(auto i : data)
		x << "{" << i.first << "," << i.second << "}";
	x << "]";
	return x.str();
}

uint64_t rrdata_set::debug_nodecount(std::set<std::pair<instance, instance>>& set)
{
	uint64_t x = 0;
	for(auto i : set)
		x += symbols_in_interval(i.first, i.second);
	return x;
}
