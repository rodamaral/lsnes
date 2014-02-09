#include "video/avi/codec.hpp"
#include <limits>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <zlib.h>

#define CBUFFER 65536

namespace
{
	struct avi_codec_uncompressed : public avi_video_codec
	{
		~avi_codec_uncompressed();
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
		std::vector<uint8_t> row;
	};


	avi_codec_uncompressed::~avi_codec_uncompressed()
	{
	}

	avi_video_codec::format avi_codec_uncompressed::reset(uint32_t width, uint32_t height, uint32_t fps_n,
		uint32_t fps_d)
	{
		iwidth = width;
		iheight = height;
		ewidth = (iwidth + 3) >> 2 << 2;
		eheight = (iheight + 3) >> 2 << 2;
		ready_flag = true;
		row.resize(3 * ewidth);
		memset(&row[0], 0, 3 * ewidth);
		avi_video_codec::format fmt(ewidth, eheight, 0, 24);	//Is 0 correct value for compression?
		return fmt;
	}

	void avi_codec_uncompressed::frame(uint32_t* data, uint32_t stride)
	{
		out.payload.resize(3 * ewidth * eheight);

		for(uint32_t y = 0; y < eheight; y++) {
			if(y < eheight - iheight)
				readrow(NULL);
			else
				readrow(data + (eheight - y - 1) * stride);
			memcpy(&out.payload[3 * ewidth * y], &row[0], 3 * ewidth);
		}
		out.typecode = 0x6264;
		out.hidden = false;
		out.indexflags = 0x10;
		ready_flag = false;
	}

	bool avi_codec_uncompressed::ready()
	{
		return ready_flag;
	}

	avi_packet avi_codec_uncompressed::getpacket()
	{
		ready_flag = true;
		return out;
	}

	void avi_codec_uncompressed::readrow(uint32_t* rptr)
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

	avi_video_codec_type rgb("uncompressed", "Uncompressed video",
		[]() -> avi_video_codec* { return new avi_codec_uncompressed;});
}
