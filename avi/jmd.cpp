#include "jmd.hpp"
#include <iostream>
#include <zlib.h>
#include <stdexcept>
#include <cstring>

namespace
{
	void write32(char* x, uint64_t v)
	{
		x[0] = (v >> 24);
		x[1] = (v >> 16);
		x[2] = (v >> 8);
		x[3] = v;
	}
}

void jmd_dumper::video(uint64_t ts, uint32_t* memory, uint32_t width, uint32_t height)
{
	frame_buffer f;
	f.ts = ts;
	size_t fsize = 0;
	//We'll compress the frame here.
	f.data = compress_frame(memory, width, height);
	frames.push_back(f);
	flush_buffers(false);
}

void jmd_dumper::audio(uint64_t ts, short l, short r)
{
	sample_buffer s;
	s.ts = ts;
	s.l = l;
	s.r = r;
	samples.push_back(s);
	flush_buffers(false);
}

jmd_dumper::jmd_dumper(const std::string& filename, unsigned level)
{
	clevel = level;
	jmd.open(filename.c_str(), std::ios::out | std::ios::binary);
	if(!jmd)
		throw std::runtime_error("Can't open output JMD file.");
	last_written_ts = 0;
	//Write the segment tables.
	//Stream #0 is video.
	//Stream #1 is PCM audio.
	//Stream #2 is Gameinfo.
	//Stream #3 is Dummy.
	char header[] = {
		/* Magic */
		-1, -1, 0x4A, 0x50, 0x43, 0x52, 0x52, 0x4D, 0x55, 0x4C, 0x54, 0x49, 0x44, 0x55, 0x4D, 0x50,
		/* Channel count. */
		0x00, 0x04,
		/* Video channel header. */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 'v', 'i',
		/* Audio channel header. */
		0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 'a', 'u',
		/* Gameinfo channel header. */
		0x00, 0x02, 0x00, 0x05, 0x00, 0x02, 'g', 'i',
		/* Dummy channel header. */
		0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 'd', 'u'
	};
	jmd.write(header, sizeof(header));
	if(!jmd)
		throw std::runtime_error("Can't write JMD header and segment table");
}

jmd_dumper::~jmd_dumper()
{
	try {
		end(last_written_ts);
	} catch(...) {
	}
}

void jmd_dumper::end(uint64_t ts)
{
	flush_buffers(true);
	if(last_written_ts > ts) {
		jmd.close();
		return;
	}
	char dummypacket[8] = {0x00, 0x03};
	write32(dummypacket + 2, ts - last_written_ts);
	last_written_ts = ts;
	jmd.write(dummypacket, sizeof(dummypacket));
	if(!jmd)
		throw std::runtime_error("Can't write JMD ending dummy packet");
	jmd.close();
}

void jmd_dumper::gameinfo(const std::string& gamename, const std::string& authors, uint64_t gametime,
	uint64_t rerecords)
{
	//FIXME: Implement this.
}

void jmd_dumper::flush_buffers(bool force)
{
	while(!frames.empty() || !samples.empty()) {
		if(frames.empty() || samples.empty()) {
			if(!force)
				return;
			else if(!frames.empty()) {
				frame_buffer& f = frames.front();
				flush_frame(f);
				frames.pop_front();
			} else if(!samples.empty()) {
				sample_buffer& s = samples.front();
				flush_sample(s);
				samples.pop_front();
			}
			continue;
		}
		frame_buffer& f = frames.front();
		sample_buffer& s = samples.front();
		if(f.ts <= s.ts) {
			flush_frame(f);
			frames.pop_front();
		} else {
			flush_sample(s);
			samples.pop_front();
		}
	}
}

void jmd_dumper::flush_frame(frame_buffer& f)
{
	char videopacketh[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
	write32(videopacketh + 2, f.ts - last_written_ts);
	last_written_ts = f.ts;
	unsigned lneed = 0;
	if(f.data.size() >= (1ULL << 63))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 63) & 0x7F);
	if(f.data.size() >= (1ULL << 56))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 56) & 0x7F);
	if(f.data.size() >= (1ULL << 49))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 49) & 0x7F);
	if(f.data.size() >= (1ULL << 42))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 42) & 0x7F);
	if(f.data.size() >= (1ULL << 35))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 35) & 0x7F);
	if(f.data.size() >= (1ULL << 28))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 28) & 0x7F);
	if(f.data.size() >= (1ULL << 21))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 21) & 0x7F);
	if(f.data.size() >= (1ULL << 14))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 14) & 0x7F);
	if(f.data.size() >= (1ULL << 7))
		videopacketh[7 + lneed++] = 0x80 | ((f.data.size() >> 7) & 0x7F);
	videopacketh[7 + lneed++] = (f.data.size() & 0x7F);

	jmd.write(videopacketh, 7 + lneed);
	if(!jmd)
		throw std::runtime_error("Can't write JMD video packet header");
	if(f.data.size() > 0)
		jmd.write(&f.data[0], f.data.size());
	if(!jmd)
		throw std::runtime_error("Can't write JMD video packet body");
}

void jmd_dumper::flush_sample(sample_buffer& s)
{
	char soundpacket[12] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04};
	write32(soundpacket + 2, s.ts - last_written_ts);
	last_written_ts = s.ts;
	soundpacket[8] = (s.l >> 8) & 0xFF;
	soundpacket[9] = s.l & 0xFF;
	soundpacket[10] = (s.r >> 8) & 0xFF;
	soundpacket[11] = s.r & 0xFF;
	jmd.write(soundpacket, sizeof(soundpacket));
	if(!jmd)
		throw std::runtime_error("Can't write JMD sound packet");
}

#define INBUF_PIXELS 4096
#define OUTBUF_ADVANCE 4096

std::vector<char> jmd_dumper::compress_frame(uint32_t* memory, uint32_t width, uint32_t height)
{
	std::vector<char> ret;
	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	if(deflateInit(&stream, clevel) != Z_OK)
		throw std::runtime_error("Can't initialize zlib stream");

	size_t usize = 4;
	ret.resize(4);
	ret[0] = (width >> 8);
	ret[1] = width;
	ret[2] = (height >> 8);
	ret[3] = height;
	uint8_t input_buffer[4 * INBUF_PIXELS];
	size_t ptr = 0;
	size_t pixels = static_cast<size_t>(width) * height;
	bool input_clear = true;
	bool flushed = false;
	size_t bsize = 0;
	uint32_t _magic = 0x18100800;
	const uint8_t* magic = reinterpret_cast<const uint8_t*>(&magic);
	while(1) {
		if(input_clear) {
			size_t pixel = ptr;
			for(unsigned i = 0; i < INBUF_PIXELS && pixel < pixels; i++, pixel++) {
				input_buffer[4 * i + 0] = memory[pixel];
				input_buffer[4 * i + 1] = memory[pixel] >> 8;
				input_buffer[4 * i + 2] = memory[pixel] >> 16;
				input_buffer[4 * i + 3] = 0;
			}
			bsize = pixel - ptr;
			ptr = pixel;
			input_clear = false;
			//Now the input data to compress is in input_buffer, bsize elements.
			stream.next_in = reinterpret_cast<uint8_t*>(input_buffer);
			stream.avail_in = 4 * bsize;
		}
		if(!stream.avail_out) {
			if(flushed)
				usize += (OUTBUF_ADVANCE - stream.avail_out);
			flushed = true;
			ret.resize(usize + OUTBUF_ADVANCE);
			stream.next_out = reinterpret_cast<uint8_t*>(&ret[usize]);
			stream.avail_out = OUTBUF_ADVANCE;
		}
		int r = deflate(&stream, (ptr == pixels) ? Z_FINISH : 0);
		if(r == Z_STREAM_END)
			break;
		if(r != Z_OK)
			throw std::runtime_error("Can't deflate data");
		if(!stream.avail_in)
			input_clear = true;
	}
	usize += (OUTBUF_ADVANCE - stream.avail_out);
	deflateEnd(&stream);

	ret.resize(usize);
	return ret;
}
