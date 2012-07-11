#include "library/minmax.hpp"
#include "library/patch.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include <cstdint>
#include <limits>
#include <cstring>
#include <iostream>
#include <zlib.h>

namespace
{
	uint8_t readbyte(const char* buf, uint64_t& pos, uint64_t size)
	{
		if(pos >= size)
			(stringfmt() << "Attempted to read byte past the end of patch (" << pos << " >= "
				<< size << ").").throwex();
		return static_cast<uint8_t>(buf[pos++]);
	}

	uint64_t safe_add(uint64_t a, uint64_t b)
	{
		if(a + b < a)
			(stringfmt() << "Integer overflow (" << a << " + " << b << ") processing patch.").throwex();
		return a + b;
	}

	uint64_t safe_sub(uint64_t a, uint64_t b)
	{
		if(a < b)
			(stringfmt() << "Integer underflow (" << a << " - " << b << ") processing patch.").throwex();
		return a - b;
	}

	uint64_t decode_varint(const char* buf, uint64_t& pos, uint64_t size)
	{
		uint64_t v = 0;
		size_t i;
		uint64_t y;
		for(i = 0; i < 10; i++) {
			y = readbyte(buf, pos, size) ^ 0x80;
			v += (y << (7 * i));
			if(i == 8 && (y | ((v >> 63) ^ 1)) == 255)
				(stringfmt() << "Varint decoding overlows: v=" << v << " y=" << y << ".").throwex();
			if(i == 9 && y > 0)
				(stringfmt() << "Varint decoding overlows: v=" << v << " y=" << y << ".").throwex();
			if(y < 128)
				return v;
		}
	}

	struct bps_patcher : public rom_patcher
	{
		~bps_patcher() throw();
		bool identify(const std::vector<char>& patch) throw();
		void dopatch(std::vector<char>& out, const std::vector<char>& original,
			const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error);
	} bpspatch;

	bps_patcher::~bps_patcher() throw()
	{
	}

	bool bps_patcher::identify(const std::vector<char>& patch) throw()
	{
		return (patch.size() > 4 && patch[0] == 'B' && patch[1] == 'P' && patch[2] == 'S' && patch[3] == '1');
	}

	void bps_patcher::dopatch(std::vector<char>& out, const std::vector<char>& original,
		const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error)
	{
		if(offset)
			(stringfmt() << "Nonzero offsets (" << offset << ") not allowed in BPS mode.").throwex();
		if(patch.size() < 19)
			(stringfmt() << "Patch is too masll to be valid BPS patch (" << patch.size()
				<< " < 19).").throwex();
		uint64_t ioffset = 4;
		const char* _patch = &patch[0];
		size_t psize = patch.size() - 12;
		uint32_t crc_init = crc32(0, NULL, 0);
		uint32_t pchcrc_c = crc32(crc_init, reinterpret_cast<const uint8_t*>(&patch[0]), patch.size() - 4);
		uint32_t pchcrc = read32ule(_patch + psize + 8);
		if(pchcrc_c != pchcrc)
			(stringfmt() << "CRC mismatch on patch: Claimed: " << pchcrc << " Actual: " << pchcrc_c
				<< ".").throwex();
		uint32_t srccrc = read32ule(_patch + psize + 0);
		uint32_t dstcrc = read32ule(_patch + psize + 4);
		uint64_t srcsize = decode_varint(_patch, ioffset, psize);
		uint64_t dstsize = decode_varint(_patch, ioffset, psize);
		uint64_t mdtsize = decode_varint(_patch, ioffset, psize);
		ioffset += mdtsize;
		if(ioffset < mdtsize || ioffset > psize)
			(stringfmt() << "Metadata size invalid: " << mdtsize << "@" << ioffset << ", plimit="
				<< patch.size() << ".").throwex();

		if(srcsize != original.size())
			(stringfmt() << "Size mismatch on original: Claimed: " << srcsize << " Actual: "
				<< original.size() << ".").throwex();
		uint32_t srccrc_c = crc32(crc_init, reinterpret_cast<const uint8_t*>(&original[0]), original.size());
		if(srccrc_c != srccrc)
			(stringfmt() << "CRC mismatch on original: Claimed: " << srccrc << " Actual: " << srccrc_c
				<< ".").throwex();

		out.resize(dstsize);
		uint64_t target_ptr = 0;
		uint64_t source_rptr = 0;
		uint64_t target_rptr = 0;
		while(ioffset < psize) {
			uint64_t opc = decode_varint(_patch, ioffset, psize);
			uint64_t len = (opc >> 2) + 1;
			uint64_t off = (opc & 2) ? decode_varint(_patch, ioffset, psize) : 0;
			bool negative = ((off & 1) != 0);
			off >>= 1;
			if(safe_add(target_ptr, len) > dstsize)
				(stringfmt() << "Illegal write: " << len << "@" << target_ptr << ", wlimit="
					<< dstsize << ".").throwex();
			const char* src;
			size_t srcoffset;
			size_t srclimit;
			const char* msg;
			switch(opc & 3) {
			case 0:
				src = &original[0];
				srcoffset = target_ptr;
				srclimit = srcsize;
				msg = "source";
				break;
			case 1:
				src = &patch[0];
				srcoffset = ioffset;
				srclimit = psize - 12;
				ioffset += len;
				msg = "patch";
				break;
			case 2:
				if(negative)
					source_rptr = safe_sub(source_rptr, off);
				else
					source_rptr = safe_add(source_rptr, off);
				src = &original[0];
				srcoffset = source_rptr;
				srclimit = srcsize;
				source_rptr += len;
				msg = "source";
				break;
			case 3:
				if(negative)
					target_rptr = safe_sub(target_rptr, off);
				else
					target_rptr = safe_add(target_rptr, off);
				src = &out[0];
				srcoffset = target_rptr;
				srclimit = min(dstsize, target_rptr + len);
				target_rptr += len;
				msg = "target";
				break;
			};
			if(safe_add(srcoffset, len) > srclimit)
				(stringfmt() << "Illegal read: " << len << "@" << srcoffset << " from " << msg
					<< ", limit=" << srclimit << ".").throwex();
			for(uint64_t i = 0; i < len; i++)
				out[target_ptr + i] = src[srcoffset + i];
			target_ptr += len;
		}
		if(target_ptr != out.size())
			(stringfmt() << "Size mismatch on result: Claimed: " << out.size() << " Actual: "
				<< target_ptr << ".").throwex();
		uint32_t dstcrc_c = crc32(crc_init, reinterpret_cast<const uint8_t*>(&out[0]), out.size());
		if(dstcrc_c != dstcrc)
			(stringfmt() << "CRC mismatch on result: Claimed: " << dstcrc << " Actual: " << dstcrc_c
				<< ".").throwex();
	}
}
