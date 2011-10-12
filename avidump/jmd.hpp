#ifndef _jmd__hpp__included__
#define _jmd__hpp__included__

#include <cstdint>
#include <string>
#include <fstream>
#include <deque>
#include <list>
#include <vector>

class jmd_dumper
{
public:
	jmd_dumper(const std::string& filename, unsigned level);
	~jmd_dumper();
	void video(uint64_t ts, uint16_t* memory, uint32_t width, uint32_t height);
	void audio(uint64_t ts, short l, short r);
	void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
		authors, double gametime, const std::string& rerecords);
	void end(uint64_t ts);
private:
	struct frame_buffer
	{
		uint64_t ts;
		std::vector<char> data;
	};
	struct sample_buffer
	{
		uint64_t ts;
		short l;
		short r;
	};

	std::deque<frame_buffer> frames;
	std::deque<sample_buffer> samples;

	std::vector<char> compress_frame(uint16_t* memory, uint32_t width, uint32_t height);
	void flush_buffers(bool force);
	void flush_frame(frame_buffer& f);
	void flush_sample(sample_buffer& s);

	std::ofstream jmd;
	uint64_t last_written_ts;
	unsigned clevel;
};

#endif
