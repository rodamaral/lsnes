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
		uint64_t C = end_pts - xfade_pts;
		uint64_t L = end_pts - loop_pts;
		uint64_t T = end_pts - start_pts;
		uint64_t LminusC = L - C;
		uint64_t Lminus2C = L - 2 * C;
		if(pcm == past_end)
			return past_end;
		if(pcm < T)
			return pcm + start_pts;
		pcm -= T;
		pcm %= 2 * LminusC;
		//There is first a gap.
		if(pcm < Lminus2C)
			return past_end;
		pcm -= Lminus2C;
		//Then there's a new looping section.
		return pcm + loop_pts;
	}

	uint64_t background_song::pcm_to_pts2(uint64_t pcm)
	{
		//                 |           | 
		//1: PPIIIIIILLLLLLC     LLLLLLC     LLLLLLC      LLLLLLC     L...
		//2:               LLLLLLC     LLLLLLC      LLLLLLC     LLLLLLC...
		uint64_t C = end_pts - xfade_pts;
		uint64_t L = end_pts - loop_pts;
		uint64_t T = end_pts - start_pts;
		uint64_t LminusC = L - C;
		uint64_t Lminus2C = L - 2 * C;
		if(pcm == past_end)
			return past_end;
		//Before crossfade_start, there's nothing.
		if(pcm < xfade_pts - start_pts)
			return past_end;
		pcm -= (xfade_pts - start_pts);
		pcm %= 2 * LminusC;
		//First there is loop region.
		if(pcm < L)
			return pcm + loop_pts;
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

	void background_song::parse_oggopus_header(ogg_page& p, ogg_demuxer& demux)
	{
		struct oggopus_header h = ::parse_oggopus_header(p);
		if(h.map_family != 0)
			(stringfmt() << "Unsupported mapping family " << h.map_family).throwex();
		opus_pregap = h.preskip;
		gain = h.gain;
		while(demux.wants_packet_out())
			demux.discard_packet();
	}

	void background_song::parse_oggopus_tags(ogg_page& p, ogg_demuxer& demux)
	{
		struct oggopus_tags t = ::parse_oggopus_tags(p);
		for(auto i : t.comments) {
			regex_results r = regex("([^=]+)=(.*)", i);
			if(!r)
				continue;
			if(r[1] == "SKY_START_PCM")
				start_pts = parse_value<uint64_t>(r[2]) + opus_pregap;
			else if(r[1] == "SKY_LOOP_PCM")
				loop_pts = parse_value<uint64_t>(r[2]) + opus_pregap;
			else if(r[1] == "SKY_XFADE_PCM")
				xfade_pts = parse_value<uint64_t>(r[2]) + opus_pregap;
			else if(r[1] == "SKY_END_PCM")
				end_pts = parse_value<uint64_t>(r[2]) + opus_pregap;
		}
		while(demux.wants_packet_out())
			demux.discard_packet();
	}

	void background_song::parse_oggopus_datapage(ogg_page& p, ogg_demuxer& demux, bool first)
	{
		while(demux.wants_packet_out()) {
			ogg_packet p;
			demux.packet_out(p);
			std::vector<uint8_t> pkt = p.get_vector();
			uint8_t tcnt = opus_packet_tick_count(&pkt[0], pkt.size());
			if(tcnt > 0)
				packets[packetpos] = pkt;
			packetpos += 120 * tcnt;
			datalen = packetpos;
			if(p.get_last_page()) {
				uint64_t samples = p.get_granulepos() - last_granulepos;
				if(samples > p.get_granulepos())
					samples = 0;
				if(samples > datalen - last_datalen) {
					if(!first)
						messages << "Warning: Granulepos says there are " << samples
							<< " samples, found " << datalen - last_datalen << std::endl;
				} else if(p.get_on_eos_page())
					//On EOS page, clip.
					datalen = last_datalen + samples; 
				else
					messages << "Warning: Granulepos says there are " << samples
						<< " samples, found " << datalen - last_datalen << std::endl;
				last_datalen = datalen;
				last_granulepos = p.get_granulepos();
			}
		}
	}

	background_song::background_song(std::istream& stream)
	{
		ogg_stream_reader_iostreams r(stream);
		r.set_errors_to(messages);
		ogg_page p;
		uint64_t packet_num = 0;
		ogg_demuxer d(messages);
		start_pts = past_end;
		loop_pts = past_end;
		xfade_pts = past_end;
		end_pts = past_end;
		packetpos = 0;
		last_granulepos = 0;
		last_datalen = 0;
		datalen = 0;
		
		while(r.get_page(p)) {
			bool c = d.page_in(p);
			if(!c)
				continue;
			if(packet_num == 0) {
				parse_oggopus_header(p, d);
			} else if(packet_num == 1) {
				parse_oggopus_tags(p, d);
			} else {
				parse_oggopus_datapage(p, d, (packet_num == 2));
			}
			packet_num++;
		}
		if(packet_num < 2)
			throw std::runtime_error("Required header packets missing");
		if(datalen == 0)
			throw std::runtime_error("Empty song");
		if(start_pts == past_end)
			start_pts = opus_pregap;
		if(loop_pts == past_end)
			loop_pts = start_pts;
		if(end_pts == past_end)
			end_pts = datalen;
		if(xfade_pts == past_end)
			xfade_pts = end_pts;
		if(loop_pts >= end_pts || xfade_pts <= (loop_pts + end_pts) / 2 || xfade_pts > end_pts ||
			end_pts > datalen) {
			messages << "Explicit song timestamps bad, using defaults." << std::endl;
			start_pts = opus_pregap;
			loop_pts = start_pts;
			xfade_pts = end_pts;
			end_pts = datalen;
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
		uint64_t cfstart = song->xfade_pts;
		uint64_t cflen = song->end_pts - song->xfade_pts;
		for(; samples > 0; output++, samples--, pcmpos++) {
			if(song->xfade_pts == pts1)
				seek_channel(i2, pts2, song->loop_pts);
			if(song->xfade_pts == pts2)
				seek_channel(i1, pts1, song->loop_pts);
			if(song->end_pts == pts1)
				seek_channel(i1, pts1, past_end);
			if(song->end_pts == pts2)
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
