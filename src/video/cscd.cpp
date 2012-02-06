#include "video/cscd.hpp"
#include <zlib.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <list>
#include "video/avi_structure.hpp"
#include "library/serialization.hpp"

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

	void copy_row(unsigned char* target, const unsigned char* src, unsigned width,
		enum avi_cscd_dumper::pixelformat pf)
	{
		unsigned ewidth = (width + 3) >> 2 << 2;
		for(unsigned i = 0; i < width; i++) {
			switch(pf) {
			case avi_cscd_dumper::PIXFMT_BGRX:
				target[3 * i + 0] = src[4 * i + 0];
				target[3 * i + 1] = src[4 * i + 1];
				target[3 * i + 2] = src[4 * i + 2];
				break;
			case avi_cscd_dumper::PIXFMT_RGBX:
				target[3 * i + 0] = src[4 * i + 2];
				target[3 * i + 1] = src[4 * i + 1];
				target[3 * i + 2] = src[4 * i + 0];
				break;
			case avi_cscd_dumper::PIXFMT_XRGB:
				target[3 * i + 0] = src[4 * i + 3];
				target[3 * i + 1] = src[4 * i + 2];
				target[3 * i + 2] = src[4 * i + 1];
				break;
			case avi_cscd_dumper::PIXFMT_XBGR:
				target[3 * i + 0] = src[4 * i + 1];
				target[3 * i + 1] = src[4 * i + 2];
				target[3 * i + 2] = src[4 * i + 3];
				break;
			}
		}
		memset(target + 3 * width, 0, 3 * (ewidth - width));
	}
}

namespace
{
	void fill_avi_structure(struct avi_file_structure* avis, unsigned width, unsigned height, unsigned long fps_n,
		unsigned long fps_d, unsigned long sampling_rate)
	{
		avis->hdrl.avih.microsec_per_frame = (uint64_t)1000000 * fps_d / fps_n;
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
		avis->hdrl.videotrack.strf.bit_count = 24;
		avis->hdrl.videotrack.strf.compression = 0x44435343;
		avis->hdrl.videotrack.strf.size_image = (3UL * width * height);
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
		avis->hdrl.audiotrack.strf.channels = 2;
		avis->hdrl.audiotrack.strf.samples_per_second = sampling_rate;
		avis->hdrl.audiotrack.strf.average_bytes_per_second = sampling_rate * 4;
		avis->hdrl.audiotrack.strf.block_align = 4;
		avis->hdrl.audiotrack.strf.bits_per_sample = 16;
		avis->hdrl.audiotrack.strf.blocksize = 4;
	}
}

avi_cscd_dumper::avi_cscd_dumper(const std::string& prefix, const avi_cscd_dumper::global_parameters& global,
	const avi_cscd_dumper::segment_parameters& segment) throw(std::bad_alloc, std::runtime_error)
{
	dump_prefix = prefix;
	if(!global.sampling_rate || global.sampling_rate >= 0xFFFFFFFFUL)
		throw std::runtime_error("Sound sampling rate invalid");
	if(!segment.fps_n || segment.fps_n >= 0xFFFFFFFFUL)
		throw std::runtime_error("FPS numerator invalid");
	if(!segment.fps_d || segment.fps_d >= 0xFFFFFFFFUL)
		throw std::runtime_error("FPS denominator invalid");
	if(segment.dataformat < PIXFMT_RGBX || segment.dataformat > PIXFMT_XBGR)
		throw std::runtime_error("Pixel format invalid");
	if(!segment.width || segment.width > 0xFFFCU)
		throw std::runtime_error("Width invalid");
	if(!segment.height || segment.height > 0xFFFCU)
		throw std::runtime_error("Height invalid");
	if(segment.deflate_level > 9)
		throw std::runtime_error("Invalid deflate level");
	gp_sampling_rate = global.sampling_rate;
	sp_fps_n = segment.fps_n;
	sp_fps_d = segment.fps_d;
	sp_dataformat = segment.dataformat;
	sp_width = segment.width;
	sp_height = segment.height;
	sp_max_segment_frames = segment.max_segment_frames;
	if(segment.default_stride)
		sp_stride = 4 * segment.width;
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
	delete frame_thread;
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
	if(segment.dataformat < PIXFMT_RGBX || segment.dataformat > PIXFMT_XBGR)
		throw std::runtime_error("Pixel format invalid");
	if(!segment.width || segment.width > 0xFFFCU)
		throw std::runtime_error("Width invalid");
	if(!segment.height || segment.height > 0xFFFCU)
		throw std::runtime_error("Height invalid");
	if(segment.deflate_level > 9)
		throw std::runtime_error("Invalid deflate level");
	//Switch all parameters that can't be incompatible.
	if(segment.default_stride)
		sp_stride = 4 * segment.width;
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

void avi_cscd_dumper::audio(const short* audio, size_t samples) throw(std::bad_alloc, std::runtime_error)
{
	if(exception_error_present)
		throw std::runtime_error(exception_error);
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	for(size_t i = 0; i < samples; i++) {
		for(size_t j = 0; j < 2; j++) {
			unsigned short as = static_cast<unsigned short>(audio[2 * i + j]) + 32768;
			while(buffered_sound_samples * 2 + j >= sound_buffer.size())
				sound_buffer.resize(sound_buffer.size() + 128);
			sound_buffer[buffered_sound_samples * 2 + j] = as;
		}
		buffered_sound_samples++;
	}
	frame_mutex.unlock();
	request_flush_buffers(false);
}

void avi_cscd_dumper::audio(const short* laudio, const short* raudio, size_t samples) throw(std::bad_alloc,
	std::runtime_error)
{
	if(exception_error_present)
		throw std::runtime_error(exception_error);
	//std::cerr << "Locking lock." << std::endl;
	frame_mutex.lock();
	//std::cerr << "Locked lock." << std::endl;
	for(size_t i = 0; i < samples; i++) {
		unsigned short ls = static_cast<unsigned short>(laudio[i]) + 32768;
		unsigned short rs = static_cast<unsigned short>(raudio[i]) + 32768;
		while(buffered_sound_samples * 2 >= sound_buffer.size())
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
	frame.fps_d = sp_fps_d;
	frame.fps_n = sp_fps_n;
	frame.width = sp_width;
	frame.height = sp_height;
	frame.keyframe = (++frames_since_last_keyframe >= sp_keyframe_distance);
	if(frame.keyframe)
		frames_since_last_keyframe = 0;
	size_t stride = 3 * ((sp_width + 3) >> 2 << 2);
	size_t srcstride = 4 * sp_width;
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
	request_flush_buffers(true);
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

size_t avi_cscd_dumper::emit_frame(const std::vector<unsigned char>& data, bool keyframe, unsigned level)
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
	write32ule(&compression_output[4], l + 2);
	compression_output[8] = (keyframe ? 0x3 : 0x2) | (level << 4);
	compression_output[9] = 8;
	return l + 10;
}

size_t avi_cscd_dumper::emit_sound(size_t samples)
{
	size_t packetsize = 8 + samples * 4;
	size_t towrite = samples * 2;
	if(packetsize + 3 > compression_output.size())
		compression_output.resize(packetsize + 3);
	compression_output[0] = '0';
	compression_output[1] = '1';
	compression_output[2] = 'w';
	compression_output[3] = 'b';
	write32ule(&compression_output[4], packetsize - 8);
	size_t itr = 0;
	umutex_class _frame_mutex(frame_mutex);
	for(size_t i = 0; i < towrite; i++) {
		unsigned short sample = 0;
		if(itr < buffered_sound_samples * 2)
			sample = sound_buffer[itr++];
		write16ule(&compression_output[8 + 2 * i], sample + 32768);
	}
	if(itr < buffered_sound_samples * 2) {
		memmove(&sound_buffer[0], &sound_buffer[itr], sizeof(unsigned short) * (buffered_sound_samples * 2
			- itr));
		buffered_sound_samples -= itr / 2;
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
		f.fps_d, gp_sampling_rate);
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
	bool force_break = f.forcebreak;

	size_t size;
	bool tmp = restart_segment_if_needed(force_break);
	keyframe = keyframe || tmp;
	size = emit_frame(data, keyframe, level);
	emit_frame_stream(size, keyframe);
	size = emit_sound(samples);
	emit_sound_stream(size, samples);
	current_major_segment_frames++;
	frame_buffer.erase(frame_buffer.begin());
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
	unsigned long critical = static_cast<uint64_t>(gp_sampling_rate) * f.fps_d % f.fps_n;
	unsigned long ret = static_cast<uint64_t>(gp_sampling_rate) * f.fps_d / f.fps_n;
	if(static_cast<uint64_t>(frame_period_counter) * critical % f.fps_n < critical)
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
