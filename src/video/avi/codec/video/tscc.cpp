#include "video/avi/codec.hpp"
#include "library/minmax.hpp"
#include "library/zlibstream.hpp"
#include "core/instance.hpp"
#include "core/settings.hpp"
#include <limits>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <stdexcept>

#define CBUFFER 65536

const void* check;

namespace
{
	settingvar::supervariable<settingvar::model_int<0,9>> clvl(lsnes_setgrp, "avi-tscc-compression",
		"AVI‣TSCC‣Compression", 7);
	settingvar::supervariable<settingvar::model_int<0,999999999>> kint(lsnes_setgrp, "avi-tscc-keyint",
		"AVI‣TSCC‣Keyframe interval", 299);

	struct msrle_compressor
	{
	/**
	 * Set the frames involved with compression.
	 *
	 * Parameter thisframe: The frame to compress.
	 * Parameter prevframe: The previous frame. May be NULL to signal no previous frame.
	 * Parameter width: The width of frame.
	 * Parameter height: The height of frame.
	 */
		void setframes(const uint8_t* thisframe, const uint8_t* prevframe, size_t width, size_t height);
	/**
	 * Read compressed data.
	 *
	 * Parameter buffer: The buffer to fill with compressed data. Must be at least 770 bytes.
	 * Returns: Amount of data filled. Calls after image is compressed always return 0.
	 */
		size_t read(uint8_t* buffer);
		const uint8_t* tframe;
	private:
		const uint8_t* pframe;
		size_t fwidth;
		size_t fheight;
		size_t fstride;
		size_t cptr_x;		//Compression pointer x. If fwidth, next thing will be end of line.
		size_t cptr_y;		//Compression pointer y.
		size_t do_end_of_line(uint8_t* buffer);
		size_t do_end_of_picture(uint8_t* buffer);
		size_t do_null_frame(uint8_t* buffer);
		size_t do_xy_delta(uint8_t* buffer, size_t newx, size_t newy);
		size_t do_rle(uint8_t* buffer, size_t count, uint8_t d1, uint8_t d2, uint8_t d3);
		size_t maxrle(const uint8_t* data, size_t bound);
	};

	size_t msrle_compressor::do_rle(uint8_t* buffer, size_t count, uint8_t d1, uint8_t d2, uint8_t d3)
	{
		buffer[0] = min(count, fwidth - cptr_x);
		buffer[1] = d1;
		buffer[2] = d2;
		buffer[3] = d3;
		cptr_x += buffer[0];
		return 4;
	}

	size_t msrle_compressor::maxrle(const uint8_t* d, size_t bound)
	{
		size_t r = 0;
		while(r < bound && d[0] == d[3 * r + 0] && d[1] == d[3 * r + 1] && d[2] == d[3 * r + 2])
			r++;
		return r;
	}

	size_t msrle_compressor::do_end_of_line(uint8_t* buffer)
	{
		buffer[0] = 0;
		buffer[1] = (cptr_y == fheight - 1) ? 1 : 0;
		cptr_x = 0;
		cptr_y++;
		return 2;
	}

	size_t msrle_compressor::do_end_of_picture(uint8_t* buffer)
	{
		buffer[0] = 0;
		buffer[1] = 1;
		cptr_x = 0;
		cptr_y = fheight;
		return 2;
	}

	size_t msrle_compressor::do_null_frame(uint8_t* buffer)
	{
		cptr_x = 0;
		cptr_y = fheight;
		return 0;
	}

	size_t msrle_compressor::do_xy_delta(uint8_t* buffer, size_t newx, size_t newy)
	{
		newx = min(newx, cptr_x + 255);
		newy = min(newy, cptr_y + 255);
		buffer[0] = 0;
		buffer[1] = 2;
		buffer[2] = newx - cptr_x;
		buffer[3] = newy - cptr_y;
		cptr_x = newx;
		cptr_y = newy;
		return 4;
	}

	void msrle_compressor::setframes(const uint8_t* thisframe, const uint8_t* prevframe, size_t width,
		size_t height)
	{
		tframe = thisframe;
		check = tframe;
		pframe = prevframe;
		fwidth = width;
		fheight = height;
		fstride = 3 * width;
		cptr_x = 0;
		cptr_y = 0;
	}

	size_t msrle_compressor::read(uint8_t* buffer)
	{
		//If at end of picture, emit nothing.
		if(cptr_y == fheight)
			return 0;
		//If at end of line, emit end of line / end of picture.
		if(cptr_x == fwidth)
			return do_end_of_line(buffer);
		if(pframe) {
			//See if we can reuse content from previous image.
			size_t cptr = cptr_y * fstride + 3 * cptr_x;
			size_t maxcptr = fheight * fstride;
			while(cptr < maxcptr)
				if(tframe[cptr] != pframe[cptr])
					break;
				else
					cptr++;
			size_t next_x = (cptr % fstride) / 3;
			size_t next_y = cptr / fstride;
			//1) End of picture.
			if(next_y == fheight)
				return do_end_of_picture(buffer);
			//2) emit xy-delta.
			if(next_x >= cptr_x && next_y > cptr_y)
				return do_xy_delta(buffer, next_x, next_y);
			//3) Emit y delta followed by end of line.
			if(next_y > cptr_y + 1) {
				size_t p = do_xy_delta(buffer, cptr_x, next_y - 1);
				return p + do_end_of_line(buffer + p);
			}
			//4) Emit end of line.
			if(next_y == cptr_y + 1)
				return do_end_of_line(buffer);
			//5) Emit X delta.
			if(next_x > cptr_x)
				return do_xy_delta(buffer, next_x, cptr_y);
		}
		//If RLE is possible, do that.
		const uint8_t* d = &tframe[cptr_y * fstride + 3 * cptr_x];
		size_t maxreps = maxrle(d, min(static_cast<size_t>(255), fwidth - cptr_x));
		if(maxreps > 1 || fwidth < cptr_x + 3)	//Also do this if near end of line.
			return do_rle(buffer, maxreps, d[0], d[1], d[2]);
		//If RLE is not possible within 3 positions, emit literial insert.
		size_t maxreps1 = maxrle(d + 3, min(static_cast<size_t>(255), fwidth - cptr_x - 1));
		size_t maxreps2 = maxrle(d + 6, min(static_cast<size_t>(255), fwidth - cptr_x - 2));
		if(maxreps1 == 1 && maxreps2 == 1) {
			//TODO.
		}
		//Fallback.
		return do_rle(buffer, 1, d[0], d[1], d[2]);
	}

	struct avi_codec_tscc : public avi_video_codec
	{
		avi_codec_tscc(unsigned level, unsigned _keyint);
		~avi_codec_tscc();
		avi_video_codec::format reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d);
		void frame(uint32_t* data, uint32_t stride);
		bool ready();
		avi_packet getpacket();
	private:
		void readrow(uint32_t* rptr);
		avi_packet out;
		bool ready_flag;
		unsigned iwidth;
		unsigned iheight;
		unsigned ewidth;
		unsigned eheight;
		unsigned pframes;
		unsigned max_pframes;
		unsigned level;
		uint64_t frameno;
		zlibstream z;
		std::vector<uint8_t> _frame;
		std::vector<uint8_t> prevframe;
	};

	unsigned getzlevel(uint32_t _level)
	{
		if(_level < 0 || _level > 9)
			throw std::runtime_error("Invalid compression level");
		return _level;
	}

	avi_codec_tscc::avi_codec_tscc(unsigned compression, unsigned _keyint)
		: z(getzlevel(compression))
	{
		max_pframes = _keyint;
		frameno = 0;
	}

	avi_codec_tscc::~avi_codec_tscc()
	{
	}

	avi_video_codec::format avi_codec_tscc::reset(uint32_t width, uint32_t height, uint32_t fps_n,
		uint32_t fps_d)
	{
		pframes = std::numeric_limits<unsigned>::max();	//Next frame has to be keyframe.
		iwidth = width;
		iheight = height;
		ewidth = (iwidth + 3) >> 2 << 2;
		eheight = (iheight + 3) >> 2 << 2;
		ready_flag = true;
		_frame.resize(3 * ewidth * eheight);
		prevframe.resize(3 * ewidth * eheight);
		memset(&_frame[0], 0, 3 * ewidth * eheight);
		avi_video_codec::format fmt(ewidth, eheight, 0x43435354, 24);
		return fmt;
	}

	//24-bit MSRLE variant:
	//00 00: End of line.
	//00 01: End of bitmap.
	//00 02 xx yy: Move x pixels right, y pixels down.
	//00 03-FF <pixels>: 3-255 literal pixels (determined by the second byte).
	//01-FF <pixel>: 1-255 repetions of pixel (determined by the first byte).

	void avi_codec_tscc::frame(uint32_t* data, uint32_t stride)
	{
		msrle_compressor c;
		bool keyframe = false;
		if(pframes >= max_pframes) {
			keyframe = true;
			pframes = 0;
		} else
			pframes++;

		//Reduce the frame to rgb24.
		for(uint32_t y = eheight - iheight; y < eheight; y++) {
			const uint32_t* rptr = data + (eheight - y - 1) * stride;
			for(uint32_t i = 0; i < iwidth; i++) {
				_frame[3 * y * ewidth + 3 * i + 0] = rptr[i] >> 16;
				_frame[3 * y * ewidth + 3 * i + 1] = rptr[i] >> 8;
				_frame[3 * y * ewidth + 3 * i + 2] = rptr[i] >> 0;
			}
		}

		//NULL frames are disabled for now. Even without these, no-change frames are very small,
		//as those are just end of frame code.
		//if(!keyframe && !memcmp(&_frame[0], &prevframe[0], 3 * ewidth * eheight)) {
		//	//Special: NULL frame.
		//	out.payload.resize(0);
		//	out.typecode = 0x6264;
		//	out.hidden = false;
		//	out.indexflags = 0x00;
		//	ready_flag = false;
		//	return;
		//}

		//This codec is extended version of MSRLE followed by deflate.
		c.setframes(&_frame[0], keyframe ? NULL : &prevframe[0], ewidth, eheight);
		size_t block;
		unsigned char buffer[CBUFFER];
		size_t blockused = 0;
		z.reset(NULL, 0);
		while(!z.get_flag()) {
			block = c.read(buffer + blockused);
			if(!block)
				z.set_flag(true);
			blockused += block;
			if((blockused + 770 > CBUFFER) || z.get_flag()) {
				z.write(buffer, blockused);
				blockused = 0;
			}
		}
		z.read(out.payload);
		memcpy(&prevframe[0], &_frame[0], 3 * ewidth * eheight);
		out.typecode = 0x6264;
		out.hidden = false;
		out.indexflags = keyframe ? 0x10 : 0x00;
		ready_flag = false;
	}

	bool avi_codec_tscc::ready()
	{
		return ready_flag;
	}

	avi_packet avi_codec_tscc::getpacket()
	{
		ready_flag = true;
		return out;
	}

	avi_video_codec_type rgb("tscc", "TSCC video codec",
		[]() -> avi_video_codec* {
			return new avi_codec_tscc(clvl(*CORE().settings), kint(*CORE().settings));
		});
}
