#include "framebuffer-font2.hpp"
#include "memorywatch.hpp"
#include "memoryspace.hpp"
#include "int24.hpp"
#include "mathexpr-error.hpp"
#include "mathexpr.hpp"
#include <functional>
#include <sstream>
#include "string.hpp"

namespace memorywatch
{
namespace
{
	template<typename T> T* pointer_cast(char* ptr)
	{
		return reinterpret_cast<T*>(ptr);
	}
}

memread_oper::memread_oper()
	: operinfo("(readmemory)")
{
}

memread_oper::~memread_oper() {}

void memread_oper::evaluate(mathexpr::value target, std::vector<std::function<mathexpr::value()>> promises)
{
	if(promises.size() != 1)
		throw mathexpr::error(mathexpr::error::ARGCOUNT, "Memory read operator takes 1 argument");
	static const int system_endian = memory_space::get_system_endian();
	uint64_t addr;
	mathexpr::value val;
	try {
		val = promises[0]();
		void* res = val._value;
		addr = val.type->tounsigned(res);
		if(addr_size)
			addr %= addr_size;
		addr += addr_base;
	} catch(std::exception& e) {
		throw mathexpr::error(mathexpr::error::ADDR, e.what());
	}
	if(bytes > 8)
		throw mathexpr::error(mathexpr::error::SIZE, "Memory read size out of range");
	char buf[8];
	mspace->read_range(addr, buf, bytes);
	//Endian swap if needed.
	if(endianess && system_endian != endianess)
		for(unsigned i = 0; i < bytes / 2; i++)
			std::swap(buf[i], buf[bytes - i - 1]);
	switch(bytes) {
	case 1:
		if(float_flag)
			throw mathexpr::error(mathexpr::error::SIZE, "1 byte floats not supported");
		else if(signed_flag)
			target.type->parse_s(target._value, *(const int8_t*)buf);
		else
			target.type->parse_u(target._value, *(const uint8_t*)buf);
		break;
	case 2:
		if(float_flag)
			throw mathexpr::error(mathexpr::error::SIZE, "2 byte floats not supported");
		else if(signed_flag)
			target.type->parse_s(target._value, *pointer_cast<int16_t>(buf));
		else
			target.type->parse_u(target._value, *pointer_cast<uint16_t>(buf));
		break;
	case 3:
		if(float_flag)
			throw mathexpr::error(mathexpr::error::SIZE, "3 byte floats not supported");
		else if(signed_flag)
			target.type->parse_s(target._value, *pointer_cast<ss_int24_t>(buf));
		else
			target.type->parse_u(target._value, *pointer_cast<ss_uint24_t>(buf));
		break;
	case 4:
		if(float_flag)
			target.type->parse_f(target._value, *pointer_cast<float>(buf));
		else if(signed_flag)
			target.type->parse_s(target._value, *pointer_cast<int32_t>(buf));
		else
			target.type->parse_u(target._value, *pointer_cast<uint32_t>(buf));
		break;
	case 8:
		if(float_flag)
			target.type->parse_f(target._value, *pointer_cast<double>(buf));
		else if(signed_flag)
			target.type->parse_s(target._value, *pointer_cast<int64_t>(buf));
		else
			target.type->parse_u(target._value, *pointer_cast<uint64_t>(buf));
		break;
	default:
		throw mathexpr::error(mathexpr::error::SIZE, "Memory address size not supported");
	}
	if(scale_div > 1)
		target.type->scale(target._value, scale_div);
}

namespace
{
	bool is_terminal(char ch)
	{
		if(ch == '%') return true;
		if(ch == 'b') return true;
		if(ch == 'B') return true;
		if(ch == 'd') return true;
		if(ch == 'i') return true;
		if(ch == 'o') return true;
		if(ch == 's') return true;
		if(ch == 'u') return true;
		if(ch == 'x') return true;
		if(ch == 'X') return true;
		return false;
	}

	std::string get_placeholder(const std::string& str, size_t idx)
	{
		std::ostringstream p;
		for(size_t i = idx; i < str.length(); i++) {
			p << str[i];
			if(is_terminal(str[idx]))
				break;
		}
		return p.str();
	}
}
/*
	bool showsign;
	bool fillzeros;
	int width;
	int precision;
	bool uppercasehex;
*/

item_printer::~item_printer()
{
}

void item_printer::trace()
{
}

std::string item::get_value()
{
	if(format == "") {
		//Default.
		mathexpr::_format fmt;
		fmt.type = mathexpr::_format::DEFAULT;
		mathexpr::value v = expr->evaluate();
		return v.type->format(v._value, fmt);
	}
	std::ostringstream out;
	for(size_t i = 0; i < format.length(); i++) {
		if(format[i] != '%')
			out << format[i];
		else {
			//Format placeholder.
			std::string p = get_placeholder(format, i + 1);
			if(p == "")
				continue;
			i += p.length();
			if(p[p.length() - 1] == '%') {
				out << '%';
				continue;
			}
			mathexpr::_format fmt;
			fmt.showsign = false;
			fmt.fillzeros = false;
			fmt.width = -1;
			fmt.precision = -1;
			fmt.uppercasehex = false;
			auto r = regex("([+0]*)([1-9][0-9]*)?(\\.(0|[1-9][0-9]*))?([bBdiosuxX])", p);
			if(!r) {
				throw mathexpr::error(mathexpr::error::FORMAT, "Bad format placeholder");
				continue;
			}
			std::string flags = r[1];
			size_t i;
			for(i = 0; i < flags.length(); i++) {
				if(flags[i] == '+')
					fmt.showsign = true;
				if(flags[i] == '0')
					fmt.fillzeros = true;
			}
			//The remaining part is width.precision.
			if(r[2] != "")
				try {
					fmt.width = parse_value<int>(r[2]);
				} catch(...) {}
			if(r[4] != "")
				try {
					fmt.precision = parse_value<int>(r[4]);
				} catch(...) {}
			switch(r[5][0]) {
			case 'b': fmt.type = mathexpr::_format::BINARY; break;
			case 'B': fmt.type = mathexpr::_format::BOOLEAN; break;
			case 'd': fmt.type = mathexpr::_format::DECIMAL; break;
			case 'i': fmt.type = mathexpr::_format::DECIMAL; break;
			case 'o': fmt.type = mathexpr::_format::OCTAL; break;
			case 's': fmt.type = mathexpr::_format::STRING; break;
			case 'u': fmt.type = mathexpr::_format::DECIMAL; break;
			case 'x': fmt.type = mathexpr::_format::HEXADECIMAL; break;
			case 'X': fmt.type = mathexpr::_format::HEXADECIMAL; fmt.uppercasehex = true; break;
			}
			mathexpr::value v = expr->evaluate();
			out << v.type->format(v._value, fmt);
		}
	}
	return out.str();
}

void item::show(const std::string& n)
{
	std::string x;
	try {
		x = get_value();
	} catch(std::bad_alloc& e) {
		throw;
	} catch(mathexpr::error& e) {
		x = e.get_short_error();
	} catch(std::runtime_error& e) {
		x = e.what();
	}
	if(printer)
		printer->show(n, x);
}


set::~set()
{
	roots.clear();
	GC::item::do_gc();
}

void set::reset()
{
	for(auto& i : roots) {
		if(i.second.printer)
			i.second.printer->reset();
		i.second.expr->reset();
	}
}

void set::refresh()
{
	for(auto& i : roots)
		i.second.expr->reset();
	for(auto& i : roots)
		i.second.show(i.first);
}

std::set<std::string> set::names_set()
{
	std::set<std::string> r;
	for(auto i : roots)
		r.insert(i.first);
	return r;
}

item& set::get(const std::string& name)
{
	auto i = get_soft(name);
	if(!i)
		throw std::runtime_error("No such watch '" + name + "'");
	return *i;
}

item* set::get_soft(const std::string& name)
{
	if(!roots.count(name))
		return NULL;
	return &(roots.find(name)->second);
}

item* set::create(const std::string& name, item& item)
{
	roots.insert(std::make_pair(name, item));
	return &(roots.find(name)->second);
}

void set::destroy(const std::string& name)
{
	if(!roots.count(name))
		return;
	roots.erase(name);
	GC::item::do_gc();
}

const std::string& set::get_longest_name(std::function<size_t(const std::string& n)> rate)
{
	static std::string empty;
	size_t best_len = 0;
	const std::string* best = &empty;
	for(auto& i : roots) {
		size_t r = rate(i.first);
		if(r > best_len) {
			best = &i.first;
			best_len = r;
		}
	}
	return *best;
}

size_t set::utflength_rate(const std::string& n)
{
	return utf8::strlen(n);
}

void set::foreach(std::function<void(item& item)> cb)
{
	for(auto& i : roots)
		cb(i.second);
}

void set::swap(set& s) throw()
{
	std::swap(roots, s.roots);
}
}
