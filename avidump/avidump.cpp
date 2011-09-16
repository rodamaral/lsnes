#include "avidump.hpp"
#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <zlib.h>
//#include "misc.hpp"

#define AVI_CUTOFF 2000000000

avi_frame::avi_frame(uint32_t _flags, uint32_t _type, uint32_t _offset, uint32_t _size)
{
	flags = _flags;
	type = _type;
	offset = _offset;
	size = _size;
}

void avi_frame::write(uint8_t* buf)
{
	//Yes, this is written big-endian!
	buf[0] = type >> 24;
	buf[1] = type >> 16;
	buf[2] = type >> 8;
	buf[3] = type;

	buf[4] = flags;
	buf[5] = flags >> 8;
	buf[6] = flags >> 16;
	buf[7] = flags >> 24;

	buf[8] = offset;
	buf[9] = offset >> 8;
	buf[10] = offset >> 16;
	buf[11] = offset >> 24;

	buf[12] = size;
	buf[13] = size >> 8;
	buf[14] = size >> 16;
	buf[15] = size >> 24;
}

namespace
{
	struct dumper_thread_obj
	{
		int operator()(avidumper* d)
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
}

int avidumper::encode_thread()
{
	umutex_class _frame_mutex(frame_mutex);
	while(!sigquit) {
		if(mt_data) {
			_frame_mutex.unlock();
			on_frame_threaded(mt_data, mt_width, mt_height, mt_fps_n, mt_fps_d);
			_frame_mutex.lock();
		}
		mt_data = NULL;
		frame_cond.notify_all();
		frame_cond.wait(_frame_mutex);
	}
	return 0;
}

avidumper::avidumper(const std::string& _prefix, struct avi_info parameters)
{
	compression_level = parameters.compression_level;
	audio_sampling_rate = parameters.audio_sampling_rate;
	keyframe_interval = parameters.keyframe_interval;

	avi_open = false;
	capture_error = false;
	pwidth = 0xFFFF;
	pheight = 0xFFFF;
	pfps_n = 0xFFFFFFFFU;
	pfps_d = 0xFFFFFFFFU;
	current_segment = 0;
	prefix = _prefix;
	total_data = 0;
	total_frames = 0;
	total_samples = 0;
	audio_put_ptr = 0;
	audio_get_ptr = 0;
	audio_commit_ptr = 0;
	mt_data = NULL;
	mt_width = 0;
	mt_height = 0;
	mt_fps_n = 0;
	mt_fps_d = 0;
	sigquit = false;

	std::cerr << "Creating thread..." << std::endl;
	dumper_thread_obj dto;
	frame_thread = new thread_class(dto, this);
	std::cerr << "Created thread..." << std::endl;
}

void avidumper::set_capture_error(const char* err) throw()
{
	try {
		capture_error = true;
		capture_error_str = err;
	} catch(std::bad_alloc& e) {
	}
}

void avidumper::on_sample(short left, short right) throw(std::bad_alloc, std::runtime_error)
{
	if(capture_error)
		throw std::runtime_error("Video capture thread crashed: " + capture_error_str);
	audio_buffer[audio_put_ptr++] = left;
	audio_buffer[audio_put_ptr++] = right;
	if(audio_put_ptr == AVIDUMPER_AUDIO_BUFFER)
		audio_put_ptr = 0;
}

void avidumper::on_frame(const uint32_t* data, uint16_t width, uint16_t height, uint32_t fps_n, uint32_t fps_d)
	throw(std::bad_alloc, std::runtime_error)
{
	if(capture_error)
		throw std::runtime_error("Video capture thread crashed: " + capture_error_str);
	wait_idle();
	frame_mutex.lock();
	audio_commit_ptr = audio_put_ptr;
	mt_data = data;
	mt_width = width;
	mt_height = height;
	mt_fps_n = fps_n;
	mt_fps_d = fps_d;
	frame_cond.notify_all();
	frame_mutex.unlock();
#ifdef NO_THREADS
	on_frame_threaded(mt_data, mt_width, mt_height, mt_fps_n, mt_fps_d);
	mt_data = NULL;
#endif
}

namespace
{
	std::string fmtint(uint64_t val, unsigned prec)
	{
		std::ostringstream s2;
		s2 << std::setw(prec) << std::setfill(' ') << val;
		return s2.str();
	}

	std::string fmtdbl(double val, unsigned prec)
	{
		std::ostringstream s2;
		s2 << std::setw(prec) << std::setfill(' ') << val;
		std::string x = s2.str();
		if(x.length() == prec)
			return x;
		size_t p = x.find_first_of("e");
		if(p >= x.length())
			return x.substr(0, prec);
		return x.substr(0, p - (x.length() - prec)) + x.substr(p);
	}
}

void avidumper::print_summary(std::ostream& str)
{
	uint64_t local_segno = current_segment;
	uint64_t local_vframes = segment_frames;
	double local_vlength = segment_frames * static_cast<double>(pfps_d) / pfps_n;
	uint64_t global_vframes = total_frames;
	double global_vlength = total_frames * static_cast<double>(pfps_d) / pfps_n;
	uint64_t local_aframes = segment_samples;
	double local_alength = static_cast<double>(segment_samples) / audio_sampling_rate;
	uint64_t global_aframes = total_samples;
	double global_alength = static_cast<double>(total_samples) / audio_sampling_rate;
	uint64_t local_size = segment_movi_ptr + 352 + 16 * segment_chunks.size();
	uint64_t global_size = total_data + 8 + 16 * segment_chunks.size();

	std::ostringstream s2;

	s2 << "Quantity        |This segment         |All segments         |" << std::endl;
	s2 << "----------------+---------------------+---------------------+" << std::endl;
	s2 << "Segment number  |           " << fmtint(local_segno, 10) << "|                  N/A|" << std::endl;
	s2 << "Video stream    |" << fmtint(local_vframes, 10) << "/" << fmtdbl(local_vlength, 10) << "|"
		<< fmtint(global_vframes, 10) << "/" << fmtdbl(global_vlength, 10) << "|" << std::endl;
	s2 << "Audio stream    |" << fmtint(local_aframes, 10) << "/" << fmtdbl(local_alength, 10) << "|"
		<< fmtint(global_aframes, 10) << "/" << fmtdbl(global_alength, 10) << "|" << std::endl;
	s2 << "A/V desync      |           " << fmtdbl(local_alength - local_vlength, 10) << "|           "
		<< fmtdbl(global_alength - global_vlength, 10) << "|" << std::endl;
	s2 << "Size            |           " << fmtint(local_size, 10) << "|           "
		<< fmtint(global_size, 10) << "|" << std::endl;
	s2 << "----------------+---------------------+---------------------+" << std::endl;

	str << s2.str();
}

void avidumper::on_frame_threaded(const uint32_t* data, uint16_t width, uint16_t height, uint32_t fps_n, uint32_t fps_d)
	throw(std::bad_alloc, std::runtime_error)
{
	//The AVI part of sound to write is [audio_get, audio_commit). We don't write part [audio_commit,audio_put)
	//yet, as it is being concurrently written. Also grab lock to read the commit value. Also, if global frame
	//counter is 0, don't write audio to avoid A/V desync.
	frame_mutex.lock();
	unsigned commit_to = audio_commit_ptr;
	frame_mutex.unlock();
	if(total_frames)
		flush_audio_to(commit_to);
	else
		audio_get_ptr = commit_to;

	if(segment_movi_ptr > AVI_CUTOFF - 16 * segment_chunks.size())
		fixup_avi_header_and_close();

	uint16_t rheight = (height + 3) / 4 * 4;
	bool this_is_keyframe;
	if(width != pwidth || height != pheight || fps_n != pfps_n || fps_d != pfps_d || !avi_open) {
		std::cerr << "Starting segment # " << current_segment << ": " << width << "x" << height << "."
			<< std::endl;
		fixup_avi_header_and_close();
		pwidth = width;
		pheight = height;
		pfps_n = fps_n;
		pfps_d = fps_d;
		pframe.resize(4 * static_cast<size_t>(width) * height);
		tframe.resize(4 * static_cast<size_t>(width) * rheight);
		cframe.resize(compressBound(4 * static_cast<size_t>(width) * rheight) + 13);
		memset(&tframe[0], 0, 4 * static_cast<size_t>(width) * rheight);
		open_and_write_avi_header(width, rheight, fps_n, fps_d);
	}

	this_is_keyframe = (segment_frames == 0 || segment_frames - segment_last_keyframe >= keyframe_interval);

	if(this_is_keyframe) {
		memcpy(&tframe[0], data, 4 * static_cast<size_t>(width) * height);
		segment_last_keyframe = segment_frames;
	} else {
		memcpy(&tframe[0], data, 4 * static_cast<size_t>(width) * height);
		for(size_t i = 0; i < 4 * static_cast<size_t>(width) * height; i++)
			tframe[i] -= pframe[i];
	}
	size_t l = cframe.size() - 10;
	if(compress2(&cframe[10], &l, &tframe[0], tframe.size(), compression_level) != Z_OK)
		throw std::runtime_error("Error compressing frame");
	//Pad the frame.
	while((l % 4) != 2)
		l++;
	cframe[0] = '0';
	cframe[1] = '0';
	cframe[2] = 'd';
	cframe[3] = 'b';	//strictly speaking, this is wrong, but FCEUX does this when dumping.
	cframe[4] = (l + 2);
	cframe[5] = (l + 2) >> 8;
	cframe[6] = (l + 2) >> 16;
	cframe[7] = (l + 2) >> 24;
	cframe[8] = (this_is_keyframe ? 0x3 : 0x2) | (compression_level << 4);
	cframe[9] = 12;
	avi_stream.write(reinterpret_cast<char*>(&cframe[0]), l + 10);
	if(!avi_stream)
		throw std::runtime_error("Error writing video frame");
	//Flags is 0x10 for keyframes, because those frames are always keyframes, chunks, independent and take time.
	//For non-keyframes, flags are 0x00 (chunk, not a keyframe).
	segment_chunks.push_back(avi_frame(this_is_keyframe ? 0x10 : 0x00, 0x30306462, segment_movi_ptr + 4, l + 2));
	segment_movi_ptr += (l + 10);
	total_data += (l + 10);
	segment_frames++;
	total_frames++;

	if((segment_frames % 1200) == 0)
		print_summary(std::cerr);

	memcpy(&pframe[0], data, 4 * static_cast<size_t>(width) * height);
}

void avidumper::on_end() throw(std::bad_alloc, std::runtime_error)
{
	if(capture_error)
		throw std::runtime_error("Video capture thread crashed: " + capture_error_str);
	frame_mutex.lock();
	sigquit = true;
	frame_cond.notify_all();
	frame_mutex.unlock();
	frame_thread->join();

	flush_audio_to(audio_put_ptr);
	fixup_avi_header_and_close();
}

namespace
{
	struct str
	{
		str(const char* _s)
		{
			s = _s;
		}
		const char* s;
	};

	struct u16
	{
		u16(uint16_t _v)
		{
			v = _v;
		}
		uint16_t v;
	};

	struct u32
	{
		u32(uint32_t _v)
		{
			v = _v;
		}
		uint32_t v;
	};

	struct ptr
	{
		ptr(uint32_t& _p) : p(_p)
		{
		}
		uint32_t& p;
	};

	void append(std::vector<uint8_t>& v)
	{
	}

	template<typename... rest>
        void append(std::vector<uint8_t>& v, struct str s, rest... _rest)
	{
		const char* str = s.s;
		while(*str)
			v.push_back(*(str++));
		append(v, _rest...);
	}

	template<typename... rest>
	void append(std::vector<uint8_t>& v, struct u16 u, rest... _rest)
	{
		uint16_t val = u.v;
		v.push_back(val);
		v.push_back(val >> 8);
		append(v, _rest...);
	}

	template<typename... rest>
	void append(std::vector<uint8_t>& v, struct u32 u, rest... _rest)
	{
		uint32_t val = u.v;
		v.push_back(val);
		v.push_back(val >> 8);
		v.push_back(val >> 16);
		v.push_back(val >> 24);
		append(v, _rest...);
	}

	template<typename... rest>
	void append(std::vector<uint8_t>& v, ptr u, rest... _rest)
	{
		u.p = v.size();
		append(v, _rest...);
	}

	void fix_write(std::ostream& str, uint32_t off, uint32_t val)
	{
		str.seekp(off, std::ios::beg);
		char x[4];
		x[0] = val;
		x[1] = val >> 8;
		x[2] = val >> 16;
		x[3] = val >> 24;
		str.write(x, 4);
		if(!str)
			throw std::runtime_error("Can't fixup AVI header");
	}
}

void avidumper::flush_audio_to(unsigned commit_to)
{
	if(!avi_open)
		return;

	//Count the number of samples to actually write.
	unsigned samples_to_write = 0;
	unsigned aptr = audio_get_ptr;
	unsigned idx = 8;
	while(aptr != commit_to) {
		samples_to_write++;
		if((aptr += 2) == AVIDUMPER_AUDIO_BUFFER)
			aptr = 0;
	}

	std::vector<uint8_t> buf;
	buf.resize(8 + 4 * samples_to_write);
	buf[0] = '0';
	buf[1] = '1';
	buf[2] = 'w';
	buf[3] = 'b';
	buf[4] = (4 * samples_to_write);
	buf[5] = (4 * samples_to_write) >> 8;
	buf[6] = (4 * samples_to_write) >> 16;
	buf[7] = (4 * samples_to_write) >> 24;

	while(audio_get_ptr != commit_to) {
		//Write sample.
		buf[idx] = static_cast<unsigned short>(audio_buffer[audio_get_ptr]);
		buf[idx + 1] = static_cast<unsigned short>(audio_buffer[audio_get_ptr]) >> 8;
		buf[idx + 2] = static_cast<unsigned short>(audio_buffer[audio_get_ptr + 1]);
		buf[idx + 3] = static_cast<unsigned short>(audio_buffer[audio_get_ptr + 1]) >> 8;
		idx += 4;
		segment_samples++;
		total_samples++;
		if((audio_get_ptr += 2) == AVIDUMPER_AUDIO_BUFFER)
			audio_get_ptr = 0;
	}
	assert(idx == 8 + 4 * samples_to_write);
	avi_stream.write(reinterpret_cast<char*>(&buf[0]), idx);
	if(!avi_stream)
		throw std::runtime_error("Error writing audio frame");
	//Flags is 0x10, because sound frames are always keyframes, chunks, independent and take time.
	segment_chunks.push_back(avi_frame(0x10, 0x30317762, segment_movi_ptr + 4, 4 * samples_to_write));
	segment_movi_ptr += idx;
	total_data += idx;
}

void avidumper::open_and_write_avi_header(uint16_t width, uint16_t height, uint32_t fps_n, uint32_t fps_d)
{
	std::string fstr;
	{
		std::ostringstream str;
		str << prefix << "_" << std::setw(9) << std::setfill('0') << (current_segment++) << ".avi";
		fstr = str.str();
	}
	avi_stream.clear();
	avi_stream.open(fstr.c_str(), std::ios::out | std::ios::binary);
	if(!avi_stream)
		throw std::runtime_error("Can't open output AVI file");
	std::vector<uint8_t> aviheader;
	uint32_t usecs_per_frame = static_cast<uint32_t>(1000000ULL * fps_d / fps_n);

	/* AVI main chunk header. */
	/* The tentative AVI header size of 336 doesn't include the video data, so we need to fix it up later. */
	append(aviheader, str("RIFF"), ptr(fixup_avi_size), u32(336), str("AVI "));
	/* Header list header. Has 312 bytes of data. */
	append(aviheader, str("LIST"), u32(312), str("hdrl"));
	/* Main AVI header. */
	append(aviheader, str("avih"), u32(56),		/* 56 byte header of type avih. */
		u32(usecs_per_frame),			/* usecs per frame. */
		u32(1000000),				/* Max transfer rate... Give some random value. */
		u32(0),					/* Padding granularity (no padding). */
		u32(2064),				/* Flags... Has index, trust chunk types */
		ptr(fixup_avi_frames), u32(0),		/* Frame count... To be fixed later. */
		u32(0),					/* Initial frames... We don't have any. */
		u32(2),					/* 2 streams (video + audio). */
		u32(1000000),				/* Suggested buffer size... Give some random value. */
		u32(width), u32(height),		/* Size of image. */
		u32(0), u32(0), u32(0), u32(0));	/* Reserved. */
	/* Stream list header For stream #1, 124 bytes of data. */
	append(aviheader, str("LIST"), u32(124), str("strl"));
	/* Stream header for stream #1 (video). */
	append(aviheader, str("strh"), u32(64),		/* 64 byte header of type strh */
		str("vids"), u32(0),			/* Video data??? */
		u32(0),					/* Some flags that are all clear. */
		u16(0), u16(0),				/* Priority and language... Doesn't matter. */
		u32(0),					/* Initial frames... We don't have any. */
		u32(fps_d), u32(fps_n),			/* Frame rate is fps_n / fps_d. */
		u32(0),					/* Starting time... It starts at t=0. */
		ptr(fixup_avi_length), u32(0),		/* Video length (to be fixed later). */
		u32(1000000),				/* Suggested buffer size... Just give some random value. */
		u32(9999),				/* Quality... Doesn't matter. */
		u32(4),					/* Video sample size... 32bpp. */
		u32(0), u32(0),				/* Bounding box upper left. */
		u32(width), u32(height));		/* Bounding box lower right. */
	/* BITMAPINFO header for the video stream. */
	append(aviheader, str("strf"), u32(40), 	/* 40 byte header of type strf. */
		u32(40),				/* BITMAPINFOHEADER is 40 bytes. */
		u32(width), u32(height),		/* Image size. */
		u16(1), u16(32),			/* 1 plane, 32 bits (RGB32). */
		str("CSCD"),				/* Compressed with Camstudio codec. */
		u32(4 * width * height),		/* Image size. */
		u32(4000), u32(4000),			/* Resolution... Give some random values. */
		u32(0), u32(0));			/* Colors used values (0 => All colors used). */
	/* Stream list header For stream #2, 104 bytes of data. */
	append(aviheader, str("LIST"), u32(104), str("strl"));
	/* Stream header for stream #2. */
	append(aviheader, str("strh"), u32(64),		/* 64 byte header of type strh */
		str("auds"), u32(0),			/* audio data??? */
		u32(0),					/* Flags... None set. */
		u16(0), u16(0),				/* Priority and language... Doesn't matter. */
		u32(0),					/* Initial frames... None. */
		u32(1), u32(audio_sampling_rate),	/* Audio sampling rate. */
		u32(0),					/* Starts at t=0s. */
		ptr(fixup_avi_a_length), u32(0),	/* Audio length (to be fixed later). */
		u32(4096),				/* Suggested buffer size... Some random value. */
		u32(5),					/* Audio quality... Some random value. */
		u32(4),					/* Sample size (16bit Stereo PCM). */
		u32(0), u32(0), u32(0), u32(0));	/* Bounding box, not sane for audio data. */
	/* WAVEFORMAT header for the audio stream. */
	append(aviheader, str("strf"), u32(20),		/* 20 byte header of type strf. */
		u16(1),					/* PCM. */
		u16(2),					/* Stereo. */
		u32(audio_sampling_rate),		/* Audio Sampling rate. */
		u32(4 * audio_sampling_rate),		/* Audio transfer rate (4 times sampling rate). */
		u16(4),					/* Sample size. */
		u16(16),				/* Bits per sample. */
		u16(0),					/* Extension size... We don't have extension. */
		u16(0));				/* Dummy. */
	/* MOVI list header. 4 bytes without movie data. */
	append(aviheader, str("LIST"), ptr(fixup_movi_size), u32(4), str("movi"));
	avi_stream.write(reinterpret_cast<char*>(&aviheader[0]), aviheader.size());
	if(!avi_stream)
		throw std::runtime_error("Can't write AVI header");
	total_data += aviheader.size();
	avi_open = true;
	segment_movi_ptr = 0;
	segment_frames = 0;
	segment_samples = 0;
	segment_last_keyframe = 0;
	segment_chunks.clear();
}

void avidumper::fixup_avi_header_and_close()
{
	if(!avi_open)
		return;
	print_summary(std::cerr);
	uint8_t buf[16];
	buf[0] = 'i';
	buf[1] = 'd';
	buf[2] = 'x';
	buf[3] = '1';
	buf[4] = (16 * segment_chunks.size());
	buf[5] = (16 * segment_chunks.size()) >> 8;
	buf[6] = (16 * segment_chunks.size()) >> 16;
	buf[7] = (16 * segment_chunks.size()) >> 24;
	avi_stream.write(reinterpret_cast<char*>(buf), 8);
	if(!avi_stream)
		throw std::runtime_error("Error writing index header");
	for(auto i = segment_chunks.begin(); i != segment_chunks.end(); ++i) {
		i->write(buf);
		avi_stream.write(reinterpret_cast<char*>(buf), 16);
		if(!avi_stream)
			throw std::runtime_error("Error writing index entry");
	}
	total_data += (8 + 16 * segment_chunks.size());

	fix_write(avi_stream, fixup_avi_size, 344 + segment_movi_ptr + 16 * segment_chunks.size());
	fix_write(avi_stream, fixup_avi_frames, segment_frames);
	fix_write(avi_stream, fixup_avi_length, segment_frames);
	fix_write(avi_stream, fixup_avi_a_length, segment_samples);
	fix_write(avi_stream, fixup_movi_size, 4 + segment_movi_ptr);
	segment_chunks.clear();
	avi_stream.close();
	avi_open = false;
}

avidumper::~avidumper() throw()
{
	delete frame_thread;
}

void avidumper::wait_idle() throw()
{
	umutex_class _frame_mutex(frame_mutex);
	while(1) {
		if(!mt_data)
			return;
		frame_cond.wait(_frame_mutex);
	}
}
