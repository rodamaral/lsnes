#include "video/avi/codec.hpp"
#include "core/instance.hpp"
#include "core/settings.hpp"
#include "library/zlibstream.hpp"
#include <limits>
#include <cstring>
#include <cerrno>
#include <stdexcept>

#define CBUFFER 16384

namespace
{
	settingvar::supervariable<settingvar::model_int<0,9>> clvl(lsnes_setgrp, "avi-cscd-compression",
		"AVI‣CSCD‣Compression", 7);
	settingvar::supervariable<settingvar::model_int<0,999999999>> kint(lsnes_setgrp, "avi-cscd-keyint",
		"AVI‣CSCD‣Keyframe interval", 0);

	struct avi_codec_cscd : public avi_video_codec
	{
		avi_codec_cscd(uint32_t _level, uint32_t maxpframes);
		~avi_codec_cscd();
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
		zlibstream z;
		std::vector<uint8_t> row;
		std::vector<uint8_t> prevframe;
	};

	avi_codec_cscd::~avi_codec_cscd()
	{
	}

	unsigned getzlevel(uint32_t _level)
	{
		if(_level < 0 || _level > 9)
			throw std::runtime_error("Invalid compression level");
		return _level;
	}

	avi_codec_cscd::avi_codec_cscd(uint32_t _level, uint32_t maxpframes)
		: z(getzlevel(_level))
	{
		level = _level;
		max_pframes = maxpframes;
	}

	avi_video_codec::format avi_codec_cscd::reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d)
	{
		pframes = std::numeric_limits<unsigned>::max();	//Next frame has to be keyframe.
		iwidth = width;
		iheight = height;
		ewidth = (iwidth + 3) >> 2 << 2;
		eheight = (iheight + 3) >> 2 << 2;
		ready_flag = true;
		row.resize(3 * ewidth);
		prevframe.resize(3 * ewidth * eheight);
		memset(&row[0], 0, 3 * ewidth);
		memset(&prevframe[0], 0, 3 * ewidth * eheight);
		avi_video_codec::format fmt(ewidth, eheight, 0x44435343, 24);
		return fmt;
	}

	void avi_codec_cscd::frame(uint32_t* data, uint32_t stride)
	{
		bool keyframe = false;
		if(pframes >= max_pframes) {
			keyframe = true;
			pframes = 0;
		} else
			pframes++;

		unsigned char h[2];
		h[0] = (keyframe ? 0x3 : 0x2) | (level << 4);
		h[1] = 8;		//RGB24.
		z.reset(h, 2);

		for(uint32_t y = 0; y < eheight; y++) {
			if(y < eheight - iheight)
				readrow(NULL);
			else
				readrow(data + (eheight - y - 1) * stride);
			if(keyframe) {
				memcpy(&prevframe[3 * y * ewidth], &row[0], 3 * ewidth);
			} else {
				//Ew, we need to have prevframe = row, row = row - prevframe at the same time.
				for(unsigned i = 0; i < 3 * ewidth; i++) {
					uint8_t tmp = row[i];
					row[i] -= prevframe[3 * y * ewidth + i];
					prevframe[3 * y * ewidth + i] = tmp;
				}
			}
			z.write(&row[0], row.size());
		}
		z.read(out.payload);
		out.typecode = 0x6264;		//Not exactly correct according to specs...
		out.hidden = false;
		out.indexflags = keyframe ? 0x10 : 0;
		ready_flag = false;
	}

	bool avi_codec_cscd::ready()
	{
		return ready_flag;
	}

	avi_packet avi_codec_cscd::getpacket()
	{
		ready_flag = true;
		return out;
	}

	void avi_codec_cscd::readrow(uint32_t* rptr)
	{
		if(!rptr)
			memset(&row[0], 0, 3 * iwidth);
		else
			for(uint32_t i = 0; i < iwidth; i++) {
				row[3 * i + 0] = rptr[i] >> 16;
				row[3 * i + 1] = rptr[i] >> 8;
				row[3 * i + 2] = rptr[i] >> 0;
			}
	}

	avi_video_codec_type rgb("cscd", "Camstudio video codec",
		[]() -> avi_video_codec* {
			return new avi_codec_cscd(clvl(*CORE().settings), kint(*CORE().settings));
		});
}
