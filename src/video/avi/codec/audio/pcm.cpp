#include "video/avi/codec.hpp"
#include "library/serialization.hpp"

namespace
{
	struct avi_codec_pcm : public avi_audio_codec
	{
		~avi_codec_pcm();
		avi_audio_codec::format reset(uint32_t samplerate, uint16_t channels);
		void samples(int16_t* data, size_t samples);
		bool ready();
		avi_packet getpacket();
	private:
		uint16_t chans;
		avi_packet out;
		bool ready_flag;
	};

	avi_codec_pcm::~avi_codec_pcm()
	{
	}

	avi_audio_codec::format avi_codec_pcm::reset(uint32_t samplerate, uint16_t channels)
	{
		chans = channels;
		ready_flag = true;
		avi_audio_codec::format fmt(1);		//1 => PCM.
		fmt.max_bytes_per_sec = fmt.average_rate = samplerate * channels * 2;
		fmt.alignment = channels * 2;
		fmt.bitdepth = 16;
		return fmt;
	}

	void avi_codec_pcm::samples(int16_t* data, size_t samples)
	{
		out.payload.resize(2 * chans * samples);
		for(size_t i = 0; i < chans * samples; i++)
			serialization::s16l(&out.payload[2 * i], data[i]);
		out.typecode = 0x6277;
		out.indexflags = 0x10;
		out.hidden = false;
		ready_flag = false;
	}

	bool avi_codec_pcm::ready()
	{
		return ready_flag;
	}

	avi_packet avi_codec_pcm::getpacket()
	{
		ready_flag = true;
		return out;
	}

	avi_audio_codec_type pcm("pcm", "PCM audio", []() -> avi_audio_codec* { return new avi_codec_pcm;});
}
