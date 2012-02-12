#ifndef _avi__avi_structure__hpp__included__
#define _avi__avi_structure__hpp__included__

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <list>


struct stream_format_base
{
	virtual ~stream_format_base();
/**
 * Type of track.
 * - 0x73646976 for video tracks.
 * - 0x73647561 for audio tracks.
 */
	virtual uint32_t type() = 0;
/**
 * Denomerator of samping/frame rate.
 */
	virtual uint32_t scale() = 0;
/**
 * Numerator of samping/frame rate.
 */
	virtual uint32_t rate() = 0;
	virtual uint32_t sample_size() = 0;
	virtual uint32_t rect_left() = 0;
	virtual uint32_t rect_top() = 0;
	virtual uint32_t rect_right() = 0;
	virtual uint32_t rect_bottom() = 0;
	virtual size_t size() = 0;
	virtual void serialize(std::ostream& out) = 0;
};

struct stream_format_video : public stream_format_base
{
	~stream_format_video();
	uint32_t type();
	uint32_t scale();
	uint32_t rate();
	uint32_t rect_left();
	uint32_t rect_top();
	uint32_t rect_right();
	uint32_t rect_bottom();
	uint32_t sample_size();
	size_t size();
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bit_count;
	uint32_t compression;
	uint32_t size_image;
	uint32_t resolution_x;
	uint32_t resolution_y;
	uint32_t clr_used;
	uint32_t clr_important;
	uint32_t fps_n;
	uint32_t fps_d;
	std::vector<char> extra;
	void serialize(std::ostream& out);
};

struct stream_format_audio : public stream_format_base
{
	~stream_format_audio();
	uint32_t type();
	uint32_t scale();
	uint32_t rate();
	uint32_t rect_left();
	uint32_t rect_top();
	uint32_t rect_right();
	uint32_t rect_bottom();
	uint32_t sample_size();
	size_t size();
	uint16_t format_tag;
	uint16_t channels;
	uint32_t samples_per_second;
	uint32_t average_bytes_per_second;
	uint16_t block_align;
	uint16_t bits_per_sample;
	uint32_t blocksize;
	std::vector<char> extra;
	void serialize(std::ostream& out);
};

struct stream_header
{
	size_t size();
	uint32_t handler;
	uint32_t flags;
	uint16_t priority;
	uint16_t language;
	uint32_t initial_frames;
	uint32_t start;
	uint32_t length;
	uint32_t suggested_buffer_size;
	uint32_t quality;
	stream_header();
	void add_frames(size_t count);
	void serialize(std::ostream& out, struct stream_format_base& format);
	void reset();
};

template<class format>
struct stream_header_list
{
	size_t size();
	stream_header strh;
	format strf;
	void serialize(std::ostream& out);
	void reset();
};

struct avi_header
{
	size_t size();
	uint32_t microsec_per_frame;
	uint32_t max_bytes_per_sec;
	uint32_t padding_granularity;
	uint32_t flags;
	uint32_t initial_frames;
	uint32_t suggested_buffer_size;
	void serialize(std::ostream& out, stream_header_list<stream_format_video>& videotrack, uint32_t tracks);
};

struct header_list
{
	size_t size();
	avi_header avih;
	stream_header_list<stream_format_video> videotrack;
	stream_header_list<stream_format_audio> audiotrack;
	void serialize(std::ostream& out);
	void reset();
};

struct movi_chunk
{
	uint32_t payload_size;
	size_t write_offset();
	size_t size();
	movi_chunk();
	void add_payload(size_t s);
	void serialize(std::ostream& out);
	void reset();
};

struct index_entry
{
	size_t size();
	uint32_t chunk_type;
	uint32_t flags;
	uint32_t offset;
	uint32_t length;
	index_entry(uint32_t _chunk_type, uint32_t _flags, uint32_t _offset, uint32_t _length);
	void serialize(std::ostream& out);
};

struct idx1_chunk
{
	void add_entry(const index_entry& entry);
	std::list<index_entry> entries;
	size_t size();
	void serialize(std::ostream& out);
	void reset();
};

struct avi_file_structure
{
	size_t write_offset();
	size_t size();
	header_list hdrl;
	movi_chunk movi;
	idx1_chunk idx1;
	std::ostream* outstream;
	void serialize();
	void start_data(std::ostream& out);
	void finish_avi();
};

#endif
