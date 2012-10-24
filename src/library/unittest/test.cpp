#include "library/bintohex.hpp"
#include <iostream>

struct testcase
{
	const char* name;
	bool (*run)();
};

struct testcase tests[] = {
	"Bintohex on empty array", []() -> bool {
		uint8_t data[1];
		std::string x = binary_to_hex(data, 0);
		return (x == "");
	},
	"Bintohex on empty NULL array", []() -> bool {
		std::string x = binary_to_hex(NULL, 0);
		return (x == "");
	},
	"Bintohex one byte", []() -> bool {
		uint8_t data[1] = {81};
		std::string x = binary_to_hex(data, 1);
		return (x == "51");
	},
	"Bintohex four bytes", []() -> bool {
		uint8_t data[4] = {0x14, 0x73, 0x2F, 0x4D};
		std::string x = binary_to_hex(data, 4);
		return (x == "14732f4d");
	},
};

int main()
{
	uint64_t total = 0, pass = 0, fail = 0;
	for(size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		std::cout << "Test #" << (i + 1) << ": " << tests[i].name << "..." << std::flush;
		bool res = tests[i].run();
		if(res) {
			std::cout << "\e[1;32mOK\e[0m" << std::endl;
			pass++;
		} else {
			std::cout << "\e[1;31mFAIL\e[0m" << std::endl;
			fail++;
		}
		total++;
	}
	std::cout << "Total: " << total << ", pass: " << pass << " fail: " << fail << std::endl;
	return fail ? 1 : 0;
}
