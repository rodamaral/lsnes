#include "minmax.hpp"
#include "fileimage-patch.hpp"
#include "serialization.hpp"
#include "string.hpp"
#include <cstdint>
#include <limits>
#include <cstring>
#include <iostream>

namespace fileimage
{
namespace
{
	uint8_t readbyte(const char* buf, uint64_t& pos, uint64_t size)
	{
		if(pos >= size)
			(stringfmt() << "Attempted to read byte past the end of patch (" << pos << " >= "
				<< size << ").").throwex();
		return static_cast<uint8_t>(buf[pos++]);
	}

	struct ips_patcher : public patcher
	{
		~ips_patcher() throw();
		bool identify(const std::vector<char>& patch) throw();
		void dopatch(std::vector<char>& out, const std::vector<char>& original,
			const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error);
	} ipspatch;

	ips_patcher::~ips_patcher() throw()
	{
	}

	bool ips_patcher::identify(const std::vector<char>& patch) throw()
	{
		return (patch.size() > 5 && patch[0] == 'P' && patch[1] == 'A' && patch[2] == 'T' &&
			patch[3] == 'C' && patch[4] == 'H');
	}

	void ips_patcher::dopatch(std::vector<char>& out, const std::vector<char>& original,
		const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error)
	{
		//Initial guess.
		out = original;
		const char* _patch = &patch[0];
		size_t psize = patch.size();

		uint64_t ioffset = 5;
		while(true) {
			bool rle = false;
			uint8_t b;
			uint32_t off = 0, l = 0;
			off |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize)) << 16;
			off |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize)) << 8;
			off |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize));
			if(off == 0x454F46)
				break;	//EOF code.
			l |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize)) << 8;
			l |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize));
			if(l == 0) {
				//RLE.
				l |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize)) << 8;
				l |= static_cast<uint32_t>(readbyte(_patch, ioffset, psize));
				b = readbyte(_patch, ioffset, psize);
				rle = true;
			}
			uint64_t extra = 0;
			if(offset >= 0)
				off += offset;
			else {
				uint32_t noffset = static_cast<uint32_t>(-offset);
				uint32_t fromoff = min(noffset, off);
				off -= fromoff;
				extra = min(noffset - fromoff, l);
				l -= extra;
			}
			if(off + l >= out.size())
				out.resize(off + l);
			if(!rle) {
				ioffset += extra;
				for(uint64_t i = 0; i < l; i++)
					out[off + i] = readbyte(_patch, ioffset, psize);
			} else
				for(uint64_t i = 0; i < l; i++)
					out[off + i] = b;
		}
	}
}
}