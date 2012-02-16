#include "video/avi/codec.hpp"
#include "core/settings.hpp"
#include <limits>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <zlib.h>

#define CBUFFER 16384

namespace
{
	numeric_setting clvl("avi-cscd-compression", 0, 9, 7);
	numeric_setting kint("avi-cscd-keyint", 1, 999999999, 0);

	struct avi_codec_cscd : public avi_video_codec
	{
		avi_codec_cscd(uint32_t _level, uint32_t maxpframes);
		~avi_codec_cscd();
		avi_video_codec::format reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d);
		void frame(uint32_t* data);
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
		std::vector<uint8_t> row;
		std::vector<uint8_t> ctmp;
		std::vector<uint8_t> prevframe;
	};

	avi_codec_cscd::~avi_codec_cscd()
	{
	}

	avi_codec_cscd::avi_codec_cscd(uint32_t _level, uint32_t maxpframes)
	{
		if(_level < 0 || _level > 9)
			throw std::runtime_error("Invalid compression level");
		level = _level;
		max_pframes = maxpframes;
		ctmp.resize(CBUFFER);
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

	void avi_codec_cscd::frame(uint32_t* data)
	{
		z_stream zlib;
		bool buffer_loaded = false;
		memset(&zlib, 0, sizeof(zlib));
		int r = deflateInit(&zlib, level);
		switch(r) {
		case Z_ERRNO:
			throw std::runtime_error(strerror(errno));
		case Z_STREAM_ERROR:
			throw std::runtime_error("Illegal compression level");
		case Z_DATA_ERROR:
			throw std::runtime_error("Data error while initializing zlib state?");
		case Z_MEM_ERROR:
			throw std::bad_alloc();
		case Z_BUF_ERROR:
			throw std::runtime_error("Buffer error while initializing zlib state?");
		case Z_VERSION_ERROR:
			throw std::runtime_error("Zlib is FUBAR");
		case Z_OK:
			break;
		default:
			throw std::runtime_error("Unkonwn error from deflateInit");
		};
	
		bool keyframe = false;
		if(pframes >= max_pframes) {
			keyframe = true;
			pframes = 0;
		} else
			pframes++;

		out.payload.resize(2);
		out.payload[0] = (keyframe ? 0x3 : 0x2) | (level << 4);
		out.payload[1] = 8;		//RGB24.

		for(uint32_t y = 0; y < eheight; y++) {
			bool done = true;
			if(y < eheight - iheight)
				readrow(NULL);
			else
				readrow(data + (eheight - y - 1) * iwidth);
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
			zlib.next_in = &row[0];
			zlib.avail_in = row.size();
			if(y == eheight - 1)
				done = false;
			while(zlib.avail_in || !done) {
				//Make space in output buffer.
				if(!zlib.avail_out) {
					if(buffer_loaded) {
						size_t p = out.payload.size();
						out.payload.resize(p + ctmp.size());
						memcpy(&out.payload[p], &ctmp[0], ctmp.size());
					}
					zlib.next_out = &ctmp[0];
					zlib.avail_out = ctmp.size();
					buffer_loaded = true;
				}
				r = deflate(&zlib, (y == eheight - 1) ? Z_FINISH : 0);
				if(r == Z_STREAM_END)
					done = true;
			}
		}
		if(buffer_loaded) {
			size_t p = out.payload.size();
			out.payload.resize(p + (ctmp.size() - zlib.avail_out));
			memcpy(&out.payload[p], &ctmp[0], ctmp.size() - zlib.avail_out);
		}
		deflateEnd(&zlib);
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
		[]() -> avi_video_codec* { return new avi_codec_cscd(clvl, kint);});
}
