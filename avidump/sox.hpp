#ifndef _sox__hpp__included__
#define _sox__hpp__included__

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

/**
 * .sox sound dumper.
 */
class sox_dumper
{
public:
/**
 * Create new dumper with specified sound sampling rate and channel count.
 *
 * parameter filename: The name of file to dump to.
 * parameter samplerate: The sampling rate (must be positive)
 * parameter channels: The channel count.
 *
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Error opening .sox file
 */
	sox_dumper(const std::string& filename, double samplerate, uint32_t channels) throw(std::bad_alloc,
		std::runtime_error);

/**
 * Destructor.
 */
	~sox_dumper() throw();

/**
 * Close the dump.
 *
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Error fixing and closing .sox file
 */
	void close() throw(std::bad_alloc, std::runtime_error);

/**
 * Dump a sample
 * 
 * parameters a: Sample channel values.
 */
	template<typename... args>
	void sample(args... a)
	{
		sample2<0>(a...);
	}
private:
	template<size_t o>
	void sample2()
	{
		for(size_t i = o; i < samplebuffer.size(); ++i)
			samplebuffer[i] = 0;
		internal_dump_sample();
	}
	template<size_t o, typename itype, typename... args>
	void sample2(itype v, args... a)
	{
		if(o < samplebuffer.size())
			place_sample(o, v);
		sample2<o + 1>(a...);
	}

	void place_sample(size_t index, int8_t v)
	{
		samplebuffer[index] = static_cast<int32_t>(v) << 24;
	}
	void place_sample(size_t index, uint8_t v)
	{
		place_sample(index, static_cast<uint32_t>(v) << 24);
	}
	void place_sample(size_t index, int16_t v)
	{
		samplebuffer[index] = static_cast<int32_t>(v) << 16;
	}
	void place_sample(size_t index, uint16_t v)
	{
		place_sample(index, static_cast<uint32_t>(v) << 16);
	}
	void place_sample(size_t index, int32_t v)
	{
		samplebuffer[index] = v;
	}
	void place_sample(size_t index, uint32_t v)
	{
		place_sample(index, static_cast<int32_t>(v + 0x80000000U));
	}

	void internal_dump_sample();
	std::vector<char> databuf;
	std::vector<int32_t> samplebuffer;
	std::ofstream sox_file;
	uint64_t samples_dumped;
};

#endif
