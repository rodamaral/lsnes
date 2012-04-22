#include "video/avi/codec.hpp"
#include "core/settings.hpp"
#include <zlib.h>
#include <limits>
#include <cstring>
#include <cerrno>
#include <stdexcept>

namespace
{
	numeric_setting clvl("avi-zmbv-compression", 0, 9, 7);
	numeric_setting kint("avi-zmbv-keyint", 0, 999999999, 299);
	numeric_setting bwv("avi-zmbv-blockw", 8, 64, 16);
	numeric_setting bhv("avi-zmbv-blockh", 8, 64, 16);

	struct motion
	{
		int dx;
		int dy;
		uint32_t p;
	};

	struct avi_codec_zmbv : public avi_video_codec
	{
		avi_codec_zmbv(uint32_t _level, uint32_t maxpframes, uint32_t _bw, uint32_t _bh);
		~avi_codec_zmbv();
		avi_video_codec::format reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d);
		void frame(uint32_t* data);
		bool ready();
		avi_packet getpacket();
	private:
		avi_packet out;
		bool ready_flag;
		unsigned iwidth;
		unsigned iheight;
		unsigned ewidth;
		unsigned eheight;
		unsigned pframes;
		unsigned max_pframes;
		unsigned level;

		//Size of block.
		uint32_t bw;
		uint32_t bh;
		//Entropy estimator table.
		std::vector<uint32_t> entropy_tab;
		//Temporary scratch memory (one block).
		std::vector<uint32_t> tmp;
		//Motion vector buffer.
		std::vector<motion> mv;
		//Previous&Current frame.
		std::vector<uint32_t> current;
		std::vector<uint32_t> prev;
		//Compression packet buffer and size.
		std::vector<char> diff;
		size_t diffsize;
		z_stream zstream;
		//Output packet buffer and size.
		std::vector<char> output;
		size_t output_size;

		//Motion vector penalty.
		uint32_t mv_penalty(uint32_t* data, int32_t bx, int32_t by, int dx, int dy);
		//Do motion detection.
		void mv_detect(uint32_t* data, int32_t bx, int32_t by, motion& m, motion t);
		//Serialize to difference buffer.
		void serialize_frame(bool keyframe, uint32_t* data);
		//Take compression packet buffer and write output packet buffer.
		void compress_packet(bool keyframe);
	};

	void rbound(int32_t x, int32_t w, uint32_t b, int32_t& start, int32_t& offset, int32_t& size)
	{
		start = x;
		offset = 0;
		size = b;
		if(start < 0) {
			offset = -start;
			start = 0;
			size = b - offset;
		}
		if(start + size > w)
			size = w - start;
		if(size < 0)
			size = 0;
		start = x + offset;
	}

	void xor_blocks(uint32_t* target, uint32_t* src1, int32_t src1x, int32_t src1y,
		int32_t src1w, int32_t src1h, uint32_t* src2, int32_t src2x, int32_t src2y,
		int32_t src2w, int32_t src2h, uint32_t bw, uint32_t bh)
	{
		int32_t h_s1start;
		int32_t h_s1off;
		int32_t h_s1size;
		int32_t h_s2start;
		int32_t h_s2off;
		int32_t h_s2size;
		int32_t v_s1start;
		int32_t v_s1off;
		int32_t v_s1size;
		int32_t v_s2start;
		int32_t v_s2off;
		int32_t v_s2size;

		rbound(src1x, src1w, bw, h_s1start, h_s1off, h_s1size);
		rbound(src2x, src2w, bw, h_s2start, h_s2off, h_s2size);
		rbound(src1y, src1h, bh, v_s1start, v_s1off, v_s1size);
		rbound(src2y, src2h, bh, v_s2start, v_s2off, v_s2size);

		if(h_s1size < bw || v_s1size < bh)
			memset(target, 0, 4 * bw * bh);
		uint32_t* t1ptr = target + v_s1off * bh + h_s1off;
		uint32_t* t2ptr = target + v_s2off * bh + h_s2off;
		uint32_t* s1ptr = src1 + v_s1start * src1w + h_s1start;
		uint32_t* s2ptr = src2 + v_s2start * src2w + h_s2start;
		for(int32_t y = 0; y < v_s1size; y++)
			memcpy(t1ptr + bw * y, s1ptr + src1w * y, 4 * h_s1size);
		for(int32_t y = 0; y < v_s2size; y++)
			for(int32_t x = 0; x < h_s2size; x++)
				t2ptr[y * bw + x] ^= s2ptr[y * src2w + x];
	}

	void entropy_init(std::vector<uint32_t>& mem, uint32_t bw, uint32_t bh)
	{
		size_t bytes = 4 * bw * bh;
		mem.resize(bytes + 1);
		mem[0] = 0;
		mem[bytes] = 0;
		double M0 = log(bytes);
		double M1 = 700000000.0 / bytes;
		for(size_t i = 1; i < bytes; i++)
			mem[i] = M1 * (M0 - log(i));
	}

	uint32_t entropy(std::vector<uint32_t>& mem, uint32_t* data)
	{
		uint8_t* _data = reinterpret_cast<uint8_t*>(data);
		uint32_t e = 0;
		size_t imax = mem.size() - 1;
		for(size_t i = 0; i < imax; i++)
			if(_data[i])
				e++;
		return e;
	}

	uint32_t avi_codec_zmbv::mv_penalty(uint32_t* data, int32_t bx, int32_t by, int dx, int dy)
	{
		xor_blocks(&tmp[0], data, bx, by, ewidth, eheight, &prev[0], bx + dx, by + dy, ewidth, eheight, bw,
			bh);
		return entropy(entropy_tab, &tmp[0]);
	}

	void avi_codec_zmbv::serialize_frame(bool keyframe, uint32_t* data)
	{
		if(keyframe) {
			memcpy(&diff[0], data, 4 * ewidth * eheight);
			diffsize = 4 * ewidth * eheight;
			return;
		}
		uint32_t nhb = (ewidth + bw - 1) / bw;
		uint32_t nvb = (eheight + bh - 1) / bh;
		uint32_t nb = nhb * nvb;
		size_t osize = 0;
		for(size_t i = 0; i < nb; i++) {
			diff[osize++] = (mv[i].dx << 1) | (mv[i].p ? 1 : 0);
			diff[osize++] = (mv[i].dy << 1);
		}
		while(osize % 4)
			diff[osize++] = 0;
		for(size_t i = 0; i < nb; i++) {
			if(mv[i].p == 0)
				continue;
			int32_t bx = (i % nhb) * bw;
			int32_t by = (i / nhb) * bh;
			xor_blocks(reinterpret_cast<uint32_t*>(&diff[osize]), data, bx, by, ewidth, eheight, &prev[0],
				bx + mv[i].dx, by + mv[i].dy, ewidth, eheight, bw, bh);
			osize += 4 * bw * bh;
		}
		diffsize = osize;
	}

	void avi_codec_zmbv::compress_packet(bool keyframe)
	{
		size_t osize = 0;
		output[osize++] = keyframe ? 1 : 0;	//Indicate keyframe/not.
		if(keyframe) {
			output[osize++] = 0;		//Version 0.1
			output[osize++] = 1;
			output[osize++] = 1;		//Zlib compression.
			output[osize++] = 8;		//32 bit.
			output[osize++] = bw;		//Block size.
			output[osize++] = bh;
			deflateReset(&zstream);		//Reset the zlib context.
		}
		zstream.next_in = reinterpret_cast<uint8_t*>(&diff[0]);
		zstream.avail_in = diffsize;
		zstream.next_out = reinterpret_cast<uint8_t*>(&output[osize]);
		zstream.avail_out = output.size() - osize;
		if(deflate(&zstream, Z_SYNC_FLUSH) != Z_OK)
			throw std::runtime_error("Zlib error while compressing data");
		if(zstream.avail_in || !zstream.avail_out)
			throw std::runtime_error("Buffer overrun while compressing data");
		output_size = output.size() - zstream.avail_out;
	}

	bool update_best(motion& best, motion& candidate)
	{
		if(candidate.p < best.p)
			best = candidate;
		return (best.p == 0);
	}

	void avi_codec_zmbv::mv_detect(uint32_t* data, int32_t bx, int32_t by, motion& m, motion t)
	{
		motion c;
		m.p = mv_penalty(data, bx, by, m.dx = t.dx, m.dy = t.dy);
		if(!m.p)
			return;
		c.p = mv_penalty(data, bx, by, c.dx = 0, c.dy = 0);
		if(update_best(m, c))
			return;
		for(int s = 1; s < 10; s++) {
			if(s == 0)
				continue;
			c.p = mv_penalty(data, bx, by, c.dx = -s, c.dy = 0);
			if(update_best(m, c))
				return;
			c.p = mv_penalty(data, bx, by, c.dx = 0, c.dy = -s);
			if(update_best(m, c))
				return;
			c.p = mv_penalty(data, bx, by, c.dx = s, c.dy = 0);
			if(update_best(m, c))
				return;
			c.p = mv_penalty(data, bx, by, c.dx = 0, c.dy = s);
			if(update_best(m, c))
				return;
		}
	}

	avi_codec_zmbv::~avi_codec_zmbv()
	{
		deflateEnd(&zstream);
	}

	unsigned getzlevel(uint32_t _level)
	{
		if(_level < 0 || _level > 9)
			throw std::runtime_error("Invalid compression level");
		return _level;
	}

	avi_codec_zmbv::avi_codec_zmbv(uint32_t _level, uint32_t maxpframes, uint32_t _bw, uint32_t _bh)
	{
		bh = _bh;
		bw = _bw;
		level = _level;
		max_pframes = maxpframes;
		memset(&zstream, 0, sizeof(zstream));
		if(deflateInit(&zstream, getzlevel(_level)))
			throw std::runtime_error("Error initializing deflate");
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

		entropy_init(entropy_tab, bw, bh);
		prev.resize(4 * ewidth * eheight);
		current.resize(4 * ewidth * eheight);
		tmp.resize(4 * bw * bh);
		mv.resize(((ewidth + bw - 1) / bw) * ((eheight + bh - 1) / bh));
		diff.resize(4 * ((mv.size() + 1) / 2) + 4 * ewidth * eheight);
		output.resize(deflateBound(&zstream, diff.size()) + 128);
		return fmt;
	}

	void avi_codec_zmbv::frame(uint32_t* data)
	{
		bool buffer_loaded = false;
		bool keyframe = false;
		if(pframes >= max_pframes) {
			keyframe = true;
			pframes = 0;
		} else
			pframes++;

		//If bigendian, swap.
		short magic = 258;
		if(reinterpret_cast<uint8_t*>(&magic)[0] == 1)
			for(size_t i = 0; i < ewidth * eheight; i++) {
				uint8_t* _current = reinterpret_cast<uint8_t*>(&current[0]);
				uint8_t* _data = reinterpret_cast<uint8_t*>(&data[0]);
				_current[4 * i + 0] = _data[4 * i + 3];
				_current[4 * i + 1] = _data[4 * i + 2];
				_current[4 * i + 2] = _data[4 * i + 1];
				_current[4 * i + 3] = _data[4 * i + 0];
			}
		else
			for(size_t i = 0; i < ewidth * eheight; i++) {
				uint8_t* _current = reinterpret_cast<uint8_t*>(&current[0]);
				uint8_t* _data = reinterpret_cast<uint8_t*>(&data[0]);
				_current[4 * i + 2] = _data[4 * i + 0];
				_current[4 * i + 1] = _data[4 * i + 1];
				_current[4 * i + 0] = _data[4 * i + 2];
				_current[4 * i + 3] = _data[4 * i + 3];
			}

		uint32_t nhb = (ewidth + bw - 1) / bw;
		if(!keyframe) {
			motion t;
			t.dx = 0;
			t.dy = 0;
			t.p = 0;
			for(size_t i = 0; i < mv.size(); i++) {
				mv_detect(&current[0], (i % nhb) * bw, (i / nhb) * bh, mv[i], t);
				t = mv[i];
			}
		}

		serialize_frame(keyframe, &current[0]);
		compress_packet(keyframe);
		memcpy(&prev[0], &current[0], 4 * ewidth * eheight);
		out.payload.resize(output_size);
		memcpy(&out.payload[0], &output[0], output_size);
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


	avi_video_codec_type rgb("zmbv", "Zip Motion Blocks Video codec",
		[]() -> avi_video_codec* { return new avi_codec_zmbv(clvl, kint, bwv, bhv);});
}
