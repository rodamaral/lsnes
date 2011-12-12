#include "core/patchrom.hpp"
#include "core/misc.hpp"

#include <nall/ips.hpp>
#include <nall/bps/patch.hpp>
#include <cstring>

namespace
{
	void throw_bps_error(nall::bpspatch::result r)
	{
		switch(r)
		{
		case nall::bpspatch::unknown:
			throw std::runtime_error("Unknown error status");
		case nall::bpspatch::success:
			break;
		case nall::bpspatch::patch_too_small:
			throw std::runtime_error("Patch too small to be valid");
		case nall::bpspatch::patch_invalid_header:
			throw std::runtime_error("Patch has invalid header");
		case nall::bpspatch::source_too_small:
			throw std::runtime_error("Source file is too small");
		case nall::bpspatch::target_too_small:
			throw std::runtime_error("INTERNAL ERROR: Target file is too small");
		case nall::bpspatch::source_checksum_invalid:
			throw std::runtime_error("Source file fails CRC check");
		case nall::bpspatch::target_checksum_invalid:
			throw std::runtime_error("Result fails CRC check");
		case nall::bpspatch::patch_checksum_invalid:
			throw std::runtime_error("Corrupt patch file");
		default:
			throw std::runtime_error("Unknown error applying patch");
		};
	}

	std::vector<char> do_patch_bps(const std::vector<char>& original, const std::vector<char>& patch,
		int32_t offset) throw(std::bad_alloc, std::runtime_error)
	{
		if(offset)
			throw std::runtime_error("Offsets are not supported for .bps patches");
		std::vector<char> _original = original;
		std::vector<char> _patch = patch;
		nall::bpspatch p;

		p.source(reinterpret_cast<uint8_t*>(&_original[0]), _original.size());
		p.modify(reinterpret_cast<uint8_t*>(&_patch[0]), _patch.size());

		//Do trial apply to get the size.
		uint8_t tmp;
		p.target(&tmp, 1);
		nall::bpspatch::result r = p.apply();
		if(r == nall::bpspatch::success) {
			//Fun, the output is 0 or 1 bytes.
			std::vector<char> ret;
			ret.resize(p.size());
			memcpy(&ret[0], &tmp, p.size());
			return ret;
		} else if(r != nall::bpspatch::target_too_small) {
			//This is actual error in patch.
			throw_bps_error(r);
		}
		size_t tsize = p.size();

		//Okay, do it for real.
		std::vector<char> ret;
		ret.resize(tsize);
		p.source(reinterpret_cast<uint8_t*>(&_original[0]), _original.size());
		p.modify(reinterpret_cast<uint8_t*>(&_patch[0]), _patch.size());
		p.target(reinterpret_cast<uint8_t*>(&ret[0]), tsize);
		r = p.apply();
		if(r != nall::bpspatch::success)
			throw_bps_error(r);
		return ret;
	}

	std::pair<size_t, size_t> rewrite_ips_record(std::vector<char>& _patch, size_t woffset,
		const std::vector<char>& patch, size_t roffset, int32_t offset)
	{
		if(patch.size() < roffset + 3)
			throw std::runtime_error("Patch incomplete");
		uint32_t a, b, c;
		a = static_cast<uint8_t>(patch[roffset + 0]);
		b = static_cast<uint8_t>(patch[roffset + 1]);
		c = static_cast<uint8_t>(patch[roffset + 2]);
		uint32_t rec_off = a * 65536 + b * 256 + c;
		if(rec_off == 0x454F46) {
			//EOF.
			memcpy(&_patch[woffset], "EOF", 3);
			return std::make_pair(3, 3);
		}
		if(patch.size() < roffset + 5)
			throw std::runtime_error("Patch incomplete");
		a = static_cast<uint8_t>(patch[roffset + 3]);
		b = static_cast<uint8_t>(patch[roffset + 4]);
		uint32_t rec_size = a * 256 + b;
		uint32_t rec_rlesize = 0;
		uint32_t rec_rawsize = 0;
		if(!rec_size) {
			if(patch.size() < roffset + 8)
				throw std::runtime_error("Patch incomplete");
			a = static_cast<uint8_t>(patch[roffset + 5]);
			b = static_cast<uint8_t>(patch[roffset + 6]);
			rec_rlesize = a * 256 + b;
			rec_rawsize = 8;
		} else
			rec_rawsize = 5 + rec_size;
		int32_t rec_noff = rec_off + offset;
		if(rec_noff > 0xFFFFFF)
			throw std::runtime_error("Offset exceeds IPS 16MiB limit");
		if(rec_noff < 0) {
			//This operation needs to clip the start as it is out of bounds.
			if(rec_size) {
				if(rec_size > -rec_noff) {
					rec_noff = 0;
					rec_size -= -rec_noff;
				} else
					rec_size = 0;
			}
			if(rec_rlesize) {
				if(rec_rlesize > -rec_noff) {
					rec_noff = 0;
					rec_rlesize -= -rec_noff;
				} else
					rec_rlesize = 0;
			}
			if(!rec_size && !rec_rlesize)
				return std::make_pair(0, rec_rawsize);		//Completely out of bounds.
		}
		//Write the modified record.
		_patch[woffset + 0] = (rec_noff >> 16);
		_patch[woffset + 1] = (rec_noff >> 8);
		_patch[woffset + 2] = rec_noff;
		_patch[woffset + 3] = (rec_size >> 8);
		_patch[woffset + 4] = rec_size;
		if(rec_size == 0) {
			//RLE.
			_patch[woffset + 5] = (rec_rlesize >> 8);
			_patch[woffset + 6] = rec_rlesize;
			_patch[woffset + 7] = patch[roffset + 7];
			return std::make_pair(8, 8);
		} else
			memcpy(&_patch[woffset + 5], &patch[roffset + rec_rawsize - rec_size], rec_size);
		return std::make_pair(5 + rec_size, rec_rawsize);
	}

	std::vector<char> rewrite_ips_offset(const std::vector<char>& patch, int32_t offset)
	{
		size_t wsize = 5;
		size_t roffset = 5;
		if(patch.size() < 5)
			throw std::runtime_error("IPS file doesn't even have magic");
		std::vector<char> _patch;
		//The result is at most the size of the original.
		_patch.resize(patch.size());
		memcpy(&_patch[0], "PATCH", 5);
		while(true) {
			std::pair<size_t, size_t> r = rewrite_ips_record(_patch, wsize, patch, roffset, offset);
			wsize += r.first;
			roffset += r.second;
			if(r.first == 3)
				break;	//EOF.
		}
		_patch.resize(wsize);
		return _patch;
	}

	std::vector<char> do_patch_ips(const std::vector<char>& original, const std::vector<char>& patch,
		int32_t offset) throw(std::bad_alloc, std::runtime_error)
	{
		std::vector<char> _original = original;
		std::vector<char> _patch = rewrite_ips_offset(patch, offset);
		nall::ips p;
		p.source(reinterpret_cast<uint8_t*>(&_original[0]), _original.size());
		p.modify(reinterpret_cast<uint8_t*>(&_patch[0]), _patch.size());
		if(!p.apply())
			throw std::runtime_error("Error applying IPS patch");
		std::vector<char> ret;
		ret.resize(p.size);
		memcpy(&ret[0], p.data, p.size);
		//No, these can't be freed.
		p.source(NULL, 0);
		p.modify(NULL, 0);
		return ret;
	}
}

std::vector<char> do_patch_file(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	if(patch.size() > 5 && patch[0] == 'P' && patch[1] == 'A' && patch[2] == 'T' && patch[3] == 'C' &&
		patch[4] == 'H')
		return do_patch_ips(original, patch, offset);
	else if(patch.size() > 4 && patch[0] == 'B' && patch[1] == 'P' && patch[2] == 'S' && patch[3] == '1')
		return do_patch_bps(original, patch, offset);
	else
		throw std::runtime_error("Unknown patch file format");
}
