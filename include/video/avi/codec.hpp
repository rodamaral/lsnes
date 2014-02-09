#ifndef _avi__avi_codec__hpp__included__
#define _avi__avi_codec__hpp__included__

#include <cstdint>
#include <vector>
#include <cstdlib>
#include <map>
#include "video/avi/structure.hpp"
#include "video/avi/samplequeue.hpp"
#include "video/avi/timer.hpp"

/**
 * AVI packet.
 */
struct avi_packet
{
/**
 * Packet type code. The usual values are:
 * - 0x6264: Uncompressed bitmap
 * - 0x6364: compressed bitmap
 * - 0x6277: Sound data
 */
	uint16_t typecode;
/**
 * Hide from index flag.
 */
	bool hidden;
/**
 * Index flags. Ignored if hidden flag is set.
 */
	uint32_t indexflags;
/**
 * The actual packet payload.
 */
	std::vector<char> payload;
};

/**
 * AVI video codec (compressor).
 */
struct avi_video_codec
{
/**
 * Strf info
 */
	struct format
	{
		format(uint32_t _width, uint32_t _height, uint32_t compression, uint16_t bitcount);
		uint32_t width;
		uint32_t height;
		uint32_t suggested_buffer_size;
		uint32_t max_bytes_per_sec;
		uint16_t planes;
		uint16_t bit_count;
		uint32_t compression;
		uint32_t resolution_x;
		uint32_t resolution_y;
		uint32_t quality;
		uint32_t clr_used;
		uint32_t clr_important;
		std::vector<uint8_t> extra;
	};

	virtual ~avi_video_codec();
/**
 * Reset the codec, giving new state.
 *
 * Parameter width: The width of image.
 * Parameter height: The height of image.
 * Parameter fps_n: fps numerator.
 * Parameter fps_d: fps denominator.
 * Returns: Stream format.
 *
 * Note: The next frame emitted MUST be a keyframe.
 */
	virtual format reset(uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d) = 0;
/**
 * Send in frame of data. ready() must return true.
 *
 * - The buffer can be reused after this call returns.
 * - rshift = 0, gshift = 8, bshift = 16.
 *
 * Parameter data: Video frame data, left to right, top to bottom order.
 * Parameter stride: Stride between rows in pixels.
 */
	virtual void frame(uint32_t* data, uint32_t stride) = 0;
/**
 * Is the codec ready to receive a new frame?
 *
 * Returns: True if new frame can be passed. False if packets have to be extracted.
 */
	virtual bool ready() = 0;
/**
 * Read packet. Before calling, ready() must return false.
 *
 * Returns: The packet.
 */
	virtual avi_packet getpacket() = 0;
/**
 * Send performance counters.
 *
 * Parameter b: Amount of busywaiting by emulator.
 * Parameter w: Amount of workwaiting by dumper.
 */
	virtual void send_performance_counters(uint64_t b, uint64_t w);
};

/**
 * AVI common codec type.
 */
template<typename T>
struct avi_codec_type
{
	avi_codec_type(const char* iname, const char* hname, T* (*instance)());
/**
 * Unregister instance.
 */
	~avi_codec_type();
/**
 * Find instance.
 *
 * Parameter iname: Iname of instance to find.
 */
	static avi_codec_type<T>* find(const std::string& iname);
/**
 * Find next codec type.
 *
 * Parameter type: Type to find. If NULL, find the first one.
 * Returns: The next codec, or NULL if none.
 */
	static avi_codec_type<T>* find_next(avi_codec_type<T>* type);
/**
 * Get iname field of codec.
 */
	std::string get_iname();
/**
 * Get hname field of codec.
 */
	std::string get_hname();
/**
 * Get instance of codec.
 */
	T* get_instance();
private:
	std::string iname;
	std::string hname;
	T* (*instance)();
	static std::map<std::string, avi_codec_type<T>*>& codecs();
};

/**
 * AVI audio codec (compressor).
 */
struct avi_audio_codec
{
/**
 * Strf info.
 */
	struct format
	{
		format(uint16_t tag);
		uint32_t max_bytes_per_sec;
		uint32_t suggested_buffer_size;
		uint16_t format_tag;
		uint32_t average_rate;
		uint16_t alignment;
		uint16_t bitdepth;
		uint32_t quality;
		std::vector<uint8_t> extra;;
	};

	virtual ~avi_audio_codec();
/**
 * Reset the codec, giving new state.
 *
 * Parameter samplerate: The new sampling rate.
 * Parameter channels: Channel count.
 * Returns: Stream format.
 */
	virtual format reset(uint32_t samplerate, uint16_t channels) = 0;
/**
 * Send in samples of data. ready() must return true.
 *
 * - The buffer can be reused after this call returns.
 *
 * Parameter data: Interleaved samples.
 * Parameter samples: Number of samples offered.
 */
	virtual void samples(int16_t* data, size_t samples) = 0;
/**
 * Flush the audio state. Default implementation does nothing.
 */
	virtual void flush();
/**
 * Is the codec ready to receive a new frame?
 *
 * Returns: True if new frame can be passed. False if packets have to be extracted.
 */
	virtual bool ready() = 0;
/**
 * Read packet. Before calling, ready() must return false.
 *
 * Returns: The packet.
 */
	virtual avi_packet getpacket() = 0;
};

typedef avi_codec_type<avi_video_codec> avi_video_codec_type;
typedef avi_codec_type<avi_audio_codec> avi_audio_codec_type;

/**
 * Combine trackid and packet typecode into full type.
 */
uint32_t get_actual_packet_type(uint8_t trackid, uint16_t typecode);

/**
 * AVI output stream.
 */
struct avi_output_stream
{
/**
 * Create new output stream.
 */
	avi_output_stream();
/**
 * Destructor.
 */
	~avi_output_stream();
/**
 * Start new segment. If there is existing segment, it is closed.
 *
 * Parameter out: Output file.
 * Parameter vcodec: The video codec to use.
 * Parameter acodec: The audio codec to use.
 * Parameter width: Width of video.
 * Parameter height: Height of video.
 * Parameter fps_n: Framerate numerator.
 * Parameter fps_d: Framerate denomerator.
 * Parameter samplerate: Audio sampling rate.
 * Parameter channels: Number of audio channels.
 */
	void start(std::ostream& out, avi_video_codec& vcodec, avi_audio_codec& acodec, uint32_t width,
		uint32_t height, uint32_t fps_n, uint32_t fps_d, uint32_t samplerate, uint16_t channels);
/**
 * Write stuff to video codec.
 *
 * Parameter frame: The frame to write. See avi_video_codec::frame() for format.
 * Parameter stride: The stride in pixels.
 */
	void frame(uint32_t* frame, uint32_t stride);
/**
 * Write stuff to audio codec.
 *
 * Parameter samples: The samples to write.
 * Parameter samplecount: Count of samples.
 */
	void samples(int16_t* samples, size_t samplecount);
/**
 * Flush audio codec state in preparation to close the AVI.
 */
	void flushaudio();
/**
 * Get number of samples for the next frame.
 *
 * Returns: The number of samples.
 */
	size_t framesamples();
/**
 * Get size estimate.
 *
 * Returns: The estimated size.
 */
	uint64_t get_size_estimate();
/**
 * Flush frame and associtated samples from queue.
 *
 * Parameter frame: The frame to write.
 * Parameter oframe: The frame to delete if written.
 * Parameter stride: The stride between rows in pixels.
 * Parameter aqueue: The audio queue.
 * Parameter force: Read the frame even if there aren't enough sound samples.
 * Returns: True if frame was read, false otherwise.
 */
	bool readqueue(uint32_t* frame, uint32_t* oframe, uint32_t stride, sample_queue& aqueue, bool force);
/**
 * End a segment.
 */
	void end();
private:
	bool in_segment;
	avi_file_structure avifile;
	avi_video_codec* vcodec;
	avi_audio_codec* acodec;
	uint16_t achans;
	timer video_timer;
	timer audio_timer;
};

#endif
