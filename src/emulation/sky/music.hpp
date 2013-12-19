#ifndef _skycore__music__hpp__included__
#define _skycore__music__hpp__included__

#include <stdexcept>
#include <cstdint>
#include <vector>
#include <map>
#include <set>
#include "random.hpp"
#include "library/ogg.hpp"
#include "library/opus.hpp"

namespace sky
{
	struct song_buffer;
	extern const uint64_t past_end;		//Position past the end of song.

	struct multistream_characteristics
	{
		multistream_characteristics();
		uint8_t channels;
		uint8_t streams;
		uint8_t coupled;
		uint8_t mapping[255];
		float downmix_l[255];
		float downmix_r[255];
		int16_t gain;
	};

	struct subsong_transition
	{
		subsong_transition();
		void fixup(uint64_t pregap, uint64_t datalen, const std::string& ssname);
		//Starting PTS.
		uint64_t start_pts;
		//Crossfade start PTS.
		uint64_t xfade_pts;
		//Ending pts.
		uint64_t end_pts;
		//Set of next subsongs.
		std::set<uint32_t> next_subsongs;
	};

	struct song_buffer
	{
		//Load song from stream.
		song_buffer(std::istream& stream);
		//Access a packet.
		const std::vector<uint8_t>& get_packet(uint32_t subsong, uint64_t pts);
		//Get packet pts at or after given pts.
		uint64_t next_timecode(uint32_t subsong, uint64_t pts);
		//Get packet pts at or before given pts.
		uint64_t prev_timecode(uint32_t subsong, uint64_t pts);
		//Fill structure with multistream characteristics of a subsong.
		void fill_ms_characteristics(uint32_t subsong, struct multistream_characteristics& c);
		//Get set of subsongs with entry flag set.
		const std::set<uint32_t>& entrypoints() { return entry; }
		//Get transition info for a subsong.
		const struct subsong_transition& transitions(uint32_t subsong);
	private:
		struct subsong_context
		{
			//Create a new subsong context.
			//Note: Does not initialize psid.
			subsong_context();
			//Demuxer.
			ogg::demuxer demux;
			//Stream ID.
			uint32_t psid;
			//Ogg Stream ID.
			uint32_t oggid;
			//Last granule position recorded.
			uint64_t last_granule;
			//PTS at last granule position recorded.
			uint64_t last_pts;
			//Current PTS.
			uint64_t pts;
			//Number of pages seen.
			uint64_t pages;
			//Pregap.
			uint32_t pregap;
			//Gain
			uint16_t gain;
			//Seen EOS flag.
			bool eos_seen;
		};
		bool page_starts_new_stream(ogg::page& p);
		bool parse_ogg_page(ogg::page& p, subsong_context& ctx);
		void parse_ogg_header(ogg::packet& p, subsong_context& ctx);
		void parse_ogg_tags(ogg::packet& p, subsong_context& ctx, const ogg::page& debug);
		void parse_ogg_data(ogg::packet& p, subsong_context& ctx, const ogg::page& debug);
		//Register a LSID.
		uint32_t register_lsid(const std::string& ssid);
		uint32_t register_lsid(const std::string& ssid, uint32_t psid);
		void delete_stream(uint32_t psid);
		void delete_undefined_substreams();
		std::string reverse_lsid(uint32_t lsid);
		//Mappings between ssid, lsid and psid.
		std::map<std::string, uint32_t> ssid_to_lsid;
		std::map<uint32_t, uint32_t> lsid_to_psid;
		//Next LSID to allocate.
		uint32_t next_lsid;
		//LSIDs valid for entry.
		std::set<uint32_t> entry;
		//Multistream characteristics by PSID.
		std::map<uint32_t, multistream_characteristics> mscharacteristics;
		//Subsong transitions by LSID.
		std::map<uint32_t, subsong_transition> stransitions;
		//Packet data, indexed by (PSID,PTS).
		std::map<std::pair<uint32_t, uint64_t>, std::vector<uint8_t>> packetdata;
		//Dummy data.
		subsong_transition dummy_transition;
		std::vector<uint8_t> dummy_packet;
	};

	struct packet_decoder
	{
		//Create a new packet decoder.
		packet_decoder();
		//Set multistream characteristics of packet decoder.
		void set_multistream(const struct multistream_characteristics& c);
		//Decode a packet, setting buffers.
		void decode_packet(const std::vector<uint8_t>& data);
		//Reset the opus codec.
		void reset();
		uint16_t pcmpos;
		uint16_t pcmlen;
		int16_t pcmbuf[11522];
	private:
		opus::multistream_decoder* d;
		uint8_t channels;
		float downmix_l[255];
		float downmix_r[255];
		std::vector<uint8_t> memory;
		float* dmem;
	};

	struct music_player_memory
	{
		uint64_t pcmpos1;
		uint64_t pcmpos2;
		uint32_t subsong1;
		uint32_t subsong2;
	};

	struct music_player
	{
		music_player(struct music_player_memory& m, random& _rng);
		void set_song(song_buffer* _song) { song = _song; do_preroll(); }
		void rewind() { song_to_beginning(); do_preroll(); }
		void do_preroll();
		void decode(std::pair<int16_t, int16_t>* output, size_t samples);
	private:
		void song_to_beginning();
		void seek_channel(packet_decoder& i, uint64_t& spts, uint32_t subsong, uint64_t pts);
		music_player_memory& mem;
		packet_decoder i1;
		packet_decoder i2;
		song_buffer* song;
		random& rng;
	};
}

#endif
