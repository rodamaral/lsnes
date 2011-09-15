#ifndef _avidump__hpp__included__
#define _avidump__hpp__included__

#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <stdexcept>


#ifndef NO_THREADS
#include <thread>
#include <condition_variable>
#include <mutex>

/**
 * Class of thread.
 */
typedef std::thread thread_class;

/**
 * Class of condition variables.
 */
typedef std::condition_variable cv_class;

/**
 * Class of mutexes.
 */
typedef std::mutex mutex_class;

/**
 * Class of unique mutexes (for condition variable waiting).
 */
typedef std::unique_lock<std::mutex> umutex_class;

#else

/**
 * Class of thread.
 */
struct thread_class
{
/**
 * Does nothing.
 */
	template<typename T, typename... args>
	thread_class(T obj, args... a) {}
/**
 * Does nothing.
 */
	void join() {}
};

/**
 * Class of mutexes.
 */
typedef struct mutex_class
{
/**
 * Does nothing.
 */
	void lock() {}
/**
 * Does nothing.
 */
	void unlock() {}
} umutex_class;

/**
 * Class of condition variables.
 */
struct cv_class
{
/**
 * Does nothing.
 */
	void wait(umutex_class& m) {}
/**
 * Does nothing.
 */
	void notify_all() {}
};

#endif

/**
 * Size of audio buffer (enough to buffer 3 frames).
 */
#define AVIDUMPER_AUDIO_BUFFER 4096

/**
 * Information about frame in AVI.
 */
struct avi_frame
{
/**
 * Constructor.
 *
 * parameter _flags: Flags for frame.
 * parameter _type: AVI type for frame (big-endian!).
 * parameter _offset: Offset of frame from start of MOVI.
 * parameter _size: Size of frame data.
 */
	avi_frame(uint32_t _flags, uint32_t _type, uint32_t _offset, uint32_t _size);

/**
 * Write the index entry for frame.
 *
 * parameter buf: Buffer to write to.
 */
	void write(uint8_t* buf);

/**
 * Flags.
 */
	uint32_t flags;

/**
 * Chunk type.
 */
	uint32_t type;

/**
 * Chunk offset.
 */
	uint32_t offset;

/**
 * Chunk size.
 */
	uint32_t size;
};

/**
 * Parameters for AVI dumping.
 */
struct avi_info
{
/**
 * Zlib compression level (0-9).
 */
	unsigned compression_level;

/**
 * Audio drop counter increments by this much every frame.
 */
	uint64_t audio_drop_counter_inc;

/**
 * Audio drop counter modulus (when audio drop counter warps around, sample is dropped).
 */
	uint64_t audio_drop_counter_max;

/**
 * Audio sampling rate to write to AVI.
 */
	uint32_t audio_sampling_rate;

/**
 * Native audio sampling rate to write to auxillary SOX file.
 */
	double audio_native_sampling_rate;

/**
 * Interval of keyframes (WARNING: >1 gives non-keyframes which AVISource() doesn't like).
 */
	uint32_t keyframe_interval;
};

/**
 * The actual AVI dumper.
 */
class avidumper
{
public:
	avidumper(const std::string& _prefix, struct avi_info parameters);
	~avidumper() throw();

/**
 * Waits for the encode thread. Not needed: Operations that need to synchronize synchronize themselves.
 */
	void wait_idle() throw();

/**
 * Dump a frame (new segment starts if needed). Pixel byte order is BGRx.
 *
 * parameter data: The frame data.
 * parameter width: Width of frame.
 * parameter height: Height of frame.
 * parameter fps_n: Numerator of fps value.
 * parameter fps_d: Denomerator of fps value.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping frame.
 */
	void on_frame(const uint32_t* data, uint16_t width, uint16_t height, uint32_t fps_n, uint32_t fps_d)
		throw(std::bad_alloc, std::runtime_error);

/**
 * Dump an audio sample
 *
 * parameter left: Signed sample for left channel (-32768 - 327678).
 * parameter right: Signed sample for right channel (-32768 - 327678).
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping sample.
 */
	void on_sample(short left, short right) throw(std::bad_alloc, std::runtime_error);

/**
 * Notify end of dump.
 *
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error closing dump.
 */
void on_end() throw(std::bad_alloc, std::runtime_error);

/**
 * Causes current thread to become encode thread. Do not call this, the code internally uses it.
 *
 * returns: Return status for the thread.
 */
	int encode_thread();

/**
 * Set capture errored flag.
 *
 * parameter err: The error message.
 */
	void set_capture_error(const char* err) throw();
private:
	void print_summary(std::ostream& str);
	void on_frame_threaded(const uint32_t* data, uint16_t width, uint16_t height, uint32_t fps_n, uint32_t fps_d)
		throw(std::bad_alloc, std::runtime_error);
	void flush_audio_to(unsigned commit_ptr);
	void open_and_write_avi_header(uint16_t width, uint16_t height, uint32_t fps_n, uint32_t fps_d);
	void fixup_avi_header_and_close();
	std::ofstream sox_stream;
	std::ofstream avi_stream;
	bool capture_error;
	std::string capture_error_str;
	bool sox_open;
	bool avi_open;
	//Global settings.
	unsigned compression_level;
	uint64_t audio_drop_counter_inc;
	uint64_t audio_drop_counter_max;
	uint32_t audio_sampling_rate;
	double audio_native_sampling_rate;
	uint32_t keyframe_interval;
	//Previous frame.
	uint16_t pwidth;
	uint16_t pheight;
	uint32_t pfps_n;
	uint32_t pfps_d;
	//Current segment.
	uint32_t segment_movi_ptr;
	uint32_t segment_frames;
	uint32_t segment_samples;
	uint32_t segment_last_keyframe;
	uint32_t current_segment;
	uint32_t fixup_avi_size;
	uint32_t fixup_avi_frames;
	uint32_t fixup_avi_length;
	uint32_t fixup_avi_a_length;
	uint32_t fixup_movi_size;
	std::list<avi_frame> segment_chunks;
	//Global info.
	std::string prefix;
	uint64_t total_data;
	uint64_t total_frames;
	uint64_t total_samples;
	uint64_t raw_samples;
	uint64_t audio_drop_counter;
	//Temporary buffers.
	std::vector<uint8_t> pframe;
	std::vector<uint8_t> tframe;
	std::vector<uint8_t> cframe;
	short audio_buffer[AVIDUMPER_AUDIO_BUFFER];
	unsigned audio_put_ptr;
	unsigned audio_get_ptr;
	volatile unsigned audio_commit_ptr;	//Protected by frame_mutex.
	//Multithreading stuff.
	thread_class* frame_thread;
	cv_class frame_cond;
	mutex_class frame_mutex;
	const uint32_t* mt_data;
	volatile uint16_t mt_width;
	volatile uint16_t mt_height;
	volatile uint32_t mt_fps_n;
	volatile uint32_t mt_fps_d;
	volatile bool sigquit;
};

#endif
