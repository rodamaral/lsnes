#include "patch.hpp"
#include "sha256.hpp"
#include "string.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>

namespace
{
	std::set<rom_patcher*>& patchers()
	{
		static std::set<rom_patcher*> t;
		return t;
	}
}

std::vector<char> do_patch_file(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	std::vector<char> out;
	for(auto i : patchers())
		if(i->identify(patch)) {
			i->dopatch(out, original, patch, offset);
			return out;
		}
	throw std::runtime_error("Unknown patch file format");
}

rom_patcher::rom_patcher() throw(std::bad_alloc)
{
	patchers().insert(this);
}

rom_patcher::~rom_patcher() throw()
{
	patchers().erase(this);
}
