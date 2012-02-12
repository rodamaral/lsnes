#ifndef _avi__avi_writer__hpp__included__
#define _avi__avi_writer__hpp__included__

#include <string>
#include <fstream>
#include "video/avi/codec.hpp"
#include "samplequeue.hpp"

class avi_writer
{
public:
/**
 * Create new avi writer.
 *
 * Parameter _prefix: The prefix to use.
 * Parameter _vcodec: The video codec.
 * Parameter _acodec: The audio codec.
 */
	avi_writer(const std::string& _prefix, struct avi_video_codec& _vcodec, struct avi_audio_codec& _acodec,
		uint32_t samplerate, uint16_t audiochannels);
/**
 * Destructor.
 */
	~avi_writer();
/**
 * Get the video queue.
 */
	std::deque<frame_object>& video_queue();
/**
 * Get the audio queue.
 */
	sample_queue& audio_queue();
/**
 * Flush the queue.
 */
	void flush();
/**
 * Force close the segment. Impiles forced flush (flush even if no sound samples for it).
 */
	void close();
private:
	void flush(bool force);
	bool closed;
	std::string prefix;
	uint64_t next_segment;
	std::deque<frame_object> vqueue;
	sample_queue aqueue;
	avi_output_stream aviout;
	std::ofstream avifile;
	struct avi_video_codec& vcodec;
	struct avi_audio_codec& acodec;
	uint32_t samplerate;
	uint16_t channels;
	uint32_t curwidth;
	uint32_t curheight;
	uint32_t curfps_n;
	uint32_t curfps_d;
};

#endif
