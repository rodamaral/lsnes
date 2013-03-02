#include "music.hpp"
#include <cstring>
#include <cmath>
#include "library/minmax.hpp"
#include "library/ogg.hpp"
#include "library/string.hpp"
#include "core/window.hpp"

namespace sky
{
	const uint64_t past_end = 0xFFFFFFFFFFFFFFFFULL;

	background_song* bsong = NULL;

	uint8_t ticks_per_frame[32] = {
		4, 8, 16, 24,
		4, 8, 16, 24,
		4, 8, 16, 24,
		4, 8, 4, 8,
		1, 2, 4, 8,
		1, 2, 4, 8,
		1, 2, 4, 8,
		1, 2, 4, 8
	};

	uint8_t opus_packet_tick_count(const uint8_t* packet, size_t packetsize)
	{
		if(packetsize < 1)
			return 0;
		uint8_t x = ticks_per_frame[packet[0] >> 3];
		uint8_t y = (packetsize < 2) ? 255 : (packet[1] & 0x3F);
		uint16_t z = (uint16_t)x * y;
		switch(packet[0] & 3) {
		case 0:		return x;
		case 1:		return x << 1;
		case 2:		return x << 1;
		case 3:		return (z <= 48) ? z : 0;
		};
	}

	uint64_t background_song::find_timecode_down(uint64_t pts)
	{
		if(pts == past_end)
			return past_end;
		auto i = packets.upper_bound(pts);
		if(i == packets.end())
			return packets.rbegin()->first;
		else {
			i--;
			return i->first;
		}
	}

	uint64_t background_song::find_timecode_up(uint64_t pts)
	{
		if(pts == past_end)
			return past_end;
		auto i = packets.lower_bound(pts);
		if(i == packets.end())
			return past_end;
		else
			return i->first;
		
	}

	uint64_t background_song::pcm_to_pts1(uint64_t pcm)
	{
		//                 |           | 
		//1: PPIIIIIILLLLLLC     LLLLLLC     LLLLLLC      LLLLLLC     L...
		//2:               LLLLLLC     LLLLLLC      LLLLLLC     LLLLLLC...
		uint64_t C = total - crossfade_start;
		uint64_t L = total - loop_start;
		uint64_t LminusC = L - C;
		uint64_t Lminus2C = L - 2 * C;

		if(pcm == past_end)
			return past_end;
		pcm += pregap;
		if(pcm < total)
			return pcm;
		pcm -= total;
		pcm %= 2 * LminusC;
		//There is first a gap.
		if(pcm < Lminus2C)
			return past_end;
		pcm -= Lminus2C;
		//Then there's a new looping section.
		return pcm + loop_start;
	}

	uint64_t background_song::pcm_to_pts2(uint64_t pcm)
	{
		//                 |           | 
		//1: PPIIIIIILLLLLLC     LLLLLLC     LLLLLLC      LLLLLLC     L...
		//2:               LLLLLLC     LLLLLLC      LLLLLLC     LLLLLLC...
		uint64_t C = total - crossfade_start;
		uint64_t L = total - loop_start;
		uint64_t LminusC = L - C;
		uint64_t Lminus2C = L - 2 * C;
		if(pcm == past_end)
			return past_end;
		pcm += pregap;
		//Before crossfade_start, there's nothing.
		if(pcm < crossfade_start)
			return past_end;
		pcm -= crossfade_start;
		pcm %= 2 * LminusC;
		//First there is loop region.
		if(pcm < L)
			return pcm + loop_start;
		//Then there's a gap.
		return past_end;
	}

	int32_t background_song::gain_factor()
	{
		if(!gain)
			return 256;
		double g = pow(10, gain / 20.0);
		if(g >= 256)
			return 65536;
		return 256 * g;
	}

	background_song::background_song(std::istream& stream)
	{
		ogg_stream_reader_iostreams r(stream);
		r.set_errors_to(messages);
		ogg_page p;
		uint64_t pnum = 0;
		bool loop_spec = false, cf_spec = false;
		loop_start = 0;
		crossfade_start = 0;
		std::vector<uint8_t> pending_data;
		uint64_t last_gpos = ogg_page::granulepos_none;
		uint64_t rpos = 0;
		uint64_t old_rpos = 0;
		while(r.get_page(p)) {
			if(pnum == 0) {
				//Header page.
				struct oggopus_header h = parse_oggopus_header(p);
				if(h.map_family != 0)
					(stringfmt() << "Unsupported mapping family " << h.map_family).throwex();
				pregap = h.preskip;
				gain = h.gain;
			} else if(pnum == 1) {
				//Tags page.
				struct oggopus_tags t = parse_oggopus_tags(p);
				for(auto i : t.comments) {
					regex_results r = regex("([^=]+)=(.*)", i);
					if(!r)
						continue;
					if(r[1] == "LSNES_LOOP_START") {
						loop_start = parse_value<uint64_t>(r[2]) + pregap;
						loop_spec = true;
					} else if(r[1] == "LSNES_XFADE_START") {
						crossfade_start = parse_value<uint64_t>(r[2]) + pregap;
						cf_spec = true;
					}
				}
			} else {
				//Data page.
				uint64_t gpos = p.get_granulepos();
				uint8_t pkts = p.get_packet_count();
				bool e = p.get_eos();
				bool c = p.get_continue();
				bool i = p.get_last_packet_incomplete();
				for(unsigned j = 0; j < pkts; j++) {
					if(i > 0 || !c)
						pending_data.clear();
					size_t b = pending_data.size();
					auto pkt = p.get_packet(j);
					pending_data.resize(b + pkt.second);
					memcpy(&pending_data[b], pkt.first, pkt.second);
					if(i && j == pkts - 1)
						break;	//Next page.
					//Pending_data is now opus packet.
					uint8_t tcnt = opus_packet_tick_count(&pending_data[0], pending_data.size());
					if(tcnt > 0)
						packets[rpos] = pending_data;
					rpos += 120 * tcnt;
					total = rpos;
				}
				if(e) {
					uint64_t pscnt = gpos - ((last_gpos == ogg_page::granulepos_none) ? 0 :
						last_gpos);
					total = old_rpos + pscnt;
					if(total > rpos)
						total = rpos;
					break;
				}
				if(gpos != ogg_page::granulepos_none) {
					last_gpos = gpos;
					old_rpos = rpos;
				}
			}
			pnum++;
		}
		
		if(!cf_spec)
			crossfade_start = total;
		if(!loop_spec)
			loop_start = pregap;
		if(loop_start >= total) {
			messages << "Bad loop point, assuming start of song" << std::endl;
			loop_start = pregap;
		}
		if(crossfade_start - loop_start < total - crossfade_start) {
			messages << "Bad XFADE point, assuming end of song." << std::endl;
			crossfade_start = total;
		}
	}

	music_player_int::music_player_int()
#ifdef WITH_OPUS_CODEC
		: d(opus::samplerate::r48k, true)
#endif
	{
	}

	void music_player_int::decode_packet(const std::vector<uint8_t>& data)
	{
		pcmlen = 120 * opus_packet_tick_count(&data[0], data.size());
		pcmpos = 0;
		memset(&pcmbuf[0], 0, 11520 * sizeof(int16_t));
		try {
			d.decode(&data[0], data.size(), &pcmbuf[0], 5760);
		} catch(std::exception& e) {
			messages << "Music: Failed to decode opus packet: " << e.what() << std::endl;
		}
	}

	void music_player::seek_channel(music_player_int& i, uint64_t& spts, uint64_t pts)
	{
		if(pts == past_end) {
			i.pcmpos = i.pcmlen = 0;
			spts = past_end;
			return;
		}
#ifdef WITH_OPUS_CODEC
		i.d.ctl(opus::reset);
#endif
		uint64_t ptsr = song->find_timecode_down((pts >= 3840) ? (pts - 3840) : 0);
		while(ptsr < pts) {
			ptsr = song->find_timecode_up(ptsr);
			if(ptsr == past_end)
				break;
			i.decode_packet(song->packets[ptsr]);
			if(ptsr + i.pcmlen > pts) {
				i.pcmpos = pts - ptsr;
				ptsr = pts;
			} else
				ptsr += i.pcmlen;
		}
		spts = pts;
	}

	void music_player::do_preroll()
	{
		if(!song || song->packets.empty())
			return;
		uint64_t pts1 = song->pcm_to_pts1(pcmpos);
		uint64_t pts2 = song->pcm_to_pts2(pcmpos);
		uint64_t pts1p;
		uint64_t pts2p;
		seek_channel(i1, pts1p, pts1);
		seek_channel(i2, pts2p, pts2);
	}

	void music_player::decode(std::pair<int16_t, int16_t>* output, size_t samples)
	{
		if(!song) {
			memset(output, 0, samples * sizeof(std::pair<int16_t, int16_t>));
			pcmpos += samples;
			return;
		}
		int32_t gfactor = builtin_gain ? 256 : song->gain_factor();
		uint64_t pts1 = song->pcm_to_pts1(pcmpos);
		uint64_t pts2 = song->pcm_to_pts2(pcmpos);
		uint64_t cfstart = song->crossfade_start;
		uint64_t cflen = song->total - song->crossfade_start;
		for(; samples > 0; output++, samples--, pcmpos++) {
			if(song->crossfade_start == pts1)
				seek_channel(i2, pts2, song->loop_start);
			if(song->crossfade_start == pts2)
				seek_channel(i1, pts1, song->loop_start);
			if(song->total == pts1)
				seek_channel(i1, pts1, past_end);
			if(song->total == pts2)
				seek_channel(i2, pts2, past_end);
			if(i1.pcmpos == i1.pcmlen && pts1 != past_end) {
				uint64_t pts = song->find_timecode_up(pts1);
				if(pts != past_end)
					i1.decode_packet(song->packets[pts]);
			}
			if(i2.pcmpos == i2.pcmlen && pts2 != past_end) {
				uint64_t pts = song->find_timecode_up(pts2);
				if(pts != past_end)
					i2.decode_packet(song->packets[pts]);
			}
			uint32_t cf = 0, icf = 0;
			if(i1.pcmpos < i1.pcmlen)
				cf = (pts1 > cfstart) ? (256 - 256 * (pts1 - cfstart) / cflen) : 256;
			if(i2.pcmpos < i2.pcmlen)
				icf = (pts2 > cfstart) ? (256 - 256 * (pts2 - cfstart) / cflen) : 256;
			int32_t l = (cf * i1.pcmbuf[2 * i1.pcmpos + 0] + icf * i2.pcmbuf[2 * i2.pcmpos + 0]) >> 8;
			int32_t r = (cf * i1.pcmbuf[2 * i1.pcmpos + 1] + icf * i2.pcmbuf[2 * i2.pcmpos + 1]) >> 8;
			output->first = max(min((gfactor * l) >> 8, 32767), -32768);
			output->second = max(min((gfactor * r) >> 8, 32767), -32768);
			if(i1.pcmpos < i1.pcmlen)
				i1.pcmpos++;
			if(i2.pcmpos < i2.pcmlen)
				i2.pcmpos++;
			if(pts1 != past_end)
				pts1++;
			if(pts2 != past_end)
				pts2++;			
		}
	}

	void music_player::set_gain()
	{
		if(!song)
			return;
		try {
#ifdef WITH_OPUS_CODEC
			i1.d.ctl(opus::gain(song->gain));
			i2.d.ctl(opus::gain(song->gain));
#endif
			builtin_gain = true;
		} catch(...) {
			builtin_gain = false;
		}
	}

	music_player::music_player(uint64_t& pcm)
		: pcmpos(pcm)
	{
		song = NULL;
	}
}

