#include "cscd.hpp"
#include <zlib.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <list>



#define AVI_CUTOFF_SIZE 2100000000

namespace
{
	struct dumper_thread_obj
	{
		int operator()(avi_cscd_dumper* d)
		{
			try {
				return d->encode_thread();
			} catch(std::exception& e) {
				std::cerr << "Encode thread threw: " << e.what() << std::endl;
				d->set_capture_error(e.what());
			}
			return 1;
		}
	};

	void write8(char* out, unsigned char x)
	{
		out[0] = x;
	}

	void write16(char* out, unsigned x)
	{
		out[0] = x;
		out[1] = x >> 8;
	}

	void write32(char* out, unsigned long x)
	{
		out[0] = x;
		out[1] = x >> 8;
		out[2] = x >> 16;
		out[3] = x >> 24;
	}

	struct stream_format_base
	{
		virtual ~stream_format_base();
		virtual unsigned long type() = 0;
		virtual unsigned long scale() = 0;
		virtual unsigned long rate() = 0;
		virtual unsigned long sample_size() = 0;
		virtual unsigned long rect_left() = 0;
		virtual unsigned long rect_top() = 0;
		virtual unsigned long rect_right() = 0;
		virtual unsigned long rect_bottom() = 0;
		virtual size_t size() = 0;
		virtual void serialize(std::ostream& out) = 0;
	};

	struct stream_format_video : public stream_format_base
	{
		~stream_format_video();
		unsigned long type() { return 0x73646976UL; }
		unsigned long scale() { return fps_d; }
		unsigned long rate() { return fps_n; }
		unsigned long rect_left() { return 0; }
		unsigned long rect_top() { return 0; }
		unsigned long rect_right() { return width; }
		unsigned long rect_bottom() { return height; }
		unsigned long sample_size() { return (bit_count + 7) / 8; }
		size_t size() { return 48; }
		unsigned long width;
		unsigned long height;
		unsigned planes;
		unsigned bit_count;
		unsigned long compression;
		unsigned long size_image;
		unsigned long resolution_x;
		unsigned long resolution_y;
		unsigned long clr_used;
		unsigned long clr_important;
		unsigned long fps_n;
		unsigned long fps_d;
		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(size());
			write32(&buf[0], 0x66727473UL);	//Type
			write32(&buf[4], size() - 8);	//Size.
			write32(&buf[8], 40);		//BITMAPINFOHEADER size.
			write32(&buf[12], width);
			write32(&buf[16], height);
			write16(&buf[20], planes);
			write16(&buf[22], bit_count);
			write32(&buf[24], compression);
			write32(&buf[28], size_image);
			write32(&buf[32], resolution_x);
			write32(&buf[36], resolution_y);
			write32(&buf[40], clr_used);
			write32(&buf[40], clr_important);
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write strf (video)");
		}

	};

	struct stream_format_audio : public stream_format_base
	{
		~stream_format_audio();
		unsigned long type() { return 0x73647561; }
		unsigned long scale() { return 1; }
		unsigned long rate() { return samples_per_second; }
		unsigned long rect_left() { return 0; }
		unsigned long rect_top() { return 0; }
		unsigned long rect_right() { return 0; }
		unsigned long rect_bottom() { return 0; }
		unsigned long sample_size() { return blocksize; }
		size_t size() { return 28; }
		unsigned format_tag;
		unsigned channels;
		unsigned long samples_per_second;
		unsigned long average_bytes_per_second;
		unsigned block_align;
		unsigned bits_per_sample;
		unsigned long blocksize;
		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(size());
			write32(&buf[0], 0x66727473UL);	//Type
			write32(&buf[4], size() - 8);	//Size.
			write16(&buf[8], format_tag);
			write16(&buf[10], channels);
			write32(&buf[12], samples_per_second);
			write32(&buf[16], average_bytes_per_second);
			write16(&buf[20], block_align);
			write16(&buf[22], bits_per_sample);
			write16(&buf[24], 0);		//No extension data.
			write16(&buf[26], 0);		//Pad
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write strf (audio)");
		}
	};

	stream_format_base::~stream_format_base() {}
	stream_format_video::~stream_format_video() {}
	stream_format_audio::~stream_format_audio() {}

	struct stream_header
	{
		size_t size() { return 72; }
		unsigned long handler;
		unsigned long flags;
		unsigned priority;
		unsigned language;
		unsigned long initial_frames;
		unsigned long start;
		unsigned long length;
		unsigned long suggested_buffer_size;
		unsigned long quality;
		stream_header()
		{
			length = 0;
		}

		void add_frames(size_t count)
		{
			length = length + count;
		}

		void serialize(std::ostream& out, struct stream_format_base& format)
		{
			std::vector<char> buf;
			buf.resize(size());
			write32(&buf[0], 0x68727473UL);	//Type
			write32(&buf[4], size() - 8);	//Size.
			write32(&buf[8], format.type());
			write32(&buf[12], handler);
			write32(&buf[16], flags);
			write16(&buf[20], priority);
			write16(&buf[22], language);
			write32(&buf[24], initial_frames);
			write32(&buf[28], format.scale());
			write32(&buf[32], format.rate());
			write32(&buf[36], start);
			write32(&buf[40], length);
			write32(&buf[44], suggested_buffer_size);
			write32(&buf[48], quality);
			write32(&buf[52], format.sample_size());
			write32(&buf[56], format.rect_left());
			write32(&buf[60], format.rect_top());
			write32(&buf[64], format.rect_right());
			write32(&buf[68], format.rect_bottom());
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write strh");
		}
	};

	template<class format>
	struct stream_header_list
	{
		size_t size() { return 12 + strh.size() + strf.size(); }
		stream_header strh;
		format strf;
		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(12);
			write32(&buf[0], 0x5453494CUL);		//List.
			write32(&buf[4], size() - 8);
			write32(&buf[8], 0x6c727473UL);		//Type.
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write strl");
			strh.serialize(out, strf);
			strf.serialize(out);
		}
	};

	struct avi_header
	{
		size_t size() { return 64; }
		unsigned long microsec_per_frame;
		unsigned long max_bytes_per_sec;
		unsigned long padding_granularity;
		unsigned long flags;
		unsigned long initial_frames;
		unsigned long suggested_buffer_size;
		void serialize(std::ostream& out, stream_header_list<stream_format_video>& videotrack,
			unsigned long tracks)
		{
			std::vector<char> buf;
			buf.resize(size());
			write32(&buf[0], 0x68697661);	//Type.
			write32(&buf[4], size() - 8);
			write32(&buf[8], microsec_per_frame);
			write32(&buf[12], max_bytes_per_sec);
			write32(&buf[16], padding_granularity);
			write32(&buf[20], flags);
			write32(&buf[24], videotrack.strh.length);
			write32(&buf[28], initial_frames);
			write32(&buf[32], tracks);
			write32(&buf[36], suggested_buffer_size);
			write32(&buf[40], videotrack.strf.width);
			write32(&buf[44], videotrack.strf.height);
			write32(&buf[48], 0);
			write32(&buf[52], 0);
			write32(&buf[56], 0);
			write32(&buf[64], 0);
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write avih");
		}
	};

	struct header_list
	{
		size_t size() { return 12 + avih.size() + videotrack.size() + audiotrack.size(); }
		avi_header avih;
		stream_header_list<stream_format_video> videotrack;
		stream_header_list<stream_format_audio> audiotrack;
		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(12);
			write32(&buf[0], 0x5453494CUL);		//List.
			write32(&buf[4], size() - 8);
			write32(&buf[8], 0x6c726468UL);		//Type.
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write hdrl");
			avih.serialize(out, videotrack, 2);
			videotrack.serialize(out);
			audiotrack.serialize(out);
		}
	};

	struct movi_chunk
	{
		unsigned long payload_size;
		size_t write_offset() { return 12; }
		size_t size() { return 12 + payload_size; }

		movi_chunk()
		{
			payload_size = 0;
		}

		void add_payload(size_t s)
		{
			payload_size = payload_size + s;
		}

		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(12);
			write32(&buf[0], 0x5453494CUL);		//List.
			write32(&buf[4], size() - 8);
			write32(&buf[8], 0x69766f6d);	//Type.
			out.write(&buf[0], buf.size());
			out.seekp(payload_size, std::ios_base::cur);
			if(!out)
				throw std::runtime_error("Can't write movi");
		}
	};

	struct index_entry
	{
		size_t size() { return 16; }
		unsigned long chunk_type;
		unsigned long flags;
		unsigned long offset;
		unsigned long length;
		index_entry(unsigned long _chunk_type, unsigned long _flags, unsigned long _offset,
			unsigned long _length)
		{
			chunk_type = _chunk_type;
			flags = _flags;
			offset = _offset;
			length = _length;
		}

		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(16);
			write32(&buf[0], chunk_type);
			write32(&buf[4], flags);
			write32(&buf[8], offset);
			write32(&buf[12], length);
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write index entry");
		}
	};

	struct idx1_chunk
	{
		void add_entry(const index_entry& entry) { entries.push_back(entry); }
		std::list<index_entry> entries;
		size_t size()
		{
			size_t s = 8;
			//Not exactly right, but much faster than the proper way.
			if(entries.empty())
				return s;
			s = s + entries.begin()->size() * entries.size();
			return s;
		}

		void serialize(std::ostream& out)
		{
			std::vector<char> buf;
			buf.resize(8);
			write32(&buf[0], 0x31786469UL);	//Type.
			write32(&buf[4], size() - 8);
			out.write(&buf[0], buf.size());
			if(!out)
				throw std::runtime_error("Can't write idx1");
			for(std::list<index_entry>::iterator i = entries.begin(); i != entries.end(); i++)
				i->serialize(out);
		}
	};

	size_t bpp_for_pixtype(enum avi_cscd_dumper::pixelformat pf)
	{
		switch(pf) {
		case avi_cscd_dumper::PIXFMT_RGB15_BE:
		case avi_cscd_dumper::PIXFMT_RGB15_LE:
		case avi_cscd_dumper::PIXFMT_RGB15_NE:
			return 2;
		case avi_cscd_dumper::PIXFMT_RGB24:
		case avi_cscd_dumper::PIXFMT_BGR24:
			return 3;
		case avi_cscd_dumper::PIXFMT_RGBX:
		case avi_cscd_dumper::PIXFMT_BGRX:
		case avi_cscd_dumper::PIXFMT_XRGB:
		case avi_cscd_dumper::PIXFMT_XBGR:
			return 4;
		default:
			return 0;
		}
	}

	size_t bps_for_sndtype(enum avi_cscd_dumper::soundformat sf)
	{
		switch(sf) {
		case avi_cscd_dumper::SNDFMT_SILENCE:
			return 0;
		case avi_cscd_dumper::SNDFMT_SIGNED_8:
		case avi_cscd_dumper::SNDFMT_UNSIGNED_8:
			return 1;
		case avi_cscd_dumper::SNDFMT_SIGNED_16BE:
		case avi_cscd_dumper::SNDFMT_SIGNED_16NE:
		case avi_cscd_dumper::SNDFMT_SIGNED_16LE:
		case avi_cscd_dumper::SNDFMT_UNSIGNED_16BE:
		case avi_cscd_dumper::SNDFMT_UNSIGNED_16NE:
		case avi_cscd_dumper::SNDFMT_UNSIGNED_16LE:
			return 2;
		};
		return 0;
	}

	inline unsigned short convert_audio_sample(const char* addr, enum avi_cscd_dumper::soundformat sf)
	{
		unsigned short a;
		unsigned short b;
		unsigned short magic = 258;
		bool little_endian = (*reinterpret_cast<char*>(&magic) == 2);
		switch(sf) {
		case avi_cscd_dumper::SNDFMT_SILENCE:
			return 32768;
		case avi_cscd_dumper::SNDFMT_SIGNED_8:
			return static_cast<unsigned short>((static_cast<short>(*addr) << 8)) + 32768;
		case avi_cscd_dumper::SNDFMT_UNSIGNED_8:
			return static_cast<unsigned short>(static_cast<unsigned char>(*addr)) << 8;
		case avi_cscd_dumper::SNDFMT_SIGNED_16BE:
			a = static_cast<unsigned char>(addr[0]);
			b = static_cast<unsigned char>(addr[1]);
			return a * 256 + b + 32768;
		case avi_cscd_dumper::SNDFMT_SIGNED_16NE:
			a = static_cast<unsigned char>(addr[0]);
			b = static_cast<unsigned char>(addr[1]);
			if(little_endian)
				return b * 256 + a + 32768;
			else
				return a * 256 + b + 32768;
		case avi_cscd_dumper::SNDFMT_SIGNED_16LE:
			a = static_cast<unsigned char>(addr[0]);
			b = static_cast<unsigned char>(addr[1]);
			return b * 256 + a + 32768;
		case avi_cscd_dumper::SNDFMT_UNSIGNED_16BE:
			a = static_cast<unsigned char>(addr[0]);
			b = static_cast<unsigned char>(addr[1]);
			return a * 256 + b;
		case avi_cscd_dumper::SNDFMT_UNSIGNED_16NE:
			a = static_cast<unsigned char>(addr[0]);
			b = static_cast<unsigned char>(addr[1]);
			if(little_endian)
				return b * 256 + a;
			else
				return a * 256 + b;
		case avi_cscd_dumper::SNDFMT_UNSIGNED_16LE:
			a = static_cast<unsigned char>(addr[0]);
			b = static_cast<unsigned char>(addr[1]);
			return a * 256 + b;
		};
		return 32768;
	}

	void copy_row(unsigned char* target, const unsigned char* src, unsigned width,
		enum avi_cscd_dumper::pixelformat pf)
	{
		unsigned ewidth = (width + 3) >> 2 << 2;
		size_t sbpp = bpp_for_pixtype(pf);
		size_t dbpp = (sbpp == 2) ? 2 : 3;
		unsigned short magic = 258;
		bool little_endian = (*reinterpret_cast<char*>(&magic) == 2);
		for(unsigned i = 0; i < width; i++) {
			switch(pf) {
			case avi_cscd_dumper::PIXFMT_RGB15_BE:
				target[dbpp * i + 0] = src[sbpp * i + 1];
				target[dbpp * i + 1] = src[sbpp * i + 0];
				break;
			case avi_cscd_dumper::PIXFMT_RGB15_NE:
				target[dbpp * i + 0] = src[sbpp * i + (little_endian ? 0 : 1)];
				target[dbpp * i + 1] = src[sbpp * i + (little_endian ? 1 : 0)];
				break;
			case avi_cscd_dumper::PIXFMT_RGB15_LE:
				target[dbpp * i + 0] = src[sbpp * i + 0];
				target[dbpp * i + 1] = src[sbpp * i + 1];
				break;
			case avi_cscd_dumper::PIXFMT_BGR24:
			case avi_cscd_dumper::PIXFMT_BGRX:
				target[dbpp * i + 0] = src[sbpp * i + 0];
				target[dbpp * i + 1] = src[sbpp * i + 1];
				target[dbpp * i + 2] = src[sbpp * i + 2];
				break;
			case avi_cscd_dumper::PIXFMT_RGB24:
			case avi_cscd_dumper::PIXFMT_RGBX:
				target[dbpp * i + 0] = src[sbpp * i + 2];
				target[dbpp * i + 1] = src[sbpp * i + 1];
				target[dbpp * i + 2] = src[sbpp * i + 0];
				break;
			case avi_cscd_dumper::PIXFMT_XRGB:
				target[dbpp * i + 0] = src[sbpp * i + 3];
				target[dbpp * i + 1] = src[sbpp * i + 2];
				target[dbpp * i + 2] = src[sbpp * i + 1];
				break;
			case avi_cscd_dumper::PIXFMT_XBGR:
				target[dbpp * i + 0] = src[sbpp * i + 1];
				target[dbpp * i + 1] = src[sbpp * i + 2];
				target[dbpp * i + 2] = src[sbpp * i + 3];
				break;
			}
		}
		memset(target + dbpp * width, 0, dbpp * (ewidth - width));
	}
}

struct avi_file_structure
{
	size_t write_offset() { return 12 + hdrl.size() + movi.write_offset(); }
	size_t size() { return 12 + hdrl.size() + movi.size() + idx1.size(); }
	header_list hdrl;
	movi_chunk movi;
	idx1_chunk idx1;
	void serialize(std::ostream& out)
	{
		std::vector<char> buf;
		buf.resize(12);
		write32(&buf[0], 0x46464952UL);		//RIFF.
		write32(&buf[4], size() - 8);
		write32(&buf[8], 0x20495641UL);		//Type.
		out.write(&buf[0], buf.size());
		if(!out)
			throw std::runtime_error("Can't write AVI header");
		hdrl.serialize(out);
		movi.serialize(out);
		idx1.serialize(out);
	}

	void start_data(std::ostream& out)
	{
		out.seekp(0, std::ios_base::beg);
		size_t reserved_for_header = write_offset();
		std::vector<char> tmp;
		tmp.resize(reserved_for_header);
		out.write(&tmp[0], tmp.size());
		if(!out)
			throw std::runtime_error("Can't write dummy header");
	}

	void finish_avi(std::ostream& out)
	{
		out.seekp(0, std::ios_base::beg);
		serialize(out);
		if(!out)
			throw std::runtime_error("Can't finish AVI");
	}
};

namespace
{
	void fill_avi_structure(struct avi_file_structure* avis, unsigned width, unsigned height, unsigned long fps_n,
		unsigned long fps_d, int mode, unsigned channels, unsigned long sampling_rate, bool bits16)
	{
		avis->hdrl.avih.microsec_per_frame = (Uint64)1000000 * fps_d / fps_n;
		avis->hdrl.avih.max_bytes_per_sec = 1000000;
		avis->hdrl.avih.padding_granularity = 0;
		avis->hdrl.avih.flags = 2064;
		avis->hdrl.avih.initial_frames = 0;
		avis->hdrl.avih.suggested_buffer_size = 1000000;
		avis->hdrl.videotrack.strh.handler = 0;
		avis->hdrl.videotrack.strh.flags = 0;
		avis->hdrl.videotrack.strh.priority = 0;
		avis->hdrl.videotrack.strh.language = 0;
		avis->hdrl.videotrack.strh.initial_frames = 0;
		avis->hdrl.videotrack.strh.start = 0;
		avis->hdrl.videotrack.strh.suggested_buffer_size = 1000000;
		avis->hdrl.videotrack.strh.quality = 9999;
		avis->hdrl.videotrack.strf.width = width;
		avis->hdrl.videotrack.strf.height = height;
		avis->hdrl.videotrack.strf.planes = 1;
		avis->hdrl.videotrack.strf.bit_count = (mode + 1) << 3;
		avis->hdrl.videotrack.strf.compression = 0x44435343;
		avis->hdrl.videotrack.strf.size_image = (1UL * (mode + 1) * width * height);
		avis->hdrl.videotrack.strf.resolution_x = 4000;
		avis->hdrl.videotrack.strf.resolution_y = 4000;
		avis->hdrl.videotrack.strf.clr_used = 0;
		avis->hdrl.videotrack.strf.clr_important = 0;
		avis->hdrl.videotrack.strf.fps_n = fps_n;
		avis->hdrl.videotrack.strf.fps_d = fps_d;
		avis->hdrl.audiotrack.strh.handler = 0;
		avis->hdrl.audiotrack.strh.flags = 0;
		avis->hdrl.audiotrack.strh.priority = 0;
		avis->hdrl.audiotrack.strh.language = 0;
		avis->hdrl.audiotrack.strh.initial_frames = 0;
		avis->hdrl.audiotrack.strh.start = 0;
		avis->hdrl.audiotrack.strh.suggested_buffer_size = 1000000;
		avis->hdrl.audiotrack.strh.quality = 9999;
		avis->hdrl.audiotrack.strf.format_tag = 1;
		avis->hdrl.audiotrack.strf.channels = channels;
		avis->hdrl.audiotrack.strf.samples_per_second = sampling_rate;
		avis->hdrl.audiotrack.strf.average_bytes_per_second = sampling_rate * channels * (bits16 ? 2 : 1);
		avis->hdrl.audiotrack.strf.block_align = channels * (bits16 ? 2 : 1);
		avis->hdrl.audiotrack.strf.bits_per_sample = (bits16 ? 16 : 8);
		avis->hdrl.audiotrack.strf.blocksize = channels * (bits16 ? 2 : 1);
	}
}

avi_cscd_dumper::avi_cscd_dumper(const std::string& prefix, const avi_cscd_dumper::global_parameters& global,
	const avi_cscd_dumper::segment_parameters& segment) throw(std::bad_alloc, std::runtime_error)
{
	dump_prefix = prefix;
	if(!global.sampling_rate || global.sampling_rate >= 0xFFFFFFFFUL)
		throw std::runtime_error("Sound sampling rate invalid");
	if(!global.channel_count || global.channel_count >= 0xFFFFU)
		throw std::runtime_error("Sound channel count invalid");
	if(!segment.fps_n || segment.fps_n >= 0xFFFFFFFFUL)
		throw std::runtime_error("FPS numerator invalid");
	if(!segment.fps_d || segment.fps_d >= 0xFFFFFFFFUL)
		throw std::runtime_error("FPS denominator invalid");
	if(!bpp_for_pixtype(segment.dataformat))
		throw std::runtime_error("Pixel format invalid");
	if(!segment.width || segment.width > 0xFFFCU)
		throw std::runtime_error("Width invalid");
	if(!segment.height || segment.height > 0xFFFCU)
		throw std::runtime_error("Height invalid");
	if(segment.deflate_level > 9)
		throw std::runtime_error("Invalid deflate level");
	gp_sampling_rate = global.sampling_rate;
	gp_channel_count = global.channel_count;
	gp_audio_16bit = global.audio_16bit;
	sp_fps_n = segment.fps_n;
	sp_fps_d = segment.fps_d;
	sp_dataformat = segment.dataformat;
	sp_width = segment.width;
	sp_height = segment.height;
	sp_max_segment_frames = segment.max_segment_frames;
	if(segment.default_stride)
		sp_stride = bpp_for_pixtype(segment.dataformat) * segment.width;
	else
		sp_stride = segment.stride;
	sp_keyframe_distance = segment.keyframe_distance;
	sp_deflate_level = segment.deflate_level;

	current_major_segment = 0;
	next_minor_segment = 0;
	current_major_segment_frames = 0;
	frames_since_last_keyframe = 0;
	avifile_structure = NULL;

	buffered_sound_samples = 0;
	switch_segments_on_next_frame = false;
	frame_period_counter = 0;

	quit_requested = false;
	flush_requested = false;
	flush_requested_forced = false;
	frame_processing = false;
	frame_pointer = NULL;
	exception_error_present = false;
	//std::cerr << "A" << std::endl;
	dumper_thread_obj dto;
	//std::cerr << "B" << std::endl;
	frame_thread = new thread_class(dto, this);
	//std::cerr << "C" << std::endl;
}

avi_cscd_dumper::~avi_cscd_dumper() throw()
{
	try {
		end();
	} catch(...) {
	}
}

avi_cscd_dumper::segment_parameters avi_cscd_dumper::get_segment_parameters() throw()
{
	segment_parameters sp;
	sp.dataformat = sp_dataformat;
	sp.default_stride = false;
	sp.deflate_level = sp_deflate_level;
	sp.fps_d = sp_fps_d;
	sp.fps_n = sp_fps_n;
	sp.height = sp_height;
	sp.keyframe_distance = sp_keyframe_distance;
	sp.stride = sp_stride;
	sp.width = sp_width;
	return sp;
}

void avi_cscd_dumper::set_segment_parameters(const avi_cscd_dumper::segment_parameters& segment) throw(std::bad_alloc,
	std::runtime_error)
{
	wait_frame_processing();
	if(!segment.fps_n || segment.fps_n >= 0xFFFFFFFFUL)
		throw std::runtime_error("FPS numerator invalid");
	if(!segment.fps_d || segment.fps_d >= 0xFFFFFFFFUL)
		throw std::runtime_error("FPS denominator invalid");
	if(segment.dataformat < PIXFMT_RGB15_LE || segment.dataformat > PIXFMT_XBGR)
		throw std::runtime_error("Pixel format invalid");
	if(!segment.width || segment.width > 0xFFFCU)
		throw std::runtime_error("Width invalid");
	if(!segment.height || segment.height > 0xFFFCU)
		throw std::runtime_error("Height invalid");
	if(segment.deflate_level > 9)
		throw std::runtime_error("Invalid deflate level");
	//Switch all parameters that can't be incompatible.
	if(segment.default_stride)
		sp_stride = bpp_for_pixtype(segment.dataformat) * segment.width;
	else
		sp_stride = segment.stride;
	sp_keyframe_distance = segment.keyframe_distance;
	sp_deflate_level = segment.deflate_level;
	sp_max_segment_frames = segment.max_segment_frames;

	bool incompatible = false;
	if(sp_fps_n != segment.fps_n)
		incompatible = true;
	if(sp_fps_d != segment.fps_d)
		incompatible = true;
	if(((sp_width + 3) >> 2) != ((segment.width + 3) >> 2))
		incompatible = true;
	if(((sp_height + 3) >> 2) != ((segment.height + 3) >> 2))
		incompatible = true;
	if(bpp_for_pixtype(sp_dataformat) == 2 && bpp_for_pixtype(segment.dataformat) != 2)
		incompatible = true;
	if(bpp_for_pixtype(sp_dataformat) != 2 && bpp_for_pixtype(segment.dataformat) == 2)
		incompatible = true;

	if(incompatible) {
		spn_dataformat = segment.dataformat;
		spn_fps_d = segment.fps_d;
		spn_fps_n = segment.fps_n;
		spn_height = segment.height;
		spn_width = segment.width;
		switch_segments_on_next_frame = true;
	} else {
		sp_dataformat = segment.dataformat;
		sp_fps_d = segment.fps_d;
		sp_fps_n = segment.fps_n;
		sp_height = segment.height;
		sp_width = segment.width;
		switch_segments_on_next_frame = false;
	}
}

void avi_cscd_dumper::audio(const void* audio, size_t samples, enum avi_cscd_dumper::soundformat format)
	throw(std::bad_alloc, std::runtime_error)
{
	if(exception_error_present)
		throw std::runtime_error(exception_error);
	const char* s = reinterpret_cast<const char*>(audio);
	size_t stride = bps_for_sndtype(format);
	size_t mstride = gp_channel_count * stride;
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	for(size_t i = 0; i < samples; i++) {
		for(size_t j = 0; j < gp_channel_count; j++) {
			unsigned short as = convert_audio_sample(s + mstride * i + stride * j, format);
			while(buffered_sound_samples * gp_channel_count + j >= sound_buffer.size())
				sound_buffer.resize(sound_buffer.size() + 128);
			sound_buffer[buffered_sound_samples * gp_channel_count + j] = as;
		}
		buffered_sound_samples++;
	}
	frame_mutex.unlock();
	request_flush_buffers(false);
}

void avi_cscd_dumper::audio(const void* laudio, const void* raudio, size_t samples,
	enum avi_cscd_dumper::soundformat format) throw(std::bad_alloc, std::runtime_error)
{
	if(exception_error_present)
		throw std::runtime_error(exception_error);
	if(gp_channel_count != 2)
		throw std::runtime_error("Split-stereo audio only allowed for stereo output");
	const char* l = reinterpret_cast<const char*>(laudio);
	const char* r = reinterpret_cast<const char*>(raudio);
	size_t stride = bps_for_sndtype(format);
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	for(size_t i = 0; i < samples; i++) {
		unsigned short ls = convert_audio_sample(l + stride * i, format);
		unsigned short rs = convert_audio_sample(r + stride * i, format);
		while(buffered_sound_samples * gp_channel_count >= sound_buffer.size())
			sound_buffer.resize(sound_buffer.size() + 128);
		sound_buffer[buffered_sound_samples * 2 + 0] = ls;
		sound_buffer[buffered_sound_samples * 2 + 1] = rs;
		buffered_sound_samples++;
	}
	frame_mutex.unlock();
	request_flush_buffers(false);
}

void avi_cscd_dumper::video(const void* framedata) throw(std::bad_alloc, std::runtime_error)
{
	if(exception_error_present)
		throw std::runtime_error(exception_error);
	wait_frame_processing();
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	frame_processing = true;
	frame_pointer = framedata;
	frame_cond.notify_all();
	//std::cerr << "Requesting processing of frame" << std::endl;
	frame_mutex.unlock();
#ifndef REALLY_USE_THREADS
	_video(framedata);
#endif
}

void avi_cscd_dumper::_video(const void* framedata)
{
	buffered_frame frame;
	frame.forcebreak = switch_segments_on_next_frame;
	//Switch parameters if needed.
	if(switch_segments_on_next_frame) {
		sp_dataformat = spn_dataformat;
		sp_fps_d = spn_fps_d;
		sp_fps_n = spn_fps_n;
		sp_height = spn_height;
		sp_width = spn_width;
		switch_segments_on_next_frame = false;
	}
	frame.compression_level = sp_deflate_level;
	frame.mode = (bpp_for_pixtype(sp_dataformat) == 2) ? 1 : 2;
	frame.fps_d = sp_fps_d;
	frame.fps_n = sp_fps_n;
	frame.width = sp_width;
	frame.height = sp_height;
	frame.keyframe = (++frames_since_last_keyframe >= sp_keyframe_distance);
	if(frame.keyframe)
		frames_since_last_keyframe = 0;
	size_t stride = ((bpp_for_pixtype(sp_dataformat) == 2) ? 2 : 3) * ((sp_width + 3) >> 2 << 2);
	size_t srcstride = (bpp_for_pixtype(sp_dataformat)) * sp_width;
	frame.data.resize(stride * ((sp_height + 3) >> 2 << 2));
	if(framedata == NULL)
		memset(&frame.data[0], 0, frame.data.size());
	else {
		const unsigned char* _framedata = reinterpret_cast<const unsigned char*>(framedata);
		unsigned extheight = (sp_height + 3) >> 2 << 2;
		for(unsigned i = 0; i < sp_height; i++)
			copy_row(&frame.data[(extheight - i - 1) * stride], _framedata + srcstride * i, sp_width,
				sp_dataformat);
		for(unsigned i = sp_height; i < extheight; i++)
			memset(&frame.data[(extheight - i - 1) * stride], 0, stride);
	}
	frame_mutex.lock();
	frame_processing = false;
	frame_pointer = NULL;
	frame_cond.notify_all();
	frame_mutex.unlock();
	frame_buffer.push_back(frame);
	flush_buffers(false);
}

void avi_cscd_dumper::end() throw(std::bad_alloc, std::runtime_error)
{
	flush_buffers(true);
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	quit_requested = true;
	frame_cond.notify_all();
	//std::cerr << "Requesting quit" << std::endl;
	frame_mutex.unlock();
	frame_thread->join();
	if(avifile_structure)
		end_segment();
}

size_t avi_cscd_dumper::emit_frame(const std::vector<unsigned char>& data, bool keyframe, unsigned level,
	unsigned mode)
{
	size_t nsize = data.size();
	if(previous_frame.size() != nsize) {
		previous_frame.resize(nsize);
		compression_input.resize(nsize);
		//8 bytes for AVI chunk header, 2 bytes for CSCD frame header. 3 bytes for padding.
		size_t pmaxsize = compressBound(nsize) + 13;
		if(pmaxsize > compression_output.size())
			compression_output.resize(pmaxsize);
	}
	if(!keyframe)
		for(size_t i = 0; i < nsize; i++)
			compression_input[i] = data[i] - previous_frame[i];
	else
		memcpy(&compression_input[0], &data[0], data.size());
	memcpy(&previous_frame[0], &data[0], nsize);
	uLongf l = compression_output.size();
	compress2(&compression_output[10], &l, &compression_input[0], compression_input.size(), level);
	//Pad the frame.
	while((l % 4) != 2)
		l++;
	compression_output[0] = '0';
	compression_output[1] = '0';
	compression_output[2] = 'd';
	compression_output[3] = 'b';	//strictly speaking, this is wrong, but FCEUX does this when dumping.
	compression_output[4] = (l + 2);
	compression_output[5] = (l + 2) >> 8;
	compression_output[6] = (l + 2) >> 16;
	compression_output[7] = (l + 2) >> 24;
	compression_output[8] = (keyframe ? 0x3 : 0x2) | (level << 4);
	compression_output[9] = mode << 2;
	return l + 10;
}

size_t avi_cscd_dumper::emit_sound(size_t samples)
{
	size_t packetsize = 8 + samples * gp_channel_count * (gp_audio_16bit ? 2 : 1);
	size_t towrite = samples * gp_channel_count;
	if(packetsize + 3 > compression_output.size())
		compression_output.resize(packetsize + 3);
	compression_output[0] = '0';
	compression_output[1] = '1';
	compression_output[2] = 'w';
	compression_output[3] = 'b';
	compression_output[4] = (packetsize - 8);
	compression_output[5] = (packetsize - 8) >> 8;
	compression_output[6] = (packetsize - 8) >> 16;
	compression_output[7] = (packetsize - 8) >> 24;
	size_t itr = 0;
	umutex_class _frame_mutex(frame_mutex);
	for(size_t i = 0; i < towrite; i++) {
		unsigned short sample = 0;
		if(itr < buffered_sound_samples * gp_channel_count)
			sample = sound_buffer[itr++];
		if(gp_audio_16bit) {
			compression_output[8 + 2 * i + 1] = (sample + 32768) >> 8;
			compression_output[8 + 2 * i + 0] = (sample + 32768);
		} else
			compression_output[8 + i] = (sample + 32768) >> 8;
	}
	if(itr < buffered_sound_samples * gp_channel_count) {
		memmove(&sound_buffer[0], &sound_buffer[itr], sizeof(unsigned short) * (buffered_sound_samples *
			gp_channel_count - itr));
		buffered_sound_samples -= itr / gp_channel_count;
	} else
		buffered_sound_samples = 0;
	while(packetsize & 3)
		packetsize++;
	return packetsize;
}

void avi_cscd_dumper::start_segment(unsigned major_seg, unsigned minor_seg)
{
	struct buffered_frame& f = *frame_buffer.begin();
	std::ostringstream name;
	name << dump_prefix << "_" << std::setfill('0') << std::setw(4) << major_seg << "_" << std::setfill('0')
		<< std::setw(4) << minor_seg << ".avi";
	avifile.open(name.str().c_str(), std::ios::out | std::ios::binary);
	if(!avifile)
		throw std::runtime_error("Can't open AVI file");
	avifile_structure = new avi_file_structure;
	fill_avi_structure(avifile_structure, (f.width + 3) >> 2 << 2, (f.height + 3) >> 2 << 2, f.fps_n,
		f.fps_d, f.mode, gp_channel_count, gp_sampling_rate, gp_audio_16bit);
	avifile_structure->start_data(avifile);
	frame_period_counter = 0;
}

void avi_cscd_dumper::end_segment()
{
	if(!avifile_structure)
		return;
	avifile_structure->finish_avi(avifile);
	avifile.flush();
	if(!avifile)
		throw std::runtime_error("Can't finish AVI");
	avifile.close();
	delete avifile_structure;
	avifile_structure = NULL;
}

bool avi_cscd_dumper::restart_segment_if_needed(bool force_break)
{
	if(!avifile_structure) {
		start_segment(current_major_segment, next_minor_segment++);
		return true;
	}
	if(sp_max_segment_frames && current_major_segment_frames >= sp_max_segment_frames) {
		end_segment();
		current_major_segment++;
		next_minor_segment = 0;
		start_segment(current_major_segment, next_minor_segment++);
		current_major_segment_frames = 0;
		return true;
	}
	if(force_break) {
		end_segment();
		start_segment(current_major_segment, next_minor_segment++);
		return true;
	}
	if(avifile_structure->size() > AVI_CUTOFF_SIZE) {
		end_segment();
		start_segment(current_major_segment, next_minor_segment++);
		return true;
	}
	return false;
}

void avi_cscd_dumper::write_frame_av(size_t samples)
{
	struct buffered_frame& f = *frame_buffer.begin();
	std::vector<unsigned char>& data = f.data;
	bool keyframe = f.keyframe;
	unsigned level = f.compression_level;
	unsigned mode = f.mode;
	bool force_break = f.forcebreak;

	size_t size;
	bool tmp = restart_segment_if_needed(force_break);
	keyframe = keyframe || tmp;
	size = emit_frame(data, keyframe, level, mode);
	emit_frame_stream(size, keyframe);
	size = emit_sound(samples);
	emit_sound_stream(size, samples);
	current_major_segment_frames++;
	frame_buffer.pop_front();
}

void avi_cscd_dumper::emit_frame_stream(size_t size, bool keyframe)
{
	avifile_structure->idx1.add_entry(index_entry(0x62643030UL, keyframe ? 0x10 : 0,
		avifile_structure->movi.payload_size + 4, size - 8));
	avifile.write(reinterpret_cast<const char*>(&compression_output[0]), size);
	avifile_structure->hdrl.videotrack.strh.add_frames(1);
	avifile_structure->movi.add_payload(size);
}

void avi_cscd_dumper::emit_sound_stream(size_t size, size_t samples)
{
	avifile_structure->idx1.add_entry(index_entry(0x62773130UL, 0x10,
		avifile_structure->movi.payload_size + 4, size - 8));
	avifile.write(reinterpret_cast<const char*>(&compression_output[0]), size);
	avifile_structure->hdrl.audiotrack.strh.add_frames(samples);
	avifile_structure->movi.add_payload(size);
}

size_t avi_cscd_dumper::samples_for_next_frame()
{
	//The average number of samples per frame needs to be:
	//samplerate * fps_d / fps_n.
	struct buffered_frame& f = *frame_buffer.begin();
	unsigned long critical = static_cast<Uint64>(gp_sampling_rate) * f.fps_d % f.fps_n;
	unsigned long ret = static_cast<Uint64>(gp_sampling_rate) * f.fps_d / f.fps_n;
	if(frame_period_counter % f.fps_n < critical)
		ret++;
	return ret;
}

void avi_cscd_dumper::flush_buffers(bool forced)
{
	while(!frame_buffer.empty()) {
		unsigned long s_fps_n = frame_buffer.begin()->fps_n;
		size_t samples = samples_for_next_frame();
		frame_mutex.lock();
		size_t asamples = buffered_sound_samples;
		frame_mutex.unlock();
		if(!forced && asamples < samples)
			break;
		write_frame_av(samples);
		frame_period_counter++;
		frame_period_counter %= s_fps_n;
	}
}

void avi_cscd_dumper::request_flush_buffers(bool forced)
{
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	flush_requested = true;
	flush_requested_forced = forced;
	frame_cond.notify_all();
	//std::cerr << "Requesting buffer flush (" << flush_requested_forced << ")" << std::endl;
	frame_mutex.unlock();
#ifndef REALLY_USE_THREADS
	flush_buffers(forced);
#endif
}

bool avi_cscd_dumper::is_frame_processing() throw()
{
	return frame_processing;
}

void avi_cscd_dumper::wait_frame_processing() throw()
{
	umutex_class _frame_mutex(frame_mutex);
	while(frame_processing) {
		//std::cerr << "Waiting for frame to process." << std::endl;
		frame_cond.wait(_frame_mutex);
	}
	//std::cerr << "Ok, frame processed, returning" << std::endl;
}

int avi_cscd_dumper::encode_thread()
{
	try {
		//std::cerr << "Encoder thread ready." << std::endl;
start:
		frame_mutex.lock();
		if(quit_requested && !frame_pointer && !flush_requested) {
			//std::cerr << "OK, quitting on request." << std::endl;
			goto end;
		}
		if(frame_pointer || frame_processing) {
			//std::cerr << "Servicing video frame" << std::endl;
			frame_mutex.unlock();
			const void* f = (const void*)frame_pointer;
			_video(f);
			frame_mutex.lock();
		}
		if(flush_requested) {
			//std::cerr << "Servicing flush" << std::endl;
			frame_mutex.unlock();
			flush_buffers(flush_requested_forced);
			frame_mutex.lock();
			flush_requested = false;
		}
		frame_mutex.unlock();
		{
			umutex_class _frame_mutex(frame_mutex);
			while(!quit_requested && !frame_pointer && !flush_requested && !frame_processing) {
				//std::cerr << "Waiting for work." << std::endl;
				frame_cond.wait(_frame_mutex);
			}
		}
		goto start;
end:
		frame_mutex.unlock();
		return 0;
	} catch(std::exception& e) {
		set_capture_error(e.what());
		return 1;
	}
}

void avi_cscd_dumper::set_capture_error(const std::string& err)
{
	frame_mutex.lock();
	exception_error = err;
	frame_mutex.unlock();
	exception_error_present = true;
}


