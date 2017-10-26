#include "rrdata.hpp"
#include "string.hpp"
#include "minmax.hpp"
#include <functional>
#include <iostream>
#include "library/directory.hpp"
#include <sstream>

uint64_t get_file_size(const std::string& filename)
{
	uintmax_t size = directory::size(filename);
	if(size == static_cast<uintmax_t>(-1))
		return 0;
	return size;
}

struct test
{
	const char* name;
	std::function<bool()> run;
};

struct test tests[] = {
	{"instance default ctor", []() {
		rrdata_set::instance i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000000";
	}},{"instance bytes ctor", []() {
		uint8_t buf[32] =  {0};
		buf[30] = 1;
		rrdata_set::instance i(buf);
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000100";
	}},{"instance string ctor #1", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000763234676");
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000763234676";
	}},{"instance string ctor #2", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000763afba76");
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000763AFBA76";
	}},{"instance string ctor #3", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000763AFBA76");
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000763AFBA76";
	}},{"< #1", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000000");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000000");
		return !(i1 < i2);
	}},{"< #2", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000000");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000001");
		return i1 < i2;
	}},{"< #3", []() {
		rrdata_set::instance i1("00000000000000000000000000000000000000000000000000000000000000FF");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000100");
		return i1 < i2;
	}},{"< #4", []() {
		rrdata_set::instance i1("00000000000000000000000000000000000000000000000000000000000000FF");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000100");
		return !(i2 < i1);
	}},{"== #1", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000001");
		return (i2 == i1);
	}},{"== #2", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000101");
		return !(i2 == i1);
	}},{"== #3", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000000");
		return !(i2 == i1);
	}},{"post-++ #1", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000000");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000001";
	}},{"post-++ #2", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000001");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000002";
	}},{"post-++ #3", []() {
		rrdata_set::instance i("000000000000000000000000000000000000000000000000000000000000000F");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000010";
	}},{"post-++ #4", []() {
		rrdata_set::instance i("00000000000000000000000000000000000000000000000000000000000000FF");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000100";
	}},{"post-++ #5", []() {
		rrdata_set::instance i("000000000000000000000000000000000000000000000000000000000000FFFF");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000010000";
	}},{"post-++ #6", []() {
		rrdata_set::instance i("000000000000000000000000000000000000000000000000FFFFFFFFFFFFFFFF");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000010000000000000000";
	}},{"post-++ #7", []() {
		rrdata_set::instance i("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
		i++;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000000";
	}},{"post-++ #8", []() {
		rrdata_set::instance i("00000000000000000000000000000000000000000000000000000000000000FE");
		i++;
		return (stringfmt() << i).str() == "00000000000000000000000000000000000000000000000000000000000000FF";
	}},{"pre-++ #1", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000000");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000001";
	}},{"pre-++ #2", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000001");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000002";
	}},{"pre-++ #3", []() {
		rrdata_set::instance i("000000000000000000000000000000000000000000000000000000000000000F");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000010";
	}},{"pre-++ #4", []() {
		rrdata_set::instance i("00000000000000000000000000000000000000000000000000000000000000FF");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000100";
	}},{"pre-++ #5", []() {
		rrdata_set::instance i("000000000000000000000000000000000000000000000000000000000000FFFF");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000010000";
	}},{"pre-++ #6", []() {
		rrdata_set::instance i("000000000000000000000000000000000000000000000000FFFFFFFFFFFFFFFF");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000010000000000000000";
	}},{"pre-++ #7", []() {
		rrdata_set::instance i("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
		++i;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000000";
	}},{"pre-++ #8", []() {
		rrdata_set::instance i("00000000000000000000000000000000000000000000000000000000000000FE");
		++i;
		return (stringfmt() << i).str() == "00000000000000000000000000000000000000000000000000000000000000FF";
	}},{"Operator+ #1", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000000");
		i = i + 0x12;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000012";
	}},{"Operator+ #2", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000000");
		i = i + 0x1234;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000001234";
	}},{"Operator+ #3", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000001");
		i = i + 0xFF;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000000100";
	}},{"Operator+ #4", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000001");
		i = i + 0xFFFF;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000010000";
	}},{"Operator+ #4", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000102");
		i = i + 0xFEFF;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000000010001";
	}},{"Operator+ #4", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000002");
		i = i + 0xFFFFFFFFU;
		return (stringfmt() << i).str() == "0000000000000000000000000000000000000000000000000000000100000001";
	}},{"Operator- #1", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000002");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000002");
		return (i1 - i2) == 0;
	}},{"Operator- #2", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000002");
		return (i1 - i2) == std::numeric_limits<unsigned>::max();
	}},{"Operator- #3", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000002");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000001");
		return (i1 - i2) == 1;
	}},{"Operator- #4", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000007");
		return (i1 - i2) == std::numeric_limits<unsigned>::max();
	}},{"Operator- #5", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000236");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000123");
		return (i1 - i2) == 0x113;
	}},{"Operator- #6", []() {
		rrdata_set::instance i1("00000000000000000000000000000000000000000000000000000000FFFFFFFE");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000000");
		return (i1 - i2) == 0xFFFFFFFEU;
	}},{"Operator- #7", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000100000000");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000002");
		return (i1 - i2) == 0xFFFFFFFEU;
	}},{"Operator- #8", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000001000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000002");
		return (i1 - i2) == 0xFFFFFFFFU;
	}},{"Operator- #9", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000000");
		rrdata_set::instance i2("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
		return (i1 - i2) == 1;
	}},{"rrdata init", []() {
		rrdata_set s;
		return s.debug_dump() == "0[]";
	}},{"rrdata add", []() {
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000002");
		rrdata_set s;
		if(!s.debug_add(i2))
			return false;
		return s.debug_dump() == "1[{0000000000000000000000000000000000000000000000000000000000000002,"
			"0000000000000000000000000000000000000000000000000000000000000003}]";
	}},{"rrdata add disjoint", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000002");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000004");
		rrdata_set s;
		if(!s.debug_add(i1))
			return false;
		if(!s.debug_add(i2))
			return false;
		return s.debug_dump() == "2[{0000000000000000000000000000000000000000000000000000000000000002,"
			"0000000000000000000000000000000000000000000000000000000000000003}{"
			"0000000000000000000000000000000000000000000000000000000000000004,"
			"0000000000000000000000000000000000000000000000000000000000000005}]";
	}},{"rrdata add again", []() {
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000002");
		rrdata_set s;
		if(!s.debug_add(i2))
			return false;
		if(s.debug_add(i2))
			return false;
		return s.debug_dump() == "1[{0000000000000000000000000000000000000000000000000000000000000002,"
			"0000000000000000000000000000000000000000000000000000000000000003}]";
	}},{"rrdata extend range low", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000000");
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000004");
		rrdata_set s;
		s.debug_add(i1, i2);
		if(!s.debug_add(i)) {
			std::cout << "Collides..." << std::flush;
			return false;
		}
		if(s.debug_dump() == "4[{0000000000000000000000000000000000000000000000000000000000000000,"
			"0000000000000000000000000000000000000000000000000000000000000004}]")
			return true;
		std::cout << s.debug_dump() << "..." << std::flush;
		return false;
	}},{"rrdata extend range high", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000004");
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000004");
		rrdata_set s;
		s.debug_add(i1, i2);
		if(!s.debug_add(i))
			return false;
		return s.debug_dump() == "4[{0000000000000000000000000000000000000000000000000000000000000001,"
			"0000000000000000000000000000000000000000000000000000000000000005}]";
	}},{"rrdata add again (range)", []() {
		rrdata_set::instance i("0000000000000000000000000000000000000000000000000000000000000002");
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000004");
		rrdata_set s;
		s.debug_add(i1, i2);
		if(s.debug_add(i))
			return false;
		return s.debug_dump() == "3[{0000000000000000000000000000000000000000000000000000000000000001,"
			"0000000000000000000000000000000000000000000000000000000000000004}]";
	}},{"rrdata partial overlap (previous)", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000009");
		rrdata_set::instance i3("0000000000000000000000000000000000000000000000000000000000000007");
		rrdata_set::instance i4("000000000000000000000000000000000000000000000000000000000000000F");
		rrdata_set s;
		s.debug_add(i1, i2);
		s.debug_add(i3, i4);
		return s.debug_dump() == "10[{0000000000000000000000000000000000000000000000000000000000000005,"
			"000000000000000000000000000000000000000000000000000000000000000F}]";
	}},{"rrdata bridging add", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000B");
		rrdata_set::instance i4("000000000000000000000000000000000000000000000000000000000000000F");
		rrdata_set::instance i5("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set s;
		s.debug_add(i1, i2);
		s.debug_add(i3, i4);
		s.debug_add(i5);
		return s.debug_dump() == "10[{0000000000000000000000000000000000000000000000000000000000000005,"
			"000000000000000000000000000000000000000000000000000000000000000F}]";
	}},{"rrdata bridging add #2", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000B");
		rrdata_set::instance i4("000000000000000000000000000000000000000000000000000000000000000F");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000008");
		rrdata_set::instance i6("000000000000000000000000000000000000000000000000000000000000000D");
		rrdata_set s;
		s.debug_add(i1, i2);
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		return s.debug_dump() == "10[{0000000000000000000000000000000000000000000000000000000000000005,"
			"000000000000000000000000000000000000000000000000000000000000000F}]";
	}},{"rrdata discontinuous reverse", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000009");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("000000000000000000000000000000000000000000000000000000000000000F");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i1, i2);
		return s.debug_dump() == "9[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000009}{"
			"000000000000000000000000000000000000000000000000000000000000000A,"
			"000000000000000000000000000000000000000000000000000000000000000F}]";
	}},{"rrdata elide next #1", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000010");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("000000000000000000000000000000000000000000000000000000000000000F");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i1, i2);
		return s.debug_dump() == "11[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000010}]";
	}},{"rrdata elide next #2", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000010");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000010");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i1, i2);
		return s.debug_dump() == "11[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000010}]";
	}},{"rrdata elide next multiple", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000014");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000020");
		rrdata_set::instance i6("000000000000000000000000000000000000000000000000000000000000002F");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		s.debug_add(i1, i2);
		return s.debug_dump() == "75[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000050}]";
	}},{"rrdata elide next multiple (OOR)", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000014");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000020");
		rrdata_set::instance i6("000000000000000000000000000000000000000000000000000000000000002F");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000060");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000070");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		s.debug_add(i7, i8);
		s.debug_add(i1, i2);
		return s.debug_dump() == "91[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000050}{"
			"0000000000000000000000000000000000000000000000000000000000000060,"
			"0000000000000000000000000000000000000000000000000000000000000070}]";
	}},{"rrdata elide next onehalf", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000014");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000020");
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		s.debug_add(i1, i2);
		return s.debug_dump() == "80[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000055}]";
	}},{"rrdata elide next onehalf (OOR)", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000014");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000020");
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000056");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000057");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		s.debug_add(i7, i8);
		s.debug_add(i1, i2);
		return s.debug_dump() == "81[{0000000000000000000000000000000000000000000000000000000000000005,"
			"0000000000000000000000000000000000000000000000000000000000000055}{"
			"0000000000000000000000000000000000000000000000000000000000000056,"
			"0000000000000000000000000000000000000000000000000000000000000057}]";
	}},{"rrdata elide next onehalf and prev", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000014");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000020");
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000006");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		s.debug_add(i7, i8);
		s.debug_add(i1, i2);
		return s.debug_dump() == "84[{0000000000000000000000000000000000000000000000000000000000000001,"
			"0000000000000000000000000000000000000000000000000000000000000055}]";
	}},{"rrdata elide next onehalf and prev (exact)", []() {
		rrdata_set::instance i1("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set::instance i2("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i3("000000000000000000000000000000000000000000000000000000000000000A");
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000014");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000020");
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000001");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000005");
		rrdata_set s;
		s.debug_add(i3, i4);
		s.debug_add(i5, i6);
		s.debug_add(i7, i8);
		s.debug_add(i1, i2);
		return s.debug_dump() == "84[{0000000000000000000000000000000000000000000000000000000000000001,"
			"0000000000000000000000000000000000000000000000000000000000000055}]";
	}},{"In set (empty set, empty data)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		return s.debug_in_set(i6, i6);
	}},{"In set (non empty set, empty data)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		return !s.debug_in_set(i6);
	}},{"In set (empty set, non empty data)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000058");
		s.debug_add(i6);
		return s.debug_in_set(i7, i7);
	}},{"In set (match)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		s.debug_add(i6);
		return s.debug_in_set(i6);
	}},{"In set (adjacent low)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000056");
		s.debug_add(i7);
		return !s.debug_in_set(i6);
	}},{"In set (adjacent)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000056");
		s.debug_add(i6);
		return !s.debug_in_set(i7);
	}},{"In set (match to larger)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000059");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000056");
		s.debug_add(i6, i7);
		return s.debug_in_set(i8);
	}},{"In set (match to larger, exact end)", []() {
		rrdata_set s;
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000059");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000058");
		s.debug_add(i6, i7);
		return s.debug_in_set(i8);
	}},{"In set (match to larger, multiple)", []() {
		rrdata_set s;
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000045");
		rrdata_set::instance i5("0000000000000000000000000000000000000000000000000000000000000049");
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000055");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000059");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000046");
		s.debug_add(i4, i5);
		s.debug_add(i6, i7);
		return s.debug_in_set(i8);
	}},{"In set (split)", []() {
		rrdata_set s;
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000045");
		rrdata_set::instance i5("000000000000000000000000000000000000000000000000000000000000004F");
		rrdata_set::instance i6("0000000000000000000000000000000000000000000000000000000000000050");
		rrdata_set::instance i7("0000000000000000000000000000000000000000000000000000000000000059");
		rrdata_set::instance i8("0000000000000000000000000000000000000000000000000000000000000046");
		rrdata_set::instance i9("0000000000000000000000000000000000000000000000000000000000000053");
		s.debug_add(i4, i5);
		s.debug_add(i6, i7);
		return !s.debug_in_set(i8, i9);
	}},{"Set internal, add internal", []() {
		rrdata_set s;
		rrdata_set::instance i4("0000000000000000000000000000000000000000000000000000000000000045");
		s.set_internal(i4);
		s.add_internal();
		return s.debug_dump() == "1[{0000000000000000000000000000000000000000000000000000000000000045,"
			"0000000000000000000000000000000000000000000000000000000000000046}]";
	}},{"Empty count", []() {
		rrdata_set s;
		return s.count() == 0;
	}},{"count 1 node", []() {
		rrdata_set s;
		s.add_internal();
		return s.count() == 0;
	}},{"count 2 node", []() {
		rrdata_set s;
		s.add_internal();
		s.add_internal();
		return s.count() == 1;
	}},{"count 3 node", []() {
		rrdata_set s;
		s.add_internal();
		s.add_internal();
		s.add_internal();
		return s.count() == 2;
	}},{"Count rrdata #1", []() {
		rrdata_set s;
		std::vector<char> data;
		return s.count(data) == 0;
	}},{"Count rrdata #2", []() {
		rrdata_set s;
		char _data[] = {0x1F,0x00};
		std::vector<char> data(_data, _data + sizeof(_data));
		return s.count(data) == 0;
	}},{"Count rrdata #2", []() {
		rrdata_set s;
		char _data[] = {0x1F,0x00,0x1F,0x02};
		std::vector<char> data(_data, _data + sizeof(_data));
		return s.count(data) == 1;
	}},{"Count rrdata #3", []() {
		rrdata_set s;
		char _data[] = {0x3F,0x01,0x07,0x1F,0x12};
		std::vector<char> data(_data, _data + sizeof(_data));
		return s.count(data) == 9;
	}},{"Count rrdata #4", []() {
		rrdata_set s;
		char _data[] = {0x5F,0x01,0x07,0x45,0x1F,-1};
		std::vector<char> data(_data, _data + sizeof(_data));
		return s.count(data) == 0x847;
	}},{"Count rrdata #5", []() {
		rrdata_set s;
		char _data[] = {0x7F,0x01,0x07,0x45,0x14,0x1F,-1};
		std::vector<char> data(_data, _data + sizeof(_data));
		return s.count(data) == 0x84616;
	}},{"read rrdata #1", []() {
		rrdata_set s;
		char _data[] = {0x7F,0x01,0x07,0x45,0x14,0x1F,-1,0x1F,0x00};
		std::vector<char> data(_data, _data + sizeof(_data));
		s.read(data);
		std::string ans = "542232[{0000000000000000000000000000000000000000000000000000000000000001,"
			"0000000000000000000000000000000000000000000000000000000000084617}{"
			"00000000000000000000000000000000000000000000000000000000000846FF,"
			"0000000000000000000000000000000000000000000000000000000000084701}]";
		return s.debug_dump() == ans;
	}},{"read/write rrdata #1", []() {
		rrdata_set s;
		char _data[] = {0x7F,0x01,0x07,0x45,0x14,0x1F,-1,0x1F,0x00};
		std::vector<char> data(_data, _data + sizeof(_data));
		s.read(data);
		std::vector<char> data2;
		if(s.write(data2) != 0x84617)
			return false;
		char _data2[] = {0x7F,0x01,0x07,0x45,0x14,0x3F,-1,0x00};
		return sizeof(_data2) == data2.size() && !memcmp(_data2, &data2[0], min(sizeof(_data2),
			data2.size()));
	}},{"Write blank", []() {
		rrdata_set s;
		std::vector<char> data2;
		if(s.write(data2))
			return false;
		return data2.size() == 0;
	}},{"Write oversize rrdata run", []() {
		rrdata_set s;
		rrdata_set::instance i;
		s.debug_add(i,i + 0x2000000);
		std::vector<char> data2;
		if(s.write(data2) != 0x1FFFFFF)
			return false;
		char _data2[] = {0x7F,0x00,-1,-1,-1,0x7F,0x01,-3,-3,-3};
		return sizeof(_data2) == data2.size() && !memcmp(_data2, &data2[0], min(sizeof(_data2),
			data2.size()));
	}},{"Decode split", []() {
		rrdata_set s;
		char _data[] = {0x7F,0x00,-1,-1,-1,0x7F,0x01,-3,-3,-3};
		std::vector<char> data(_data, _data + sizeof(_data));
		s.read(data);
		std::string ans = "33554432[{0000000000000000000000000000000000000000000000000000000000000000,"
			"0000000000000000000000000000000000000000000000000000000002000000}]";
		return s.debug_dump() == ans;
	}},{"Write 0-byte length", []() {
		rrdata_set s;
		rrdata_set::instance i;
		s.debug_add(i);
		std::vector<char> data2;
		if(s.write(data2) != 0)
			return false;
		char _data2[] = {0x1F,0x00};
		return sizeof(_data2) == data2.size() && !memcmp(_data2, &data2[0], min(sizeof(_data2),
			data2.size()));
	}},{"Write 1-byte length", []() {
		rrdata_set s;
		rrdata_set::instance i;
		s.debug_add(i,i + 0x100);
		std::vector<char> data2;
		if(s.write(data2) != 0xFF)
			return false;
		char _data2[] = {0x3F,0x00,-2};
		return sizeof(_data2) == data2.size() && !memcmp(_data2, &data2[0], min(sizeof(_data2),
			data2.size()));
	}},{"Write 2-byte length", []() {
		rrdata_set s;
		rrdata_set::instance i;
		s.debug_add(i,i + 0x10000);
		std::vector<char> data2;
		if(s.write(data2) != 0xFFFF)
			return false;
		char _data2[] = {0x5F,0x00,-2,-2};
		return sizeof(_data2) == data2.size() && !memcmp(_data2, &data2[0], min(sizeof(_data2),
			data2.size()));
	}},{"Write 2-byte length #2", []() {
		rrdata_set s;
		rrdata_set::instance i;
		s.debug_add(i + 0x1424,i + 0x11425);
		std::vector<char> data2;
		if(s.write(data2) != 0x10000)
			return false;
		char _data2[] = {0x5E,0x14, 0x24,-2,-1};
		return sizeof(_data2) == data2.size() && !memcmp(_data2, &data2[0], min(sizeof(_data2),
			data2.size()));
	}},{"Basic rrdata with backing file", []() {
		rrdata_set s;
		unlink("foo.tmp");
		s.read_base("foo.tmp", false);
		s.set_internal(rrdata_set::instance(
			"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCD0A"));
		if(get_file_size("foo.tmp") != 0)
			return false;
		s.add_internal();
		if(get_file_size("foo.tmp") != 32)
			return false;
		s.add_internal();
		if(get_file_size("foo.tmp") != 64)
			return false;
		s.add_internal();
		if(get_file_size("foo.tmp") != 96)
			return false;
		s.add_internal();
		if(get_file_size("foo.tmp") != 128)
			return false;
		s.add_internal();
		if(get_file_size("foo.tmp") != 160)
			return false;
		return true;
	}},{"Reopen backing file", []() {
		rrdata_set s;
		unlink("foo.tmp");
		s.read_base("foo.tmp", false);
		s.set_internal(rrdata_set::instance(
			"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCD0A"));
		if(get_file_size("foo.tmp") != 0)
			return false;
		s.add_internal();
		if(get_file_size("foo.tmp") != 32)
			return false;
		s.close();
		s.read_base("foo.tmp", false);
		s.add_internal();
		if(get_file_size("foo.tmp") != 64)
			return false;
		return true;
	}},{"Close with no project", []() {
		rrdata_set s;
		unlink("foo.tmp");
		s.close();
		return true;
	}},{"Switch to self", []() {
		rrdata_set s;
		unlink("foo.tmp");
		s.read_base("foo.tmp", false);
		s.set_internal(rrdata_set::instance(
			"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCD0A"));
		s.add_internal();
		s.read_base("foo.tmp", false);
		s.add_internal();
		return s.count() == 1;
	}},{"Switch to another", []() {
		rrdata_set s;
		unlink("foo.tmp");
		unlink("foo2.tmp");
		s.read_base("foo.tmp", false);
		s.set_internal(rrdata_set::instance(
			"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCD0A"));
		s.add_internal();
		s.read_base("foo2.tmp", false);
		s.add_internal();
		s.add_internal();
		//std::cerr << s.debug_dump() << std::endl;
		return s.count() == 1;
	}},{"Lazy mode", []() {
		rrdata_set s;
		unlink("foo.tmp");
		s.read_base("foo.tmp", true);
		s.set_internal(rrdata_set::instance(
			"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCD0A"));
		s.add_internal();
		s.add_internal();
		s.read_base("foo.tmp", false);
		s.add_internal();
		s.add_internal();
		if(get_file_size("foo.tmp") != 128)
			return false;
		return s.count() == 3;
	}},{"Lazy mode with previous file", []() {
		rrdata_set s;
		unlink("foo.tmp");
		unlink("foo2.tmp");
		s.read_base("foo2.tmp", false);
		s.read_base("foo.tmp", true);
		s.set_internal(rrdata_set::instance(
			"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCD0A"));
		s.add_internal();
		s.add_internal();
		s.read_base("foo.tmp", false);
		s.add_internal();
		s.add_internal();
		if(get_file_size("foo.tmp") != 128)
			return false;
		return s.count() == 3;
	}},{"Reading a file", []() {
		rrdata_set s;
		unlink("foo.tmp");
		s.read_base("foo.tmp", false);
		char _data[] = {0x3F,0x01,0x07,0x1F,0x12};
		std::vector<char> data(_data, _data + sizeof(_data));
		s.read(data);
		if(get_file_size("foo.tmp") != 320)
			return false;
		return true;
	}},{"Reading a file /w existing entry", []() {
		rrdata_set s;
		unlink("foo.tmp");
		rrdata_set::instance i;
		i = i + 5;
		s.add(i);
		s.read_base("foo.tmp", false);
		char _data[] = {0x3F,0x01,0x07,0x1F,0x12};
		std::vector<char> data(_data, _data + sizeof(_data));
		s.read(data);
		if(get_file_size("foo.tmp") != 320)
			return false;
		return true;
	}},
};

int main()
{
	struct test* t = tests;
	while(t->name) {
		std::cout << t->name << "..." << std::flush;
		try {
			if(t->run())
				std::cout << "\e[32mPASS\e[0m" << std::endl;
			else {
				std::cout << "\e[31mFAILED\e[0m" << std::endl;
				return 1;
			}
		} catch(std::exception& e) {
			std::cout << "\e[31mEXCEPTION: " << e.what() << "\e[0m" << std::endl;
			return 1;
		} catch(...) {
			std::cout << "\e[31mUNKNOWN EXCEPTION\e[0m" << std::endl;
			return 1;
		}
		t++;
	}
	return 0;
}
