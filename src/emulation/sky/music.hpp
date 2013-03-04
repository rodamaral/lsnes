#ifndef _skycore__music__hpp__included__
#define _skycore__music__hpp__included__

#include <stdexcept>
#include <cstdint>
#include <vector>
#include <map>
#include "library/ogg.hpp"
#ifdef WITH_OPUS_CODEC
#include "library/opus.hpp"
#endif

//
//
//
//
//
//

namespace sky
{
	struct song_buffer;
	extern const uint64_t past_end;		//Position past the end of song.

	struct background_song
	{
		//Load song from stream.
		background_song(std::istream& stream);
		//The song data.
		std::map<uint64_t, std::vector<uint8_t>> packets;
		//Important points in song.
		uint64_t start_pts;	//PTS for start of song.
		uint64_t loop_pts;	//PTS for start of looping part.
		uint64_t xfade_pts;	//PTS for start of crossfade.
		uint64_t end_pts;	//PTS for end of song.
		//Gain.
		uint16_t gain;
		//Find valid timecode, rounding down.
		uint64_t find_timecode_down(uint64_t pts);
		//Find valid timecode, rounding up. Returns past_end if called with too great pts.
		uint64_t find_timecode_up(uint64_t pts);
		//Translate pcm position to pts for track 1.
		uint64_t pcm_to_pts1(uint64_t pcm);
		//Translate pcm position to pts for track 2.
		uint64_t pcm_to_pts2(uint64_t pcm);
		//Translate gain into gain factor.
		int32_t gain_factor();
	private:
		void parse_oggopus_header(ogg_page& p, ogg_demuxer& d);
		void parse_oggopus_tags(ogg_page& p, ogg_demuxer& d);
		void parse_oggopus_datapage(ogg_page& p, ogg_demuxer& d, bool first);
		uint64_t packetpos;
		uint64_t last_granulepos;
		uint64_t last_datalen;
		uint64_t datalen;
		uint64_t opus_pregap;
	};

	extern background_song* bsong;

	uint8_t opus_packet_tick_count(const uint8_t* packet, size_t packetsize);

#ifdef WITH_OPUS_CODEC
	typedef opus::decoder opus_decoder;
#else
	struct opus_decoder
	{
		size_t decode(const uint8_t* a, size_t b, int16_t* c, size_t d)
		{
			uint8_t t = opus_packet_tick_count(a, b);
			if(!t)
				throw std::runtime_error("Bad packet");
			return 120 * t;
		}
	};
#endif

	struct music_player_int
	{
		music_player_int();
		void decode_packet(const std::vector<uint8_t>& data);
		uint16_t pcmpos;
		uint16_t pcmlen;
		int16_t pcmbuf[11522];
		opus_decoder d;
	};

	struct music_player
	{
		music_player(uint64_t& _pcmpos);
		void set_song(background_song* _song) { song = _song; set_gain(); }
		void rewind() { pcmpos = 0; do_preroll(); }
		void do_preroll();
		void decode(std::pair<int16_t, int16_t>* output, size_t samples);
	private:
		void set_gain();
		void seek_channel(music_player_int& i, uint64_t& spts, uint64_t pts);
		bool builtin_gain;
		uint64_t& pcmpos;
		music_player_int i1;
		music_player_int i2;
		background_song* song;
	};
}

#endif
