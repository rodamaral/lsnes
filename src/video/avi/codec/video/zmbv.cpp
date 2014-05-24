#include "video/avi/codec.hpp"
#include "core/instance.hpp"
#include "core/settings.hpp"
#include "library/zlibstream.hpp"
#include <zlib.h>
#include <limits>
#include <cstring>
#include <cerrno>
#include <stdexcept>

//The largest possible vector.
#define MAXIMUM_VECTOR 64

namespace
{
	settingvar::supervariable<settingvar::model_int<0,9>> clvl(lsnes_setgrp, "avi-zmbv-compression",
		"AVI‣ZMBV‣Compression", 7);
	settingvar::supervariable<settingvar::model_int<0,999999999>> kint(lsnes_setgrp, "avi-zmbv-keyint",
		"AVI‣ZMBV‣Keyframe interval", 299);
	settingvar::supervariable<settingvar::model_int<8,64>> bwv(lsnes_setgrp, "avi-zmbv-blockw",
		"AVI‣ZMBV‣Block width", 16);
	settingvar::supervariable<settingvar::model_int<8,64>> bhv(lsnes_setgrp, "avi-zmbv-blockh",
		"AVI‣ZMBV‣Block height", 16);
	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> fsrch(lsnes_setgrp,
		"avi-zmbv-fullsearch", "AVI‣ZMBV‣Full search (slow)", false);

	//Motion vector.
	struct motion
	{
		//X motion (positive is to left), -64...63.
		int dx;
		//Y motion (positive it to up), -64...63.
		int dy;
		//How bad the vector is. 0 means the vector is perfect (no residual).
		uint32_t p;
	};

	//The main ZMBV decoder state.
	struct avi_codec_zmbv : public avi_video_codec
	{
		avi_codec_zmbv(uint32_t _level, uint32_t maxpframes, uint32_t _bw, uint32_t _bh, bool _fullsearch);
		~avi_codec_zmbv();
		avi_video_codec::format reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d);
		void frame(uint32_t* data, uint32_t stride);
		bool ready();
		avi_packet getpacket();
	private:
		//The current pending packet, if any.
		avi_packet out;
		//False if there is a pending packet, true if ready to take a frame.
		bool ready_flag;
		//The size of supplied frames.
		unsigned iwidth;
		unsigned iheight;
		//The size of written frames.
		unsigned ewidth;
		unsigned eheight;
		//P-frames written since last I-frame.
		unsigned pframes;
		//Maximum number of P-frames to write in sequence.
		unsigned max_pframes;
		//Size of one block.
		uint32_t bw;
		uint32_t bh;
		//Full search flag.
		bool fullsearch;
		//Motion vector buffer, one motion vector for each block, in left-to-right, top-to-bottom order.
		std::vector<motion> mv;
		//Pixel buffer (2 full frames and one block).
		std::vector<uint32_t> pixbuf;
		//Current frame pointer.
		uint32_t* current_frame;
		//Previous frame pointer.
		uint32_t* prev_frame;
		//Scratch block pointer.
		uint32_t* scratch;
		//Output buffer. Sufficient space to hold uncompressed data.
		std::vector<char> outbuffer;
		//Output scratch memory.
		char* oscratch;
		//Zlib streaam.
		zlibstream z;
		//Compute penalty for motion vector (dx, dy) on block with upper-left corner at (bx, by).
		uint32_t mv_penalty(uint32_t bx, uint32_t by, int dx, int dy);
		//Do motion detection for block with upper-left corner at (bx, by). M is filled with the resulting
		//motion vector and t is initial guess for the motion vector.
		void mv_detect(uint32_t bx, uint32_t by, motion& m, motion t);
		//Serialize movement vectors and furrent frame data to output buffer. If keyframe is true, keyframe is
		//written, otherwise non-keyframe.
		void serialize_frame(bool keyframe);
	};

	//Compute XOR of blocks.
	void xor_blocks(uint32_t* target, uint32_t* src1, uint32_t src1x, uint32_t src1y,
		uint32_t src1w, uint32_t src1h, uint32_t* src2, uint32_t src2x, uint32_t src2y,
		uint32_t src2w, uint32_t src2h, uint32_t bw, uint32_t bh)
	{
		uint32_t* s1ptr = src1 + src1y * src1w + src1x;
		uint32_t* s2ptr = src2 + src2y * src2w + src2x;
		for(uint32_t y = 0; y < bh; y++) {
			for(uint32_t x = 0; x < bw; x++)
				target[x] = s1ptr[x] ^ s2ptr[x];
			target += bw;
			s1ptr += src1w;
			s2ptr += src2w;
		}
	}

	//Estimate entropy.
	uint32_t entropy(uint32_t* data, uint32_t bw, uint32_t bh)
	{
		//Because XORs are essentially random, calculate the number of non-zeroes to ascertain badness.
		uint8_t* _data = reinterpret_cast<uint8_t*>(data);
		uint32_t e = 0;
		size_t imax = 4 * bw * bh;
		for(size_t i = 0; i < imax; i++)
			if(_data[i])
				e++;
		return e;
	}

	uint32_t avi_codec_zmbv::mv_penalty(uint32_t bx, uint32_t by, int dx, int dy)
	{
		//Penalty is entropy estimate of resulting block.
		xor_blocks(scratch, current_frame, bx, by, ewidth + 2 * MAXIMUM_VECTOR, eheight, prev_frame, bx + dx,
			by + dy, ewidth + 2 * MAXIMUM_VECTOR, eheight, bw, bh);
		return entropy(scratch, bw, bh);
	}

	void avi_codec_zmbv::serialize_frame(bool keyframe)
	{
		unsigned char tmp[7];
		uint32_t nhb, nvb, nb;
		//In_stride/in_offset is in units of words, out_stride is in units of bytes.
		size_t in_stride = (ewidth + 2 * MAXIMUM_VECTOR);
		size_t in_offset = MAXIMUM_VECTOR * (in_stride + 1);
		size_t osize = 0;
		if(keyframe) {
			//Just copy the frame data and compress that.
			for(size_t y = 0; y < eheight; y++)
				memcpy(oscratch + 4 * ewidth * y, current_frame + in_stride * y + in_offset,
					4 * ewidth);
			osize = 4 * ewidth * eheight;
			goto compress;
		}
		//Number of blocks.
		nhb = (ewidth + bw - 1) / bw;
		nvb = (eheight + bh - 1) / bh;
		nb = nhb * nvb;
		osize = 0;
		//Serialize the motion vectors.
		for(size_t i = 0; i < nb; i++) {
			oscratch[osize++] = (mv[i].dx << 1) | (mv[i].p ? 1 : 0);
			oscratch[osize++] = (mv[i].dy << 1);
		}
		//Pad to multiple of 4 bytes.
		while(osize % 4)
			oscratch[osize++] = 0;
		//Serialize the residuals.
		for(size_t i = 0; i < nb; i++) {
			if(mv[i].p == 0)
				continue;
			uint32_t bx = (i % nhb) * bw + MAXIMUM_VECTOR;
			uint32_t by = (i / nhb) * bh + MAXIMUM_VECTOR;
			xor_blocks(reinterpret_cast<uint32_t*>(oscratch + osize), current_frame, bx, by, ewidth + 2 *
				MAXIMUM_VECTOR, eheight, prev_frame, bx + mv[i].dx, by + mv[i].dy, ewidth + 2 *
				MAXIMUM_VECTOR, eheight, bw, bh);
			osize += 4 * bw * bh;
		}
compress:
		//Compress the output data.
		if(keyframe)
		{
			tmp[0] = 1;	//Keyframe
			tmp[1] = 0;	//Major version.
			tmp[2] = 1;	//Minor version.
			tmp[3] = 1;	//Zlib compresison.
			tmp[4] = 8;	//32-bit
			tmp[5] = bw;	//Block size.
			tmp[6] = bh;	//Block size.
			z.reset(tmp, 7);
		} else {
			tmp[0] = 0;	//Not keyframe.
			z.adddata(tmp, 1);
		}
		z.write(reinterpret_cast<uint8_t*>(oscratch), osize);
	}

	//If candidate is better than best, update best. Returns true if ideal has been reached, else false.
	bool update_best(motion& best, motion& candidate)
	{
		if(candidate.p < best.p)
			best = candidate;
		return (best.p == 0);
	}

	void avi_codec_zmbv::mv_detect(uint32_t bx, uint32_t by, motion& m, motion t)
	{
		//Try the suggested vector.
		motion c;
		m.p = mv_penalty(bx, by, m.dx = t.dx, m.dy = t.dy);
		if(!m.p)
			return;
		//Try the zero vector.
		c.p = mv_penalty(bx, by, c.dx = 0, c.dy = 0);
		if(update_best(m, c))
			return;
		//Try cardinal vectors up to 9 units.
		for(int s = 1; s < 10; s++) {
			if(s == 0)
				continue;
			c.p = mv_penalty(bx, by, c.dx = -s, c.dy = 0);
			if(update_best(m, c))
				return;
			c.p = mv_penalty(bx, by, c.dx = 0, c.dy = -s);
			if(update_best(m, c))
				return;
			c.p = mv_penalty(bx, by, c.dx = s, c.dy = 0);
			if(update_best(m, c))
				return;
			c.p = mv_penalty(bx, by, c.dx = 0, c.dy = s);
			if(update_best(m, c))
				return;
		}
		//Try all in [-16,16]x[-16,16].
		if(fullsearch)
			for(int dy = -16; dy <= 16; dy++) {
				for(int dx = -16; dx <= 16; dx++) {
					c.p = mv_penalty(bx, by, c.dx = dx, c.dy = dy);
					if(update_best(m, c))
						return;
				}
			}
	}

	avi_codec_zmbv::~avi_codec_zmbv()
	{
	}

	unsigned getzlevel(uint32_t _level)
	{
		if(_level < 0 || _level > 9)
			throw std::runtime_error("Invalid compression level");
		return _level;
	}

	avi_codec_zmbv::avi_codec_zmbv(uint32_t _level, uint32_t maxpframes, uint32_t _bw, uint32_t _bh,
		bool _fullsearch)
		: z(getzlevel(_level))
	{
		bh = _bh;
		bw = _bw;
		max_pframes = maxpframes;
		fullsearch = _fullsearch;
	}

	avi_video_codec::format avi_codec_zmbv::reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d)
	{
		pframes = std::numeric_limits<unsigned>::max();	//Next frame has to be keyframe.
		iwidth = width;
		iheight = height;
		ewidth = (iwidth + bw - 1) / bw * bw;
		eheight = (iheight + bh - 1) / bh * bh;
		ready_flag = true;
		avi_video_codec::format fmt(ewidth, eheight, 0x56424D5A, 24);

		pixbuf.resize(2 * (ewidth + 2 * MAXIMUM_VECTOR) * (eheight + 2 * MAXIMUM_VECTOR) + bw * bh);
		current_frame = &pixbuf[0];
		prev_frame = &pixbuf[(ewidth + 2 * MAXIMUM_VECTOR) * (eheight + 2 * MAXIMUM_VECTOR)];
		scratch = &pixbuf[2 * (ewidth + 2 * MAXIMUM_VECTOR) * (eheight + 2 * MAXIMUM_VECTOR)];
		mv.resize(((ewidth + bw - 1) / bw) * ((eheight + bh - 1) / bh));
		outbuffer.resize(4 * ((mv.size() + 1) / 2) + 4 * ewidth * eheight);
		oscratch = &outbuffer[0];
		memset(&pixbuf[0], 0, 4 * pixbuf.size());
		return fmt;
	}

	void avi_codec_zmbv::frame(uint32_t* data, uint32_t stride)
	{
		//Keyframe/not determination.
		bool keyframe = false;
		if(pframes >= max_pframes) {
			keyframe = true;
			pframes = 0;
		} else
			pframes++;

		//If bigendian, swap.
		short magic = 258;
		size_t frameoffset = MAXIMUM_VECTOR * (ewidth + 2 * MAXIMUM_VECTOR + 1);
		size_t framestride = ewidth + 2 * MAXIMUM_VECTOR;
		if(reinterpret_cast<uint8_t*>(&magic)[0] == 1)
			for(size_t y = 0; y < iheight; y++) {
				uint8_t* _current = reinterpret_cast<uint8_t*>(current_frame + frameoffset +
					framestride * y);
				uint8_t* _data = reinterpret_cast<uint8_t*>(&data[stride * y]);
				for(size_t i = 0; i < iwidth; i++) {
					_current[4 * i + 0] = _data[4 * i + 3];
					_current[4 * i + 1] = _data[4 * i + 2];
					_current[4 * i + 2] = _data[4 * i + 1];
					_current[4 * i + 3] = _data[4 * i + 0];
				}
			}
		else
			for(size_t y = 0; y < iheight; y++) {
				uint8_t* _current = reinterpret_cast<uint8_t*>(current_frame + frameoffset +
					framestride * y);
				uint8_t* _data = reinterpret_cast<uint8_t*>(&data[stride * y]);
				for(size_t i = 0; i < iwidth; i++) {
					_current[4 * i + 2] = _data[4 * i + 0];
					_current[4 * i + 1] = _data[4 * i + 1];
					_current[4 * i + 0] = _data[4 * i + 2];
					_current[4 * i + 3] = _data[4 * i + 3];
				}
			}

		//Estimate motion vectors for all blocks if non-keyframe.
		uint32_t nhb = (ewidth + bw - 1) / bw;
		if(!keyframe) {
			motion t;
			t.dx = 0;
			t.dy = 0;
			t.p = 0;
			for(size_t i = 0; i < mv.size(); i++) {
				mv_detect((i % nhb) * bw + MAXIMUM_VECTOR, (i / nhb) * bh + MAXIMUM_VECTOR, mv[i], t);
				t = mv[i];
			}
		}

		//Serialize and output.
		serialize_frame(keyframe);
		std::swap(current_frame, prev_frame);
		z.readsync(out.payload);
		out.typecode = 0x6264;		//Not exactly correct according to specs...
		out.hidden = false;
		out.indexflags = keyframe ? 0x10 : 0;
		ready_flag = false;
	}

	bool avi_codec_zmbv::ready()
	{
		return ready_flag;
	}

	avi_packet avi_codec_zmbv::getpacket()
	{
		ready_flag = true;
		return out;
	}

	//ZMBV encoder factory object.
	avi_video_codec_type rgb("zmbv", "Zip Motion Blocks Video codec",
		[]() -> avi_video_codec* {
			return new avi_codec_zmbv(clvl(*CORE().settings), kint(*CORE().settings),
				bwv(*CORE().settings), bhv(*CORE().settings), fsrch(*CORE().settings));
		});
}
