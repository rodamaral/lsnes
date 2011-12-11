#include "core/patchrom.hpp"
#include "core/misc.hpp"

#include <cstring>

rom_patcher::rom_patcher(const std::vector<char>& _original, size_t size) throw(std::bad_alloc)
	: original(_original)
{
	target.resize(size);
	offset = 0;
}

rom_patcher::~rom_patcher() throw()
{
}

void rom_patcher::resize_request(size_t dstpos, size_t reqsize)
{
	int64_t _dstpos = dstpos;
	if(_dstpos + reqsize + offset < 0)
		return;
	size_t rsize = _dstpos + reqsize + offset;
	size_t csize = target.size();
	if(rsize > csize) {
		target.resize(rsize);
		for(size_t i = csize; i < rsize; i++)
			target[i] = 0;
	}
}

void rom_patcher::literial_insert(size_t pos, const char* buf, size_t bufsize, size_t times) throw(std::bad_alloc)
{
	int64_t _pos = pos;
	const char* obuf = buf;
	size_t obufsize = bufsize;
	resize_request(pos, bufsize);
	while(bufsize > 0) {
		if(_pos + offset > 0)
			target[_pos + offset] = *buf;
		else 
			do_oob_write_warning();
		_pos++;
		buf++;
		bufsize--;
		if(bufsize == 0 && times > 0) {
			buf = obuf;
			bufsize = obufsize;
		}
	}
}

void rom_patcher::change_size(size_t size) throw(std::bad_alloc)
{
	target.resize(size);
}

void rom_patcher::set_offset(int32_t _offset) throw()
{
	offset = _offset;
}

std::vector<char> rom_patcher::get_output() throw(std::bad_alloc)
{
	return target;
}

void rom_patcher::copy_source(size_t srcpos, size_t dstpos, size_t size) throw(std::bad_alloc)
{
	resize_request(dstpos, size);
	int64_t _srcpos = srcpos;
	int64_t _dstpos = dstpos;
	while(size > 0) {
		if(_dstpos + offset) {
			char byte = 0;
			if(_srcpos + offset >= 0 && _srcpos + offset < original.size())
				byte = original[_srcpos + offset];
			else
				do_oob_read_warning();
			target[_dstpos + offset] = byte;
		}
		_srcpos++;
		_dstpos++;
		size--;
	}
}

void rom_patcher::copy_destination(size_t srcpos, size_t dstpos, size_t size) throw(std::bad_alloc)
{
	int64_t _srcpos = srcpos;
	int64_t _dstpos = dstpos;
	resize_request(dstpos, size);
	while(size > 0) {
		if(_dstpos + offset) {
			char byte = 0;
			if(_srcpos + offset >= 0 && _srcpos + offset < target.size())
				byte = target[_srcpos + offset];
			else
				do_oob_read_warning();
			target[_dstpos + offset] = byte;
		}
		_srcpos++;
		_dstpos++;
		size--;
	}
}

void rom_patcher::do_oob_read_warning() throw()
{
	if(oob_read_warning)
		return;
	messages << "WARNING: Patch copy read out of bounds (this is likely not going to work)" << std::endl;
	oob_read_warning = true;
}

void rom_patcher::do_oob_write_warning() throw()
{
	if(oob_write_warning)
		return;
	messages << "WARNING: Patch write out of bounds (this is likely not going to work)" << std::endl;
	oob_write_warning = true;
}


namespace
{
	size_t handle_ips_record(rom_patcher& r, const std::vector<char>& patch, size_t roffset)
	{
		if(patch.size() < roffset + 3)
			throw std::runtime_error("Unexpected end file in middle of IPS record");
		uint32_t a = static_cast<uint8_t>(patch[roffset + 0]);
		uint32_t b = static_cast<uint8_t>(patch[roffset + 1]);
		uint32_t c = static_cast<uint8_t>(patch[roffset + 2]);
		uint32_t offset = a * 65536 + b * 256 + c;
		if(offset == 0x454F46)
			return 0;
		if(patch.size() < roffset + 5)
			throw std::runtime_error("Unexpected end file in middle of IPS record");
		a = static_cast<uint8_t>(patch[roffset + 3]);
		b = static_cast<uint8_t>(patch[roffset + 4]);
		uint32_t size = a * 256 + b;
		if(size == 0) {
			//RLE.
			if(patch.size() < roffset + 8)
				throw std::runtime_error("Unexpected end file in middle of IPS record");
			a = static_cast<uint8_t>(patch[roffset + 5]);
			b = static_cast<uint8_t>(patch[roffset + 6]);
			size = a * 256 + b;
			r.literial_insert(offset, &patch[roffset + 7], 1, size);
			return roffset + 8;
		} else {
			//Literial.
			if(patch.size() < roffset + 5 + size)
				throw std::runtime_error("Unexpected end file in middle of IPS record");
			r.literial_insert(offset, &patch[roffset + 5], size);
			return roffset + 5 + size;
		}
	}

	std::vector<char> do_patch_ips(const std::vector<char>& original, const std::vector<char>& patch,
		int32_t offset) throw(std::bad_alloc, std::runtime_error)
	{
		rom_patcher r(original);
		//IPS implicitly starts from original.
		r.copy_source(0, 0, original.size());
		r.set_offset(offset);
		size_t roffset = 5;
		while((roffset = handle_ips_record(r, patch, roffset)));
		return r.get_output();
	}
}

std::vector<char> do_patch_file(const std::vector<char>& original, const std::vector<char>& patch,
	int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	if(patch.size() > 5 && patch[0] == 'P' && patch[1] == 'A' && patch[2] == 'T' && patch[3] == 'C' &&
		patch[4] == 'H') {
		return do_patch_ips(original, patch, offset);
	} else {
		throw std::runtime_error("Unknown patch file format");
	}
}
