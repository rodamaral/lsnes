#include "fileimage-patch.hpp"
#include "sha256.hpp"
#include "string.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>

namespace fileimage
{
namespace
{
	std::set<patcher*>& patchers()
	{
		static std::set<patcher*> t;
		return t;
	}
}

std::vector<char> patch(const std::vector<char>& original, const std::vector<char>& patch,
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

patcher::patcher() throw(std::bad_alloc)
{
	patchers().insert(this);
}

patcher::~patcher() throw()
{
	patchers().erase(this);
}
}