#include "cbor.hpp"
#include <cstring>
#include <iostream>
#include <functional>

#define BITS_MASK(X) ((1ULL << (X))-1)
#define BITS_VAL(X) (1ULL << (X))

namespace CBOR
{
thread_local std::function<void(item*)> dtor_cb;
namespace
{
	//Take half-float and expand it into double-float.
	uint64_t expand_half_fp(uint16_t x)
	{
		uint16_t sign = x >> 15;
		uint16_t exp = (x >> 10) & BITS_MASK(5);
		uint16_t mantissa = x & BITS_MASK(10);
		//Adjust exponent.
		switch(exp) {
		case 0:
			if(mantissa != 0) {
				exp = 1009;
				while(mantissa < BITS_VAL(10)) {
					mantissa <<= 1;
					exp--;
				}
				mantissa &= BITS_MASK(10);
			}
			break;
		case 31: exp = 2047; break;
		default: exp += 1008; break;
		}
		return ((uint64_t)sign << 63) | ((uint64_t)exp << 52) | ((uint64_t)mantissa << 42);
	}

	//Take single-float and expand it to double-float.
	uint64_t expand_single_fp(uint32_t x)
	{
		uint32_t sign = x >> 31;
		uint32_t exp = (x >> 23) & BITS_MASK(8);
		uint32_t mantissa = x & BITS_MASK(23);
		//Adjust exponent.
		switch(exp) {
		case 0:
			if(mantissa != 0) {
				exp = 897;
				while(mantissa < BITS_VAL(23)) {
					mantissa <<= 1;
					exp--;
				}
				mantissa &= BITS_MASK(23);
			}
			break;
		case 255: exp = 2047; break;
		default: exp += 896; break;
		}
		return ((uint64_t)sign << 63) | ((uint64_t)exp << 52) | ((uint64_t)mantissa << 29);
	}

	//Is the double-float exactly expressible as half-float?
	bool is_half_fp(uint64_t x)
	{
		uint64_t exp = (x >> 52) & BITS_MASK(11);
		uint64_t mantissa = x & BITS_MASK(52);
		if(exp == 2047)
			return true;	//Infinities and NaNs.
		if((mantissa & BITS_MASK(42)) != 0)
			return false;	//Mantissa has too much precision.
		if(exp >= 999 && exp <= 1008) {
			//Denormals in half-fp.
			uint64_t tbits = 1051 - exp;
			return (mantissa & BITS_MASK(tbits)) == 0;
		}
		//Check exponent in range. 0 is zero.
		if(exp > 0 && (exp < 1009 || exp > 1038))
			return false;
		return true;
	}

	//Is the double-float exactly expressible as single-float?
	bool is_single_fp(uint64_t x)
	{
		uint64_t exp = (x >> 52) & BITS_MASK(11);
		uint64_t mantissa = x & BITS_MASK(52);
		//The following probably isn't reachable, because half-fp is tested first, and it accepts all
		//exp 2047 values.
		if(exp == 2047)
			return true;	//Infinities and NaNs.
		if((mantissa & BITS_MASK(29)) != 0)
			return false;	//Mantissa has too much precision.
		if(exp >= 874 && exp <= 896) {
			//Denormals in single-fp.
			uint64_t tbits = 926 - exp;
			return (mantissa & BITS_MASK(tbits)) == 0;
		}
		//Check exponent in range. 0 is zero.
		if(exp > 0 && (exp < 897 || exp > 1150))
			return false;
		return true;
	}

	//Take double-float and compress it to half-float. Only valid if exactly expressible.
	uint16_t compress_half_fp(uint64_t x)
	{
		uint64_t sign = x >> 63;
		uint64_t exp = (x >> 52) & BITS_MASK(11);
		uint64_t mantissa = x & BITS_MASK(52);
		//Adjust exponent.
		if(exp == 0) {
			//Nothing to do.
		} else if(exp == 2047) {
			//Inf or NaN.
			exp = 31;
			if(mantissa != 0) {
				//Canonicalize NaN.
				mantissa = BITS_VAL(51);
				sign = 0;
			}
		} else if(exp >= 999 && exp <= 1008) {
			//Denormals.
			mantissa = (mantissa + BITS_VAL(52)) >> (1009 - exp);
			exp = 0;
		} else {
			//Normal.
			exp -= 1008;
		}
		return ((sign << 15) | (exp << 10) | (mantissa >> 42));
	}

	//Take double-float and compress it to single-float. Only valid if exactly expressible.
	uint32_t compress_single_fp(uint64_t x)
	{
		uint64_t sign = x >> 63;
		uint64_t exp = (x >> 52) & BITS_MASK(11);
		uint64_t mantissa = x & BITS_MASK(52);
		//Adjust exponent.
		if(exp == 0) {
			//Nothing to do.
		} else if(exp == 2047) {
			//Probably not reachable, because half-fp is tested first and grabs all these cases.
			//Inf or NaN.
			exp = 255;
			if(mantissa != 0) {
				//Canonicalize NaN.
				mantissa = BITS_VAL(51);
				sign = 0;
			}
		} else if(exp >= 874 && exp <= 896) {
			//Denormals.
			mantissa = (mantissa + BITS_VAL(52)) >> (897 - exp);
			exp = 0;
		} else {
			//Normal.
			exp -= 896;
		}
		return ((sign << 31) | (exp << 23) | (mantissa >> 29));
	}

	//Get minor for given argument.
	uint8_t minor_for(uint64_t arg)
	{
		if(arg >> 32) return 27;
		if(arg >> 16) return 26;
		if(arg >> 8) return 25;
		if(arg > 23) return 24;
		return arg;
	}

	//Get size of argument,
	size_t argument_size(uint64_t arg)
	{
		if(arg >> 32)
			return 8;
		if(arg >> 16)
			return 4;
		if(arg >> 8)
			return 2;
		if(arg > 23)
			return 1;
		return 0;
	}

	//Write argument using specified number of bytes.
	template<size_t size> void _write_arg(char*& out, uint64_t arg)
	{
		for(unsigned i = 1; i <= size; i++)
			*(out++) = arg >> ((size- i) << 3);
	}

	//Write argument using least number of bytes.
	void write_arg(char*& out, uint64_t arg) {
		if(arg >> 32)
			_write_arg<8>(out, arg);
		else if(arg >> 16)
			_write_arg<4>(out, arg);
		else if(arg >> 8)
			_write_arg<2>(out, arg);
		else if(arg > 23)
			_write_arg<1>(out, arg);
		//Arguments 0-23 are given as minors.
	}

	//Get minor for float argument.
	uint8_t minor_for_float(uint64_t arg)
	{
		if(is_half_fp(arg))
			return 25;
		if(is_single_fp(arg))
			return 26;
		return 27;
	}

	//Get size of float argument,
	size_t argument_size_float(uint64_t arg)
	{
		if(is_half_fp(arg))
			return 2;
		if(is_single_fp(arg))
			return 4;
		return 8;
	}

	//Write float argument.
	void write_arg_float(char*& out, uint64_t arg)
	{
		if(is_half_fp(arg))
			return _write_arg<2>(out, compress_half_fp(arg));
		if(is_single_fp(arg))
			return _write_arg<4>(out, compress_single_fp(arg));
		return _write_arg<8>(out, arg);
	}

	//Compress float.
	uint64_t compress_float(uint64_t arg)
	{
		if(is_half_fp(arg))
			return compress_half_fp(arg);
		if(is_single_fp(arg))
			return compress_single_fp(arg);
		return arg;
	}

	//Get next byte.
	uint8_t nextbyte(const char* b, size_t& p, size_t s)
	{
		if(p >= s)
			throw std::runtime_error("Invalid CBOR: Truncated");
		return b[p++];
	}

	//Peek next byte.
	uint8_t nextbyte_peek(const char* b, size_t& p, size_t s)
	{
		if(p >= s)
			throw std::runtime_error("Invalid CBOR: Truncated");
		return b[p];
	}

	//Read argument.
	uint64_t read_argument(const char* b, size_t& p, size_t s, uint8_t minor)
	{
		const static unsigned limit[8] = {24, 25, 26, 26, 27, 27, 27, 27};
		if(minor < 24) return minor;
		if(minor > 27) return 0;
		uint64_t arg = 0;
		for(unsigned i = 0; i < 8; i++) 
			if(minor >= limit[i]) arg = (arg << 8) | nextbyte(b, p, s);
		return arg;
	}

	//Check utf-8 validity.
	template<typename T>
	bool _check_utf8(T begin, T end) throw()
	{
		unsigned state = 0;
		for(T i = begin; i != end; ++i) {
			unsigned char ch = *i;
			if(state && (ch & 0xC0) != 0x80)
				return false;	//Bad continue.
			switch(state) {
			case 0:		//INIT.
				if(ch < 0x80)
					state = 0;
				else if(ch < 0xC2)
					return false;			//Continues, C0 and C1 are always overlong.
				else if(ch < 0xE0)
					state = 1;
				else if(ch == 0xE0)
					state = 6;
				else if(ch == 0xED)
					state = 7;
				else if(ch < 0xEF)
					state = 4;
				else if(ch == 0xEF)
					state = 8;
				else if(ch == 0xF0)
					state = 10;
				else if(ch < 0xF4)
					state = 9;
				else if(ch == 0xF4)
					state = 11;
				else
					return false;
				break;
			case 1:		//ANY 1 CONTINUE.
				state = 0;
				break;
			case 2:		//1 CONTINUE AT END OF PLANE
				if(ch > 0xBD)
					return false;	//xFFFE and xFFFF are not valid.
				state = 0;
				break;
			case 3:		//1 CONTINUE after EFB7.
				if(ch >= 0x90 && ch < 0xB0)
					return false;	//noncharacters.
				state = 0;
				break;
			case 4:		//ANY 2 CONTINUES.
				state = 1;
				break;
			case 5:		//2 CONTINUES AND END OF PLANE.
				if(ch == 0xBF)
					state = 2;
				else
					state = 1;
				break;
			case 6:		//2 CONTINUES AFTER E0.
				if(ch < 0xA0)
					return false;	//Overlong.
				state = 1;
				break;
			case 7:		//2 CONTINUES AFTER ED.
				if(ch >= 0xA0)
					return false;	//Surrogates.
				state = 1;
				break;
			case 8:		//2 CONTINUES AFTER EF.
				if(ch == 0xB7)
					state = 3;
				else if(ch == 0xBF)
					state = 2;
				else
					state = 1;
				break;
			case 9:		//3 CONTINUES.
				if((ch & 0xF) == 15)
					state = 5;
				else
					state = 4;
				break;
			case 10:	//3 CONTINUES after F0.
				if(ch < 0x90)
					return false;	//Overlong.
				if((ch & 0xF) == 15)
					state = 5;
				else
					state = 4;
				break;
			case 11:	//3 CONTINUES after F4.
				if(ch >= 0x90)
					return false;	//Plane out of range.
				if((ch & 0xF) == 15)
					state = 5;
				else
					state = 4;
				break;
			}
		}
		return (state == 0);
	}

	//Check utf-8 validity.
	template<typename T>
	void check_utf8(T begin, T end) throw(std::out_of_range)
	{
		if(!_check_utf8(begin, end))
			throw std::out_of_range("Invalid UTF-8");
	}
}

item::_unsigned_tag integer;
item::_negative_tag negative;
item::_octets_tag octets;
item::_string_tag string;
item::_array_tag array;
item::_map_tag map;
item::_tag_tag tag;
item::_simple_tag simple;
item::_boolean_tag boolean;
item::_null_tag null;
item::_undefined_tag undefined;
item::_float_tag floating;

item::item() throw()
{
	tag = T_UNSIGNED;
	value.integer = 0;
}

item::item(const item& tocopy) throw(std::bad_alloc)
	: item()
{
	*this = tocopy;
}

item& item::operator=(const item& tocopy) throw(std::bad_alloc)
{
	if(this == &tocopy) return *this;
	tag = tocopy.tag;
	switch(tag) {
	case T_UNSIGNED:
	case T_NEGATIVE:
	case T_SIMPLE:
	case T_BOOLEAN:
	case T_NULL:
	case T_UNDEFINED:
	case T_FLOAT:
		value.integer = tocopy.value.integer;
		break;
	case T_OCTETS:
		value.pointer = inner_copy<octets_t>(tocopy.value.pointer);
		break;
	case T_STRING:
		value.pointer = inner_copy<string_t>(tocopy.value.pointer);
		break;
	case T_ARRAY:
		value.pointer = inner_copy<array_t>(tocopy.value.pointer);
		break;
	case T_MAP:
		value.pointer = inner_copy<map_t>(tocopy.value.pointer);
		break;
	case T_TAG:
		value.pointer = inner_copy<tag_t>(tocopy.value.pointer);
		break;
	}
	return *this;
}

item::~item()
{
	if(dtor_cb) dtor_cb(this);
	release_inner();
}

uint64_t item::get_size() const throw()
{
	switch(tag) {
	case T_UNSIGNED: return 1 + argument_size(value.integer);
	case T_NEGATIVE: return 1 + argument_size(value.integer);
	case T_SIMPLE: return 1 + argument_size(value.integer);
	case T_BOOLEAN: return 1;
	case T_NULL: return 1;
	case T_UNDEFINED: return 1;
	case T_FLOAT: return 1 + argument_size_float(value.integer);
	case T_OCTETS: return 1 + argument_size(inner_as<octets_t>().size()) + inner_as<octets_t>().size();
	case T_STRING: return 1 + argument_size(inner_as<string_t>().length()) + inner_as<string_t>().length();
	case T_ARRAY: {
		auto& obj = inner_as<array_t>();
		uint64_t x = 1 + argument_size(obj.size());
		for(auto& i : obj) x += i.second.get_size();
		return x;
	}
	case T_MAP: {
		auto& obj = inner_as<map_t>();
		uint64_t x = 1 + argument_size(obj.size());
		for(auto& i : obj) x += i.first.get_size() + i.second.get_size();
		return x;
	}
	case T_TAG: return 1 + argument_size(inner_as<tag_t>().first) + inner_as<tag_t>().second.get_size();
	}
	return 0;
}

std::vector<char> item::serialize() const throw(std::bad_alloc)
{
	std::vector<char> out;
	out.resize(get_size());
	char* ptr = &out[0];
	_serialize(ptr);
	return out;
}

item::item(const char* buffer, size_t buffersize) throw(std::bad_alloc, std::runtime_error)
{
	size_t ptr = 0;
	ctor(buffer, ptr, buffersize);
}

item::item(_unsigned_tag _tag, uint64_t _value) throw()
{
	tag = T_UNSIGNED;
	value.integer = _value;
}

item::item(_negative_tag _tag, uint64_t _value) throw()
{
	tag = T_NEGATIVE;
	value.integer = _value;
}

item::item(_octets_tag _tag, const std::vector<uint8_t>& _value) throw(std::bad_alloc)
{
	tag = T_OCTETS;
	value.pointer = new octets_t(_value);
}

item::item(_string_tag _tag, const std::string& _value) throw(std::bad_alloc, std::out_of_range)
{
	check_utf8(_value.begin(), _value.end());
	tag = T_STRING;
	value.pointer = new string_t(_value);
}

item::item(_array_tag _tag) throw(std::bad_alloc)
{
	tag = T_ARRAY;
	value.pointer = new array_t;
}

item::item(_map_tag _tag) throw(std::bad_alloc)
{
	tag = T_MAP;
	value.pointer = new map_t;
}

item::item(_float_tag _tag, double _value) throw()
{
	tag = T_FLOAT;
	value.integer = *id(&_value);
}

item::item(_boolean_tag _tag, bool _value) throw()
{
	tag = T_BOOLEAN;
	value.integer = _value ? 1 : 0;
}

item::item(_simple_tag _tag, uint8_t _value) throw(std::out_of_range)
{
	if(_value > 23 && _value < 32)
		throw std::out_of_range("CBOR simple values 23-31 are not valid");
	switch(_value) {
	case 20: tag = T_BOOLEAN; value.integer = 0; break;
	case 21: tag = T_BOOLEAN; value.integer = 1; break;
	case 22: tag = T_NULL; value.integer = 0; break;
	case 23: tag = T_UNDEFINED; value.integer = 0; break;
	default: tag = T_SIMPLE; value.integer = _value; break;
	}
}

item::item(_null_tag _tag) throw()
{
	tag = T_NULL;
	value.integer = 0;
}

item::item(_undefined_tag _tag) throw()
{
	tag = T_UNDEFINED;
	value.integer = 0;
}

item::item(_tag_tag _tag, uint64_t _value, const item& _inner) throw(std::bad_alloc)
{
	tag_t* tmp = new tag_t;
	tag = T_TAG;
	value.pointer = tmp;
	tmp->first = _value;
	try {
		tmp->second = _inner;
	} catch(...) {
		delete tmp;
		throw;
	}
}

uint64_t item::get_unsigned() const throw(std::domain_error)
{
	check_unsigned();
	return value.integer;
}

void item::set_unsigned(uint64_t _value) throw()
{
	replace_inner(T_UNSIGNED, _value);
}

uint64_t item::get_negative() const throw(std::domain_error)
{
	check_negative();
	return value.integer;
}

void item::set_negative(uint64_t _value) throw()
{
	replace_inner(T_NEGATIVE, _value);
}

const std::vector<uint8_t>& item::get_octets() const throw(std::domain_error)
{
	check_octets();
	return inner_as<octets_t>();
}

void item::set_octets(const std::vector<uint8_t>& _value) throw(std::bad_alloc)
{
	octets_t* tmp = new octets_t(_value);
	replace_inner(T_OCTETS, tmp);
}

void item::set_octets(const uint8_t* _value, size_t _valuelen) throw(std::bad_alloc)
{
	octets_t* tmp = new octets_t(_value, _value + _valuelen);
	replace_inner(T_OCTETS, tmp);
}

const std::string& item::get_string() const throw(std::domain_error)
{
	check_string();
	return inner_as<string_t>();
}

void item::set_string(const std::string& _value) throw(std::bad_alloc, std::out_of_range)
{
	check_utf8(_value.begin(), _value.end());
	string_t* tmp = new string_t(_value);
	replace_inner(T_STRING, tmp);
}

void item::set_array() throw(std::bad_alloc)
{
	replace_inner(T_ARRAY, new array_t);
}

size_t item::get_array_size() const throw(std::domain_error)
{
	check_array();
	return inner_as<array_t>().size();
}

void item::set_array_size(size_t newsize) throw(std::bad_alloc, std::domain_error)
{
	check_array();
	array_t& tmp = inner_as<array_t>();
	size_t oldsize = tmp.size();
	if(newsize < oldsize) {
		for(size_t i = newsize; i < oldsize; i++)
			tmp.erase(i);
	} else if(newsize > oldsize) {
		try {
			for(size_t i = oldsize; i < newsize; i++)
				tmp[i];
		} catch(...) {
			size_t bound = tmp.size();
			for(size_t i = oldsize; i < bound; i++)
				tmp.erase(i);
			throw;
		}
	}
}

const item& item::operator[](size_t index) const throw(std::domain_error, std::out_of_range)
{
	check_array();
	const array_t& tmp = inner_as<array_t>();
	if(!tmp.count(index))
		throw std::out_of_range("Index not found in array");
	return tmp.find(index)->second;
}

item& item::operator[](size_t index) throw(std::domain_error, std::out_of_range)
{
	check_array();
	array_t& tmp = inner_as<array_t>();
	if(!tmp.count(index))
		throw std::out_of_range("Index not found in array");
	return tmp.find(index)->second;
}

void item::set_map() throw(std::bad_alloc)
{
	replace_inner(T_MAP, new map_t);
}

size_t item::get_map_size() const throw(std::domain_error)
{
	check_map();
	return inner_as<map_t>().size();
}

const item& item::get_map_lookup(const item& index) const throw(std::domain_error, std::out_of_range)
{
	check_map();
	const map_t& tmp = inner_as<map_t>();
	if(!tmp.count(index))
		throw std::out_of_range("Key not found in map");
	return tmp.find(index)->second;
}

item& item::operator[](const item& index) throw(std::bad_alloc, std::domain_error)
{
	check_map();
	return inner_as<map_t>()[index];
}

std::map<item, item>::iterator item::get_map_begin() throw(std::domain_error)
{
	check_map();
	return inner_as<map_t>().begin();
}

std::map<item, item>::iterator item::get_map_end() throw(std::domain_error)
{
	check_map();
	return inner_as<map_t>().end();
}

std::map<item, item>::const_iterator item::get_map_begin() const throw(std::domain_error)
{
	check_map();
	return inner_as<map_t>().begin();
}

std::map<item, item>::const_iterator item::get_map_end() const throw(std::domain_error)
{
	check_map();
	return inner_as<map_t>().end();
}

uint64_t item::get_tag_number() const throw(std::domain_error)
{
	check_tag();
	return inner_as<tag_t>().first;
}

void item::set_tag_number(uint64_t _value) throw(std::domain_error)
{
	check_tag();
	inner_as<tag_t>().first = _value;
}

const item& item::get_tag_inner() const throw(std::domain_error)
{
	check_tag();
	return inner_as<tag_t>().second;
}

item& item::get_tag_inner() throw(std::domain_error)
{
	check_tag();
	return inner_as<tag_t>().second;
}

void item::tag_item(uint64_t _value) throw(std::bad_alloc)
{
	tag_t* tmp = new tag_t;
	tmp->first = _value;
	tmp->second.tag = tag;
	memcpy(&tmp->second.value, &value, sizeof(value));
	//Overwrite values, since ownership was transferred.
	tag = T_TAG;
	value.pointer = tmp;
}

void item::detag_item() throw(std::domain_error)
{
	check_tag();
	tag_t& tmp = inner_as<tag_t>();
	tag = tmp.second.tag;
	memcpy(&value, &tmp.second.value, sizeof(value));
	//Set it to default value without freeing, since ownership was transferred. But free the TAG.
	tmp.second.tag = T_UNSIGNED;
	tmp.second.value.integer = 0;
	delete &tmp;
}

double item::get_float() const throw(std::domain_error)
{
	check_float();
	return *idinv(&value.integer);
}

void item::_set_float(uint64_t _value) throw()
{
	replace_inner(T_FLOAT, _value);
}

uint8_t item::get_simple() const throw(std::domain_error)
{
	switch(tag) {
	case T_SIMPLE: return value.integer;
	case T_BOOLEAN: return value.integer ? 21 : 20;
	case T_NULL: return 22;
	case T_UNDEFINED: return 23;
	default: throw std::domain_error("Expected SIMPLE, BOOLEAN, NULL or UNDEFINED");
	}
}

void item::set_simple(uint8_t _value) throw(std::out_of_range)
{
	if(_value > 23 && _value < 32)
		throw std::out_of_range("CBOR simple values 23-31 are not valid");
	tag_type tag = T_SIMPLE;
	uint64_t val = _value;
	switch(_value) {
	case 20: tag = T_BOOLEAN; val = 0; break;
	case 21: tag = T_BOOLEAN; val = 1; break;
	case 22: tag = T_NULL; val = 0; break;
	case 23: tag = T_UNDEFINED; val = 0; break;
	}
	replace_inner(tag, val);
}

bool item::get_boolean() const throw(std::domain_error)
{
	check_boolean();
	return value.integer;
}

void item::set_boolean(bool _value) throw()
{
	replace_inner(T_BOOLEAN, _value ? 1 : 0);
}

void item::set_null() throw()
{
	replace_inner(T_NULL, (uint64_t)0);
}

void item::set_undefined() throw()
{
	replace_inner(T_UNDEFINED, (uint64_t)0);
}

void item::release_inner() throw()
{
	switch(tag) {
	case T_UNSIGNED:
	case T_NEGATIVE:
	case T_SIMPLE:
	case T_BOOLEAN:
	case T_NULL:
	case T_UNDEFINED:
	case T_FLOAT:
		break;		//These do not have pointers.
	case T_OCTETS:
		delete &inner_as<octets_t>();
		break;
	case T_STRING:
		delete &inner_as<string_t>();
		break;
	case T_ARRAY:
		delete &inner_as<array_t>();
		break;
	case T_MAP:
		delete &inner_as<map_t>();
		break;
	case T_TAG:
		delete &inner_as<tag_t>();
		break;
	}
}

void item::replace_inner(tag_type _tag, uint64_t _integer) throw()
{
	release_inner();
	tag = _tag;
	value.integer = _integer;
}

void item::replace_inner(tag_type _tag, void* _pointer) throw()
{
	release_inner();
	tag = _tag;
	value.pointer = _pointer;
}

void item::swap(item& a) throw()
{
	char* ptr1 = reinterpret_cast<char*>(this);
	char* ptr2 = reinterpret_cast<char*>(&a);
	for(size_t i = 0; i < sizeof(item); i++) {
		ptr1[i] ^= ptr2[i];
		ptr2[i] ^= ptr1[i];
		ptr1[i] ^= ptr2[i];
	}
}

void item::ctor(const char* b, size_t& p, size_t s)
{
	//Note: 7:31 is not marked as valid, because it is only for special conditions.
	const static uint32_t valid_opcodes[] = {
		0x0FFFFFFFU, 0x0FFFFFFFU, 0x8FFFFFFFU, 0x8FFFFFFFU,
		0x8FFFFFFFU, 0x8FFFFFFFU, 0x0FFFFFFFU, 0x0FFFFFFFU,
	};
	uint8_t tbyte = nextbyte(b, p, s);
	uint8_t major = tbyte >> 5;
	uint8_t minor = tbyte & BITS_MASK(5);
	if((valid_opcodes[major] >> minor & 1) == 0) throw std::runtime_error("Invalid CBOR: Bad opcode");
	tag = T_UNSIGNED;
	value.integer = 0;
	switch(major) {
	case 0:
		tag = T_UNSIGNED;
		value.integer = read_argument(b, p, s, minor);
		break;
	case 1:
		tag = T_NEGATIVE;
		value.integer = read_argument(b, p, s, minor);
		break;
	case 2: {
		octets_t* tmp = new octets_t;
		try {
			if(minor == 31) {
				uint8_t tbyte2;
				while((tbyte2 = nextbyte(b, p, s)) != 255) {
					if((tbyte2 >> 5) != major)
						throw std::runtime_error("Invalid CBOR: Bad fragment");
					uint8_t minor2 = tbyte2 & BITS_MASK(5);
					uint64_t c = read_argument(b, p, s, minor2);
					if(c > s || c + p > s)
						throw std::runtime_error("Invalid CBOR: Truncated");
					size_t origsize = tmp->size();
					tmp->resize(origsize + c);
					std::copy(b + p, b + p + c, tmp->begin() + origsize); 
				}
			} else {
				uint64_t c = read_argument(b, p, s, minor);
				if(c > s || c + p > s)
					throw std::runtime_error("Invalid CBOR: Truncated");
				tmp->resize(c);
				std::copy(b + p, b + p + c, tmp->begin());
			}
			tag = T_OCTETS;
			value.pointer = tmp;
		} catch(...) {
			delete tmp;
			throw;
		}
		break;
	}
	case 3: {
		string_t* tmp = new string_t;
		try {
			if(minor == 31) {
				uint8_t tbyte2;
				while((tbyte2 = nextbyte(b, p, s)) != 255) {
					if((tbyte2 >> 5) != major)
						throw std::runtime_error("Invalid CBOR: Bad fragment");
					uint8_t minor2 = tbyte2 & BITS_MASK(5);
					uint64_t c = read_argument(b, p, s, minor2);
					if(c > s || c + p > s)
						throw std::runtime_error("Invalid CBOR: Truncated");
					if(!_check_utf8(b + p, b + p + c))
						throw std::runtime_error("Invalid CBOR: Bad UTF-8");
					size_t origsize = tmp->size();
					tmp->resize(origsize + c);
					std::copy(b + p, b + p + c, tmp->begin() + origsize); 
				}
			} else {
				uint64_t c = read_argument(b, p, s, minor);
				if(c > s || c + p > s)
					throw std::runtime_error("Invalid CBOR: Truncated");
				if(!_check_utf8(b + p, b + p + c))
					throw std::runtime_error("Invalid CBOR: Bad UTF-8");
				tmp->resize(c);
				std::copy(b + p, b + p + c, tmp->begin());
			}
			tag = T_STRING;
			value.pointer = tmp;
		} catch(...) {
			delete tmp;
			throw;
		}
		break;
	}
	case 4: {
		array_t* tmp = new array_t;
		try {
			size_t count = (uint64_t)-1;
			if(minor < 31)
				count = read_argument(b, p, s, minor);
			for(size_t i = 0; i < count && (minor < 31 || nextbyte_peek(b, p, s) != 255); i++)
				(*tmp)[i].ctor(b, p, s);
			if(minor == 31)
				nextbyte(b, p, s);	//Dump the 0xFF.
			tag = T_ARRAY;
			value.pointer = tmp;
		} catch(...) {
			delete tmp;
			throw;
		}
		break;
	}
	case 5: {
		map_t* tmp = new map_t;
		try {
			size_t count = (uint64_t)-1;
			if(minor < 31)
				count = read_argument(b, p, s, minor);
			for(size_t i = 0; i < count && (minor < 31 || nextbyte_peek(b, p, s) != 255); i++) {
				std::pair<item, item> it;
				it.first.ctor(b, p, s);
				it.second.ctor(b, p, s);
				if(tmp->count(it.first))
					throw std::runtime_error("Invalid CBOR: Duplicate map key");
				tmp->insert(it);
			}
			if(minor == 31)
				nextbyte(b, p, s);	//Dump the 0xFF.
			tag = T_MAP;
			value.pointer = tmp;
		} catch(...) {
			delete tmp;
			throw;
		}
		break;
	}
	case 6: {
		tag_t* tmp = new tag_t;
		tmp->first = read_argument(b, p, s, minor);
		try { tmp->second.ctor(b, p, s); } catch(...) { delete tmp; throw; }
		tag = T_TAG;
		value.pointer = tmp;
		break;
	}
	case 7:
		if(minor <= 24) {
			uint8_t v = read_argument(b, p, s, minor);
			if((v > 23 && v < 32) || v < minor)
				throw std::runtime_error("Invalid CBOR: Invalid simple type");
			set_simple(v);
		} else if(minor == 25)
			_set_float(expand_half_fp(read_argument(b, p, s, minor)));
		else if(minor == 26)
			_set_float(expand_single_fp(read_argument(b, p, s, minor)));
		else if(minor == 27)
			_set_float(read_argument(b, p, s, minor));
		break;
	}
}

int item::cmp(const item& a, const item& b) throw()
{
	size_t asize = a.get_size();
	size_t bsize = b.get_size();
	if(asize < bsize) return -1;
	if(asize > bsize) return 1;
	return cmp_nosize(a, b);
}

int item::cmp_nosize(const item& a, const item& b) throw()
{
	uint8_t atagb = a.get_tagbyte();
	uint8_t btagb = b.get_tagbyte();
	if(atagb < btagb) return -1;
	if(atagb > btagb) return 1;
	uint64_t aarg = a.get_arg();
	uint64_t barg = b.get_arg();
	if(aarg < barg) return -1;
	if(aarg > barg) return 1;
	switch(a.tag) {
	case T_UNSIGNED: return 0;	//Argument only.
	case T_NEGATIVE: return 0;	//Argument only.
	case T_FLOAT: return 0;		//Argument only.
	case T_SIMPLE: return 0;	//Argument only.
	case T_BOOLEAN: return 0;	//Argument only.
	case T_NULL: return 0;		//Argument only.
	case T_UNDEFINED: return 0;	//Argument only.
	case T_OCTETS: {
		auto& _a = a.inner_as<octets_t>();
		auto& _b = b.inner_as<octets_t>();
		for(size_t i = 0; i < _a.size(); i++)
			if(_a[i] < _b[i]) return -1;
			else if(_a[i] > _b[i]) return 1;
		return 0;
	}
	case T_STRING: {
		auto& _a = a.inner_as<string_t>();
		auto& _b = b.inner_as<string_t>();
		for(size_t i = 0; i < _a.size(); i++)
			if(_a[i] < _b[i]) return -1;
			else if(_a[i] > _b[i]) return 1;
		return 0;
	}
	case T_ARRAY: {
		auto& _a = a.inner_as<array_t>();
		auto& _b = b.inner_as<array_t>();
		auto i = _a.begin();
		auto j = _b.begin();
		while(i != _a.end()) {
			int c = cmp_nosize(i->second, j->second);
			if(c) return c;
			++i;
			++j;
		}
		return 0;
	}
	case T_MAP: {
		auto& _a = a.inner_as<map_t>();
		auto& _b = b.inner_as<map_t>();
		auto i = _a.begin();
		auto j = _b.begin();
		while(i != _a.end()) {
			int c = cmp_nosize(i->first, j->first);
			if(c) return c;
			c = cmp_nosize(i->second, j->second);
			if(c) return c;
			++i;
			++j;
		}
		return 0;
	}
	case T_TAG: return cmp_nosize(a.inner_as<tag_t>().second, b.inner_as<tag_t>().second);
	}
	return 0;
}

void item::_serialize(char*& out) const throw()
{
	switch(tag) {
	case T_UNSIGNED:
		*(out++) = 0x00 | minor_for(value.integer);
		write_arg(out, value.integer);
		break;
	case T_NEGATIVE:
		*(out++) = 0x20 | minor_for(value.integer);
		write_arg(out, value.integer);
		break;
	case T_OCTETS: {
		auto& tmp = inner_as<octets_t>();
		*(out++) = 0x40 | minor_for(tmp.size());
		write_arg(out, tmp.size());
		std::copy(tmp.begin(), tmp.end(), out);
		out += tmp.size();
		break;
	}
	case T_STRING: {
		auto& tmp = inner_as<string_t>();
		*(out++) = 0x60 | minor_for(tmp.length());
		write_arg(out, tmp.length());
		std::copy(tmp.begin(), tmp.end(), out);
		out += tmp.length();
		break;
	}
	case T_ARRAY: {
		auto& tmp = inner_as<array_t>();
		*(out++) = 0x80 | minor_for(tmp.size());
		write_arg(out, tmp.size());
		for(auto& i : tmp) {
			i.second._serialize(out);
		}
		break;
	}
	case T_MAP: {
		auto& tmp = inner_as<map_t>();
		*(out++) = 0xA0 | minor_for(tmp.size());
		write_arg(out, tmp.size());
		for(auto& i : tmp) {
			i.first._serialize(out);
			i.second._serialize(out);
		}
		break;
	}
	case T_TAG: {
		auto& tmp = inner_as<tag_t>();
		*(out++) = 0xC0 | minor_for(tmp.first);
		write_arg(out, tmp.first);
		tmp.second._serialize(out);
		break;
	}
	case T_FLOAT:
		*(out++) = 0xE0 | minor_for_float(value.integer);
		write_arg_float(out, value.integer);
		break;
	case T_SIMPLE:
		*(out++) = 0xE0 | minor_for(value.integer);
		write_arg(out, value.integer);
		break;
	case T_BOOLEAN:
		*(out++) = value.integer ? 0xF5 : 0xF4;
		break;
	case T_NULL:
		*(out++) = 0xF6;
		break;
	case T_UNDEFINED:
		*(out++) = 0xF7;
		break;
	}
}

void item::check_array() const throw(std::domain_error)
{
	if(tag != T_ARRAY)
		throw std::domain_error("Expected ARRAY");
}

void item::check_boolean() const throw(std::domain_error)
{
	if(tag != T_BOOLEAN)
		throw std::domain_error("Expected BOOLEAN");
}

void item::check_float() const throw(std::domain_error)
{
	if(tag != T_FLOAT)
		throw std::domain_error("Expected FLOAT");
}

void item::check_map() const throw(std::domain_error)
{
	if(tag != T_MAP)
		throw std::domain_error("Expected MAP");
}

void item::check_negative() const throw(std::domain_error)
{
	if(tag != T_NEGATIVE)
		throw std::domain_error("Expected NEGATIVE");
}

void item::check_octets() const throw(std::domain_error)
{
	if(tag != T_OCTETS)
		throw std::domain_error("Expected OCTETS");
}

void item::check_string() const throw(std::domain_error)
{
	if(tag != T_STRING)
		throw std::domain_error("Expected STRING");
}

void item::check_tag() const throw(std::domain_error)
{
	if(tag != T_TAG)
		throw std::domain_error("Expected TAG");
}

void item::check_unsigned() const throw(std::domain_error)
{
	if(tag != T_UNSIGNED)
		throw std::domain_error("Expected UNSIGNED");
}

uint8_t item::get_tagbyte() const throw()
{
	switch(tag) {
	case T_UNSIGNED: return 0x00 | minor_for(value.integer);
	case T_NEGATIVE: return 0x20 | minor_for(value.integer);
	case T_OCTETS: return 0x40 | minor_for(inner_as<octets_t>().size());
	case T_STRING: return 0x60 | minor_for(inner_as<string_t>().length());
	case T_ARRAY: return 0x80 | minor_for(inner_as<array_t>().size());
	case T_MAP: return 0xA0 | minor_for(inner_as<map_t>().size());
	case T_TAG: return 0xC0 | minor_for(inner_as<tag_t>().first);
	case T_SIMPLE: return 0xE0 | minor_for(value.integer);
	case T_BOOLEAN: return value.integer ? 0xF5 : 0xF4;
	case T_NULL: return 0xF6;
	case T_UNDEFINED: return 0xF7;
	case T_FLOAT: return 0xE0 | minor_for_float(value.integer);
	}
	return 0xFF;
}

uint64_t item::get_arg() const throw()
{
	switch(tag) {
	case T_UNSIGNED: return value.integer;
	case T_NEGATIVE: return value.integer;
	case T_OCTETS: return inner_as<octets_t>().size();
	case T_STRING: return inner_as<string_t>().length();
	case T_ARRAY: return inner_as<array_t>().size();
	case T_MAP: return inner_as<map_t>().size();
	case T_TAG: return inner_as<tag_t>().first;
	case T_SIMPLE: return value.integer;
	case T_BOOLEAN: return value.integer;
	case T_NULL: return value.integer;
	case T_UNDEFINED: return value.integer;
	case T_FLOAT: return compress_float(value.integer);
	}
	return 0;
}

}

#ifdef TEST_CBOR
bool roundtrip_serialize_test2(const unsigned char* test, size_t testlen,
	const unsigned char* ans, const size_t anslen)
{
	CBOR::item x((const char*)test, testlen);
	auto out = x.serialize();
	return !(out.size() != anslen || memcmp(&out[0], ans, anslen));
}

bool roundtrip_serialize_test2(const unsigned char* test, size_t testlen)
{
	return roundtrip_serialize_test2(test, testlen, test, testlen);
}

bool utf8_test2(const unsigned char* test, size_t testlen)
{
	return CBOR::_check_utf8(test, test + testlen);
}

struct test
{
	const char* name;
	std::function<bool()> fn;
};

void run_tests(struct test* t)
{
	while(t->name) {
		std::cout << "Testing " << t->name << "..." << std::flush;
		if(!t->fn()) {
			std::cout << "ERROR" << std::endl;
			throw std::logic_error("Test failed");
		}
		std::cout << "OK" << std::endl;
		t++;
	}
}

#define W(X) X, sizeof(X)

struct test tests[] = {
	{"Half-fp NaN", []() -> bool {
		unsigned char test_x[] = {0xF9, 0x7E, 0x00};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Half-fp zero", []() -> bool {
		unsigned char test_x[] = {0xF9, 0x00, 0x00};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Half-fp -zero", []() -> bool {
		unsigned char test_x[] = {0xF9, 0x80, 0x00};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Half-fp denormal", []() -> bool {
		unsigned char test_x[] = {0xF9, 0x80, 0x01};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Half-fp one", []() -> bool {
		unsigned char test_x[] = {0xF9, 0x3C, 0x00};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Single-fp neg denormal", []() -> bool {
		unsigned char test_x[] = {0xFA, 0x80, 0x00, 0x00, 0x01};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Single-fp ~1", []() -> bool {
		unsigned char test_x[] = {0xFA, 0x3F, 0x80, 0x00, 0x01};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Single-fp infinity", []() -> bool {
		unsigned char test_x[] = {0xFA, 0x7F, 0x80, 0x00, 0x00};
		unsigned char test_y[] = {0xF9, 0x7C, 0x00};
		return roundtrip_serialize_test2(W(test_x), W(test_y));
	}},
	{"Double-fp ~1", []() -> bool {
		unsigned char test_x[] = {0xFB, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"double-fp too small for single", []() -> bool {
		unsigned char test_x[] = {0xFB, 0x36, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"double-fp too large for single", []() -> bool {
		unsigned char test_x[] = {0xFB, 0x47, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Double-fp infinity", []() -> bool {
		unsigned char test_x[] = {0xFB, 0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		unsigned char test_y[] = {0xF9, 0x7C, 0x00};
		return roundtrip_serialize_test2(W(test_x), W(test_y));
	}},
	{"+23", []() -> bool {
		unsigned char test_x[] = {0x17};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"+2^8-1", []() -> bool {
		unsigned char test_x[] = {0x18, 0xFF};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"+2^16-1", []() -> bool {
		unsigned char test_x[] = {0x19, 0xFF, 0xFF};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"+2^32-1", []() -> bool {
		unsigned char test_x[] = {0x1A, 0xFF, 0xFF, 0xFF, 0xFF};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"+2^64-1", []() -> bool {
		unsigned char test_x[] = {0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Empty noncanonical array", []() -> bool {
		unsigned char test_x[] = {0x9F, 0xFF};
		unsigned char test_y[] = {0x80};
		return roundtrip_serialize_test2(W(test_x), W(test_y));
	}},
	{"-254", []() -> bool {
		unsigned char test_x[] = {0x38, 0xFE};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"[false,true,null,undefined]", []() -> bool {
		unsigned char test_x[] = {0x84, 0xF4, 0xF5, 0xF6, 0xF7};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"[#0,#4,#8,#20]", []() -> bool {
		unsigned char test_x[] = {0x84, 0xE0, 0xE4, 0xE8, 0xF8, 0x20};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"\"foo\"", []() -> bool {
		unsigned char test_x[] = {0x63, 'f', 'o', 'o'};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"<foo>", []() -> bool {
		unsigned char test_x[] = {0x43, 'f', 'o', 'o'};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Basic map sorting", []() -> bool {
		unsigned char test_x[] = {0xBF, 0xF5, 0x01, 0xF4, 0x00, 0xFF};
		unsigned char test_y[] = {0xA2, 0xF4, 0x00, 0xF5, 0x01};
		return roundtrip_serialize_test2(W(test_x), W(test_y));
	}},
	{"Basic tagging", []() -> bool {
		unsigned char test_x[] = {0xD7, 0xF5};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"Float keys", []() -> bool {
		unsigned char test_x[] = {0xA3, 0xF9, 0x7C, 0x00, 0x01, 0xF9, 0x7E, 0x00, 0x2, 0xFA, 0x80, 0x00,
			0x00, 0x01, 0x02};
		return roundtrip_serialize_test2(W(test_x));
	}},
	{"UTF8 check: Empty string", []() -> bool {
		unsigned char str1[] = {};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: ASCII", []() -> bool {
		unsigned char str1[] = {'f', 'o', 'o'};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: Bad continue", []() -> bool {
		unsigned char str1[] = {0xC2, 'o', 'o'};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: Overlong 2 bytes", []() -> bool {
		unsigned char str1[] = {0xC1, 0xBF};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: Overlong 3 bytes", []() -> bool {
		unsigned char str1[] = {0xE0, 0x9F, 0xBF};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: Overlong 4 bytes", []() -> bool {
		unsigned char str1[] = {0xF0, 0x8F, 0xBF, 0xBF};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: First 2 bytes", []() -> bool {
		unsigned char str1[] = {0xC2, 0x80};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: First 3 bytes", []() -> bool {
		unsigned char str1[] = {0xE0, 0xA0, 0x80};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: First 4 bytes", []() -> bool {
		unsigned char str1[] = {0xF0, 0x90, 0x80, 0x80};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: Spurious continue", []() -> bool {
		unsigned char str1[] = {0x80, 0xC2, 0x80};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: Surrogate range low", []() -> bool {
		unsigned char str1[] = {0xED, 0xA0, 0x80};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: Surrogate range high", []() -> bool {
		unsigned char str1[] = {0xED, 0xBF, 0xBF};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: FFFD", []() -> bool {
		unsigned char str1[] = {0xEF, 0xBF, 0xBD};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: FFFE", []() -> bool {
		unsigned char str1[] = {0xEF, 0xBF, 0xBE};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: FFFF", []() -> bool {
		unsigned char str1[] = {0xEF, 0xBF, 0xBF};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: 110000", []() -> bool {
		unsigned char str1[] = {0xF4, 0x90, 0x80, 0x80};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: F5 byte", []() -> bool {
		unsigned char str1[] = {0xF5};
		return !utf8_test2(W(str1));
	}},
	{"UTF8 check: xFFFD", []() -> bool {
		unsigned char str1[] = {0xF4, 0xBF, 0xBF, 0xBD};
		for(unsigned i = 1; i < 17; i++) {
			str1[0] = 0xF0 + (i >> 2);
			str1[1] = 0x8F + ((i & 3) << 4);
			if(!utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: xFFFE", []() -> bool {
		unsigned char str1[] = {0xF4, 0xBF, 0xBF, 0xBE};
		for(unsigned i = 1; i < 17; i++) {
			str1[0] = 0xF0 + (i >> 2);
			str1[1] = 0x8F + ((i & 3) << 4);
			if(utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: xFFFF", []() -> bool {
		unsigned char str1[] = {0xF4, 0xBF, 0xBF, 0xBF};
		for(unsigned i = 1; i < 17; i++) {
			str1[0] = 0xF0 + (i >> 2);
			str1[1] = 0x8F + ((i & 3) << 4);
			if(utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: FDD0 nonchars", []() -> bool {
		unsigned char str1[] = {0xEF, 0xB7, 0x90};
		for(unsigned i = 0; i < 32; i++) {
			str1[2] = 0x90 + i;
			if(utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: 1FDD0 range", []() -> bool {
		unsigned char str1[] = {0xF0, 0x9F, 0xB7, 0x90};
		for(unsigned i = 0; i < 32; i++) {
			str1[2] = 0x90 + i;
			if(!utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: D7FF", []() -> bool {
		unsigned char str1[] = {0xED, 0x9F, 0xBF};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: E000", []() -> bool {
		unsigned char str1[] = {0xEE, 0x80, 0x80};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: FDCF", []() -> bool {
		unsigned char str1[] = {0xEF, 0xB7, 0x8F};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: FDF0", []() -> bool {
		unsigned char str1[] = {0xEF, 0xB7, 0xB0};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: xFFBF", []() -> bool {
		unsigned char str1[] = {0xF4, 0xBF, 0xBE, 0xBF};
		for(unsigned i = 1; i < 17; i++) {
			str1[0] = 0xF0 + (i >> 2);
			str1[1] = 0x8F + ((i & 3) << 4);
			if(!utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: xEFFF", []() -> bool {
		unsigned char str1[] = {0xF4, 0xBE, 0xBF, 0xBF};
		for(unsigned i = 1; i < 17; i++) {
			str1[0] = 0xF0 + (i >> 2);
			str1[1] = 0x8E + ((i & 3) << 4);
			if(!utf8_test2(W(str1)))
				return false;
		}
		return true;
	}},
	{"UTF8 check: FFBF", []() -> bool {
		unsigned char str1[] = {0xEF, 0xBE, 0xBF};
		return utf8_test2(W(str1));
	}},
	{"UTF8 check: EFFF", []() -> bool {
		unsigned char str1[] = {0xEE, 0xBF, 0xBF};
		return utf8_test2(W(str1));
	}},
	{"Basic string serialize", []() -> bool {
		unsigned char ans[] = {0x63, 'f', 'o', 'o'};
		CBOR::item i = CBOR::string("foo");
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Serialize booleans", []() -> bool {
		unsigned char ans[] = {0x82, 0xF4, 0xF5};
		CBOR::item i = CBOR::array();
		i.set_array_size(2);
		i[0] = CBOR::boolean(false);
		i[1] = CBOR::boolean(true);
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Boolean simple equivalence", []() -> bool {
		if(!(CBOR::simple(20) == CBOR::boolean(false))) return false;
		if(!(CBOR::simple(21) == CBOR::boolean(true))) return false;
		if(!(CBOR::simple(22) == CBOR::null())) return false;
		if(!(CBOR::simple(23) == CBOR::undefined())) return false;
		return true;
	}},
	{"Simple conversions", []() -> bool {
		CBOR::item i;
		i.set_simple(20);
		if(!(i == CBOR::boolean(false))) return false;
		i.set_simple(21);
		if(!(i == CBOR::boolean(true))) return false;
		i.set_simple(22);
		if(!(i == CBOR::null())) return false;
		i.set_simple(23);
		if(!(i == CBOR::undefined())) return false;
		return true;
	}},
	{"Float compression", []() -> bool {
		if(CBOR::compress_float(0x0000000000000000ULL) != 0x0000) return false;
		if(CBOR::compress_float(0x3FF0000000000000ULL) != 0x3C00) return false;
		if(CBOR::compress_float(0x3FF0040000000000ULL) != 0x3C01) return false;
		if(CBOR::compress_float(0x3FF0020000000000ULL) != 0x3F801000) return false;
		if(CBOR::compress_float(0x3FF0000020000000ULL) != 0x3F800001) return false;
		if(CBOR::compress_float(0x3FF0000010000000ULL) != 0x3FF0000010000000ULL) return false;
		if(CBOR::compress_float(0x7FF0000000000000ULL) != 0x7C00) return false;
		return true;
	}},
	{"String UTF-8 check", []() -> bool {
		try {
			CBOR::string("\xF5");
			return false;
		} catch(std::out_of_range& e) {
		}
		try {
			CBOR::item i;
			i.set_string("\xF5");
			return false;
		} catch(std::out_of_range& e) {
		}
		return true;
	}},
	{"Assign octets", []() -> bool {
		unsigned char test_x[] = {0x44, 0x12, 0x34, 0x56, 0x78};
		CBOR::item i((const char*)test_x, sizeof(test_x));
		CBOR::item j = i;
		auto out = i.serialize();
		return !(out.size() != sizeof(test_x) || memcmp(&out[0], test_x, sizeof(test_x)));
	}},
	{"Assign string", []() -> bool {
		unsigned char test_x[] = {0x64, 0x12, 0x34, 0x56, 0x78};
		CBOR::item i((const char*)test_x, sizeof(test_x));
		CBOR::item j = i;
		auto out = i.serialize();
		return !(out.size() != sizeof(test_x) || memcmp(&out[0], test_x, sizeof(test_x)));
	}},
	{"Assign array", []() -> bool {
		unsigned char test_x[] = {0x84, 0xF4, 0xF5, 0xF6, 0xF7};
		CBOR::item i((const char*)test_x, sizeof(test_x));
		CBOR::item j = i;
		auto out = i.serialize();
		return !(out.size() != sizeof(test_x) || memcmp(&out[0], test_x, sizeof(test_x)));
	}},
	{"Assign map", []() -> bool {
		unsigned char test_x[] = {0xA3, 0xF9, 0x7C, 0x00, 0x01, 0xF9, 0x7E, 0x00, 0x2, 0xFA, 0x80, 0x00,
			0x00, 0x01, 0x02};
		CBOR::item i((const char*)test_x, sizeof(test_x));
		CBOR::item j = i;
		auto out = i.serialize();
		return !(out.size() != sizeof(test_x) || memcmp(&out[0], test_x, sizeof(test_x)));
	}},
	{"Assign tag", []() -> bool {
		unsigned char test_x[] = {0xC3, 0x17};
		CBOR::item i((const char*)test_x, sizeof(test_x));
		CBOR::item j = i;
		auto out = i.serialize();
		return !(out.size() != sizeof(test_x) || memcmp(&out[0], test_x, sizeof(test_x)));
	}},
	{"Invalid simples", []() -> bool {
		try {
			CBOR::simple(24);
			return false;
		} catch(std::out_of_range& e) {
		}
		try {
			CBOR::item i;
			i.set_simple(24);
			return false;
		} catch(std::out_of_range& e) {
		}
		try {
			CBOR::simple(31);
			return false;
		} catch(std::out_of_range& e) {
		}
		try {
			CBOR::item i;
			i.set_simple(31);
			return false;
		} catch(std::out_of_range& e) {
		}
		return true;
	}},
	{"Unsigned compare", []() -> bool {
		CBOR::item t;
		CBOR::item i = CBOR::integer(12345);
		t.set_unsigned(12345);
		if(!(i == t)) return false;
		t.set_unsigned(12346);
		if(i == t) return false;
		return true;
	}},
	{"Negative compare", []() -> bool {
		CBOR::item t;
		CBOR::item i = CBOR::negative(12345);
		t.set_negative(12345);
		if(!(i == t)) return false;
		t.set_negative(12346);
		if(i == t) return false;
		return true;
	}},
	{"Serialize map", []() -> bool {
		unsigned char ans[] = {0xA2, 0xF4, 0x01, 0xF5, 0x02};
		CBOR::item i = CBOR::map();
		i[CBOR::boolean(true)] = CBOR::integer(2);
		i[CBOR::boolean(false)] = CBOR::integer(1);
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Serialize simple", []() -> bool {
		unsigned char ans[] = {0xF2};
		CBOR::item i = CBOR::simple(18);
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Serialize tag", []() -> bool {
		unsigned char ans[] = {0xD6, 0xF7};
		CBOR::item i = CBOR::tag(0x16, CBOR::undefined());
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Serialize float", []() -> bool {
		unsigned char ans[] = {0xF9, 0x3C, 0x00};
		CBOR::item i = CBOR::floating(1.0);
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Serialize octets", []() -> bool {
		std::vector<uint8_t> x;
		x.push_back(0x12);
		x.push_back(0x65);
		x.push_back(0x98);
		unsigned char ans[] = {0x43, 0x12, 0x65, 0x98};
		CBOR::item i = CBOR::octets(x);
		auto out = i.serialize();
		return !(out.size() != sizeof(ans) || memcmp(&out[0], ans, sizeof(ans)));
	}},
	{"Incomplete sequence", []() -> bool {
		unsigned char test_x[] = {0x84, 0x00, 0x01, 0x02};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"Incomplete sequence, non-canonical", []() -> bool {
		unsigned char test_x[] = {0x9F, 0x00, 0x01, 0x02};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"set/get unsigned", []() -> bool {
		CBOR::item i;
		i.set_unsigned(12345);
		if(i.get_type() != CBOR::T_UNSIGNED) return false;
		return i.get_unsigned() == 12345;
	}},
	{"set/get negative", []() -> bool {
		CBOR::item i;
		i.set_negative(12345);
		if(i.get_type() != CBOR::T_NEGATIVE) return false;
		return i.get_negative() == 12345;
	}},
	{"set/get string", []() -> bool {
		CBOR::item i;
		i.set_string("foobar");
		if(i.get_type() != CBOR::T_STRING) return false;
		return i.get_string() == "foobar";
	}},
	{"tag operations", []() -> bool {
		CBOR::item i;
		i.set_string("foobar");
		i.tag_item(17);
		if(i.get_type() != CBOR::T_TAG) return false;
		if(i.get_tag_number() != 17) return false;
		if(i.get_tag_inner().get_string() != "foobar") return false;
		i.set_tag_number(19);
		if(i.get_tag_number() != 19) return false;
		if(i.get_tag_inner().get_string() != "foobar") return false;
		i.detag_item();
		return i.get_string() == "foobar";
		return true;
	}},
	{"set/get float", []() -> bool {
		CBOR::item i;
		i.set_float(1.25);
		if(i.get_type() != CBOR::T_FLOAT) return false;
		return i.get_float() == 1.25;
	}},
	{"set/get boolean & co", []() -> bool {
		CBOR::item i;
		i.set_boolean(false);
		if(i.get_boolean() != false) return false;
		if(i.get_type() != CBOR::T_BOOLEAN) return false;
		if(i.get_simple() != 20) return false;
		i.set_boolean(true);
		if(i.get_boolean() != true) return false;
		if(i.get_type() != CBOR::T_BOOLEAN) return false;
		if(i.get_simple() != 21) return false;
		i.set_null();
		if(i.get_type() != CBOR::T_NULL) return false;
		if(i.get_simple() != 22) return false;
		i.set_undefined();
		if(i.get_type() != CBOR::T_UNDEFINED) return false;
		if(i.get_simple() != 23) return false;
		return true;
	}},
	{"set/get simple", []() -> bool {
		CBOR::item i;
		i.set_simple(17);
		if(i.get_type() != CBOR::T_SIMPLE) return false;
		return i.get_simple() == 17;
	}},
	{"get simple wrong", []() -> bool {
		CBOR::item i;
		i.set_unsigned(1);
		try {
			i.get_simple();
			return false;
		} catch(std::domain_error& e) {
		}
		return true;
	}},
	{"Get/set octets", []() -> bool {
		std::vector<uint8_t> x;
		x.push_back(0x12);
		x.push_back(0x65);
		x.push_back(0x98);
		CBOR::item i;
		i.set_octets(x);
		auto y = i.get_octets();
		return x == y;
	}},
	{"Get/set octets #2", []() -> bool {
		std::vector<uint8_t> x;
		x.push_back(0x12);
		x.push_back(0x65);
		x.push_back(0x98);
		x.push_back(0xAA);
		CBOR::item i;
		i.set_octets(&x[0], 4);
		auto y = i.get_octets();
		return x == y;
	}},
	{"Get/set array", []() -> bool {
		CBOR::item i;
		const CBOR::item& _i = i;
		i.set_array();
		if(i.get_type() != CBOR::T_ARRAY) return false;
		i.set_array_size(5);
		if(i.get_array_size() != 5) return false;
		i.set_array_size(3);
		if(i.get_array_size() != 3) return false;
		try {
			i[3];
			return false;
		} catch(std::out_of_range& e) {
		}
		try {
			_i[3];
			return false;
		} catch(std::out_of_range& e) {
		}
		i[0] = CBOR::string("foo");
		i[1] = CBOR::tag(13, CBOR::map());
		i[2] = CBOR::undefined();
		if(_i[0].get_string() != "foo") return false;
		if(_i[1].get_tag_number() != 13) return false;
		if(_i[1].get_tag_inner().get_type() != CBOR::T_MAP) return false;
		if(_i[2].get_simple() != 23) return false;
		return true;
	}},
	{"Get/set map", []() -> bool {
		CBOR::item i;
		const CBOR::item& _i = i;
		i.set_map();
		if(i.get_type() != CBOR::T_MAP) return false;
		if(i.get_map_size() != 0) return false;
		i[CBOR::string("foo")] = CBOR::integer(1);
		if(i.get_map_size() != 1) return false;
		i[CBOR::string("bar")] = CBOR::string("zot");
		if(i.get_map_size() != 2) return false;
		i[CBOR::string("baz")] = CBOR::null();
		if(i.get_map_size() != 3) return false;
		i[CBOR::string("foo")] = CBOR::integer(6);
		if(i.get_map_size() != 3) return false;
		try {
			i.get_map_lookup(CBOR::string("zot"));
			return false;
		} catch(std::out_of_range& e) {
		}
		try {
			_i[CBOR::string("zot")];
			return false;
		} catch(std::out_of_range& e) {
		}
		if(_i[CBOR::string("foo")].get_unsigned() != 6) return false;
		if(_i[CBOR::string("bar")].get_string() != "zot") return false;
		if(_i[CBOR::string("baz")].get_type() != CBOR::T_NULL) return false;
		if(_i[CBOR::string("baz")].get_simple() != 22) return false;
		for(auto j = i.get_map_begin(); j != i.get_map_end(); j++) {
			if(j->first.get_string() == "foo" && j->second.get_unsigned() != 6) return false;
			if(j->first.get_string() == "bar" && j->second.get_string() != "zot") return false;
			if(j->first.get_string() == "baz" && j->second.get_type() != CBOR::T_NULL) return false;
		}
		for(auto j = _i.get_map_begin(); j != _i.get_map_end(); j++) {
			if(j->first.get_string() == "foo" && j->second.get_unsigned() != 6) return false;
			if(j->first.get_string() == "bar" && j->second.get_string() != "zot") return false;
			if(j->first.get_string() == "baz" && j->second.get_type() != CBOR::T_NULL) return false;
		}
		return true;
	}},
	{"Extended simple not extended", []() -> bool {
		unsigned char test_x[] = {0xF8, 0x14};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"Map near-duplicate key", []() -> bool {
		unsigned char test_x[] = {0xA2, 0x05, 0x06, 0x18, 0x06, 0x17};
		unsigned char test_y[] = {0xA2, 0x05, 0x06, 0x06, 0x17};
		return roundtrip_serialize_test2(W(test_x), W(test_y));
	}},
	{"Map duplicate key", []() -> bool {
		unsigned char test_x[] = {0xA2, 0x05, 0x06, 0x18, 0x05, 0x17};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"Map near-duplicate key noncanonical", []() -> bool {
		unsigned char test_x[] = {0xA2, 0xA1, 0x00, 0x05, 0x06, 0xBF, 0x00, 0x06, 0xFF, 0x07};
		unsigned char test_y[] = {0xA2, 0xA1, 0x00, 0x05, 0x06, 0xA1, 0x00, 0x06, 0x07};
		return roundtrip_serialize_test2(W(test_x), W(test_y));
	}},
	{"Map duplicate key noncanonical", []() -> bool {
		unsigned char test_x[] = {0xA2, 0xA1, 0x00, 0x05, 0x06, 0xBF, 0x00, 0x05, 0xFF, 0x07};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"Float compare", []() -> bool {
		return CBOR::floating(1.375) == CBOR::floating(1.375);
	}},
	{"Simple compare", []() -> bool {
		return CBOR::simple(14) == CBOR::simple(14);
	}},
	{"Tags compare", []() -> bool {
		return CBOR::tag(14, CBOR::simple(20)) != CBOR::tag(14, CBOR::simple(21));
	}},
	{"Tags order", []() -> bool {
		return CBOR::tag(14, CBOR::simple(20)) < CBOR::tag(14, CBOR::simple(21));
	}},
	{"Tags order #2", []() -> bool {
		return CBOR::tag(15, CBOR::simple(20)) > CBOR::tag(14, CBOR::simple(21));
	}},
	{"Incomplete octets", []() -> bool {
		unsigned char test_x[] = {0x43, 0xA0, 0x52};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"Incomplete string", []() -> bool {
		unsigned char test_x[] = {0x63, 0xC4, 0x86};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{"String bad UTF-8", []() -> bool {
		unsigned char test_x[] = {0x63, 0xC4, 0x86, 0x87};
		try {
			CBOR::item i((const char*)test_x, sizeof(test_x));
			return false;
		} catch(std::runtime_error& e) {
		}
		return true;
	}},
	{NULL, []() -> bool { return true; }}
};

int main()
{
	run_tests(tests);

	return 0;
}
#endif
