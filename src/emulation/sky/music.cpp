#include "music.hpp"
#include <cstring>
#include <iomanip>
#include <cmath>
#include "library/hex.hpp"
#include "library/minmax.hpp"
#include "library/ogg.hpp"
#include "library/opus-ogg.hpp"
#include "library/string.hpp"
#include "core/messages.hpp"
#include "state.hpp"

namespace sky
{
	struct downmix_pair
	{
		float l;
		float r;
	};

	struct song_buffer;
	const uint64_t past_end = 0xFFFFFFFFFFFFFFFFULL;

	//These values are taken from libopusfile.
	const downmix_pair downmix[] = {
		{16384,16384},
		{16384,0},{0,16384},
		{9598,0},{6786,6786},{0,9598},
		{6924,0},{0,6924}, {5996,3464}, {3464,5996},
		{10666,0},{7537,7537},{0,10666},{9234,5331},{5331,9234},
		{8668,0},{6129,6129},{0,8668},{7507,4335},{4335,7507},{6129,6129},
		{7459,0},{5275,5275},{0,7459},{6460,3731},{3731,6460},{4568,4568},{5275,5275},
		{6368,0},{4502,4502},{0,6368},{5515,3183},{3183,5515},{5515,3183},{3183,5515},{4502,4502}
	};

	const size_t downmix_idx[9] = {0, 0, 1, 3, 6, 10, 15, 21, 28};

	uint32_t pick_subsong(random& rng, const std::set<uint32_t>& candidates)
	{
		if(candidates.empty())
			return 0;
		if(candidates.size() == 1)
			return *candidates.begin();
		uint32_t r = rng.pull();
		r %= candidates.size();
		for(auto i : candidates)
			if(!(r--))
				return i;
		return 0;
	}

	void fill_msc_downmix_family0(struct multistream_characteristics& c, unsigned chan)
	{
		if(chan < 1 || chan > 2)
			(stringfmt() << "Illegal channel count " << chan << " for map family 0").throwex();
		for(unsigned i = 0; i < chan; i++) {
			c.downmix_l[i] = downmix[i + downmix_idx[chan]].l;
			c.downmix_r[i] = downmix[i + downmix_idx[chan]].r;
		}
	}

	void fill_msc_downmix_family1(struct multistream_characteristics& c, unsigned chan)
	{
		if(chan < 1 || chan > 8)
			(stringfmt() << "Illegal channel count " << chan << " for map family 1").throwex();
		for(unsigned i = 0; i < chan; i++) {
			c.downmix_l[i] = downmix[i + downmix_idx[chan]].l;
			c.downmix_r[i] = downmix[i + downmix_idx[chan]].r;
		}
	}

	void fill_msc_from_header(struct multistream_characteristics& c, const opus::ogg_header& h)
	{
		c.channels = h.channels;
		c.gain = h.gain;
		c.streams = h.streams;
		c.coupled = h.coupled;
		memcpy(c.mapping, h.chanmap, 255);
		if(h.map_family == 0)
			fill_msc_downmix_family0(c, c.channels);
		else if(h.map_family == 1)
			fill_msc_downmix_family1(c, c.channels);
		else
			(stringfmt() << "Unsupported map family " << (int)h.map_family).throwex();
	}

	multistream_characteristics::multistream_characteristics()
	{
		channels = 1;
		gain = 0;
		streams = 1;
		coupled = 0;
		mapping[0] = 0;
		fill_msc_downmix_family0(*this, 1);
	}

	subsong_transition::subsong_transition()
	{
		start_pts = past_end;
		xfade_pts = past_end;
		end_pts = past_end;
	}

	void subsong_transition::fixup(uint64_t pregap, uint64_t datalen, const std::string& ssname)
	{
		if(start_pts == past_end)
			start_pts = pregap;
		if(end_pts == past_end)
			end_pts = datalen;
		if(xfade_pts == past_end)
			xfade_pts = end_pts;
		if(end_pts < start_pts || end_pts > datalen) {
			messages << "Warning: [" << ssname << "] Ending PTS out of range, clipped to the end."
				<< std::endl;
			end_pts = datalen;
		}
		if(xfade_pts < start_pts || xfade_pts > end_pts) {
			messages << "Warning: [" << ssname << "] Fade PTS out of range, clipped to the end."
				<< std::endl;
			xfade_pts = datalen;
		}
		if(start_pts >= end_pts) {
			messages << "Warning: [" << ssname << "] Start PTS out of range, clipped to the start."
				<< std::endl;
			xfade_pts = pregap;
		}
	}

	uint32_t song_buffer::register_lsid(const std::string& ssid)
	{
		if(ssid_to_lsid.count(ssid))
			return ssid_to_lsid[ssid];
		auto dummy = stransitions[next_lsid];
		ssid_to_lsid[ssid] = next_lsid++;
		return ssid_to_lsid[ssid];
	}

	uint32_t song_buffer::register_lsid(const std::string& ssid, uint32_t psid)
	{
		if(ssid_to_lsid.count(ssid)) {
			lsid_to_psid[ssid_to_lsid[ssid]] = psid;
			return ssid_to_lsid[ssid];
		}
		auto dummy = stransitions[next_lsid];
		lsid_to_psid[next_lsid] = psid;
		ssid_to_lsid[ssid] = next_lsid++;
		return ssid_to_lsid[ssid];
	}

	void song_buffer::delete_undefined_substreams()
	{
		bool deleted = false;
		do {
			deleted = false;
			for(auto& i : ssid_to_lsid) {
				uint32_t lsid = i.second;
				if(!lsid_to_psid.count(lsid)) {
					//This needs to be deleted.
					stransitions.erase(lsid);
					ssid_to_lsid.erase(i.first);
					deleted = true;
					break;
				}
			}
		} while(deleted);
	}

	void song_buffer::delete_stream(uint32_t psid)
	{
		bool deleted = false;
		do {
			deleted = false;
			for(auto& i : ssid_to_lsid) {
				uint32_t lsid = i.second;
				if(lsid_to_psid.count(lsid) && lsid_to_psid[lsid] == psid) {
					//This needs to be deleted.
					stransitions.erase(lsid);
					lsid_to_psid.erase(lsid);
					ssid_to_lsid.erase(i.first);
					deleted = true;
					break;
				}
			}
		} while(deleted);
		mscharacteristics.erase(psid);
		auto key1 = std::make_pair(psid, 0ULL);
		auto key2 = std::make_pair(psid + 1, 0ULL);
		std::map<std::pair<uint32_t, uint64_t>, std::vector<uint8_t>>::iterator itr1;
		while((itr1 = packetdata.lower_bound(key1)) != packetdata.lower_bound(key2))
			packetdata.erase(itr1);
	}

	std::string song_buffer::reverse_lsid(uint32_t lsid)
	{
		for(auto& i : ssid_to_lsid)
			if(i.second == lsid)
				return i.first;
		return "";
	}

	const std::vector<uint8_t>& song_buffer::get_packet(uint32_t subsong, uint64_t pts)
	{
		if(!lsid_to_psid.count(subsong))
			return dummy_packet;
		std::pair<uint32_t, uint64_t> ptsx = std::make_pair(lsid_to_psid[subsong], pts);
		if(!packetdata.count(ptsx))
			return dummy_packet;
		return packetdata[ptsx];
	}

	uint64_t song_buffer::next_timecode(uint32_t subsong, uint64_t pts)
	{
		if(!lsid_to_psid.count(subsong))
			return past_end;
		std::pair<uint32_t, uint64_t> ptsx = std::make_pair(lsid_to_psid[subsong], pts);
		if(ptsx.second == past_end || packetdata.empty())
			return past_end;
		auto i = packetdata.lower_bound(ptsx);
		if(i == packetdata.end())
				return past_end;
		else {
			uint32_t psid = i->first.first;
			if(psid == lsid_to_psid[subsong])
				return i->first.second;
			else
				return past_end;
		}
	}

	uint64_t song_buffer::prev_timecode(uint32_t subsong, uint64_t pts)
	{
		if(!lsid_to_psid.count(subsong))
			return past_end;
		std::pair<uint32_t, uint64_t> ptsx = std::make_pair(lsid_to_psid[subsong], pts);
		if(ptsx.second == past_end || packetdata.empty())
			return past_end;
		auto i = packetdata.upper_bound(ptsx);
		if(i == packetdata.end()) {
			uint32_t psid = packetdata.rbegin()->first.first;
			if(psid == lsid_to_psid[subsong])
				return packetdata.rbegin()->first.second;
			else
				return past_end;
		} else {
			i--;
			uint32_t psid = i->first.first;
			if(psid == lsid_to_psid[subsong])
				return i->first.second;
			else
				return past_end;
		}
	}

	song_buffer::song_buffer(std::istream& stream)
	{
		next_lsid = 0;
		ogg::stream_reader_iostreams r(stream);
		r.set_errors_to(messages);
		ogg::page p;
		std::map<uint32_t, subsong_context> psids;
		uint32_t next_psid = 0;
		while(r.get_page(p))
			if(page_starts_new_stream(p)) {
				uint32_t psid = next_psid++;
				psids[psid].psid = psid;
				psids[psid].oggid = p.get_stream();
				parse_ogg_page(p, psids[psid]);
			} else
				for(auto& i : psids)
					if(parse_ogg_page(p, i.second) && p.get_eos()) {
						if(i.second.pts <= i.second.pregap) {
							//Invalid or blank stream.
							messages << "Warning: " << p.stream_debug_id() << " has "
								<< "length <= 0. Ignoring stream." << std::endl;
							psids.erase(i.first);
							//Also erase all associated lsids.
							delete_stream(i.first);
							break;
						} else
							i.second.eos_seen = true;
					}
		for(auto& i : psids)
			if(!i.second.eos_seen)
				messages << "Warning: No EOS on stream " << hex::to(i.second.oggid, true)
					<< std::endl;
		delete_undefined_substreams();
		if(ssid_to_lsid.empty())
			throw std::runtime_error("No valid Oggopus streams found");
		for(auto& i : ssid_to_lsid) {
			uint32_t psid = lsid_to_psid[i.second];
			uint32_t next_psid = 0;
			uint32_t default_lsid;
			//Look up known psids.
			if(mscharacteristics.upper_bound(psid) != mscharacteristics.end())
				next_psid = mscharacteristics.upper_bound(psid)->first;
			else
				next_psid = mscharacteristics.begin()->first;
			default_lsid = ssid_to_lsid[(stringfmt() << "PSID" << next_psid).str()];
			bool deleted = false;
			do {
				deleted = false;
				for(auto& j : stransitions[i.second].next_subsongs)
					if(!lsid_to_psid.count(j)) {
						messages << "Warning: Undefined reference from '" << i.first
							<< "' to '" << reverse_lsid(j) << "'" << std::endl;
						stransitions[i.second].next_subsongs.erase(j);
						deleted = true;
						break;
					}
			} while(deleted);
			if(stransitions[i.second].next_subsongs.empty())
				stransitions[i.second].next_subsongs.insert(default_lsid);
			stransitions[i.second].fixup(psids[psid].pregap, psids[psid].pts, i.first);
		}
		if(entry.empty()) {
			for(auto& i : mscharacteristics) {
				entry.insert(ssid_to_lsid[(stringfmt() << "PSID" << i.first).str()]);
			}
		}
/*
		for(auto& i : ssid_to_lsid) {
			std::cerr << "SSID: " << i.first << " (#" << i.second << ") Start: "
				<< stransitions[i.second].start_pts / 48 << "ms Fade:"
				<< stransitions[i.second].xfade_pts / 48 << "ms End:"
				<< stransitions[i.second].end_pts / 48 << "ms"
				<< (entry.count(i.second) ? " ENTRYPOINT" : "") << std::endl;
			std::cerr << "\tNext up\t";
			bool f = true;
			for(auto& j : stransitions[i.second].next_subsongs) {
				std::cerr << (f ? "" : ",") << reverse_lsid(j);
				f = false;
			}
			uint32_t psid = lsid_to_psid[i.second];
			std::cerr << std::endl << "\tPhysical stream (PSID#" << psid << ", Ogg stream " <<
				hex::to(psids[psid].oggid) << "):" << std::endl;
			auto c = mscharacteristics[psid];
			std::cerr << "\t\t" << (int)c.channels << " channels (" << (int)c.streams << " streams, "
				<< (int)c.coupled << " coupled) @" << (c.gain / 256.0) << "dB" << std::endl;
			std::cerr << "\t\tPregap " << psids[psid].pregap / 48 << "ms, length " << psids[psid].pts / 48
				<< "ms." << std::endl;
		}
*/
	}

	song_buffer::subsong_context::subsong_context()
		: demux(messages)
	{
		psid = 0xDEADBEEF;
		last_granule = 0;
		pts = 0;
		last_pts = 0;
		pages = 0;
		eos_seen = false;
	}

	void song_buffer::fill_ms_characteristics(uint32_t subsong, struct multistream_characteristics& c)
	{
		if(!lsid_to_psid.count(subsong)) {
			c = multistream_characteristics();
			return;
		}
		subsong = lsid_to_psid[subsong];
		if(mscharacteristics.count(subsong))
			c = mscharacteristics[subsong];
		else
			c = multistream_characteristics();
	}

	const struct subsong_transition& song_buffer::transitions(uint32_t subsong)
	{
		if(stransitions.count(subsong))
			return stransitions[subsong];
		else
			return dummy_transition;
	}

	bool song_buffer::page_starts_new_stream(ogg::page& p)
	{
		if(!p.get_bos() || p.get_packet_count() != 1)
			return false;
		auto pkt = p.get_packet(0);
		if(pkt.second < 8 || memcmp(pkt.first, "OpusHead", 8))
			return false;
		return true;
	}

	bool song_buffer::parse_ogg_page(ogg::page& page, subsong_context& ctx)
	{
		ogg::packet p;
		if(!ctx.demux.page_in(page))
			return false;
		while(ctx.demux.wants_packet_out()) {
			ctx.demux.packet_out(p);
			if(ctx.pages == 0) {
				parse_ogg_header(p, ctx);
				ctx.pages = 1;
			} else if(ctx.pages == 1) {
				parse_ogg_tags(p, ctx, page);
				ctx.pages = 2;
			} else
				parse_ogg_data(p, ctx, page);
		}
		if(ctx.pages > 1)
			ctx.pages++;
		return true;
	}

	void song_buffer::parse_ogg_header(ogg::packet& p, subsong_context& ctx)
	{
		struct opus::ogg_header h;
		h.parse(p);
		fill_msc_from_header(mscharacteristics[ctx.psid], h);
		ctx.pregap = h.preskip;
		ctx.gain = h.gain;
	}

	void song_buffer::parse_ogg_tags(ogg::packet& p, subsong_context& ctx, const ogg::page& debug)
	{
		struct opus::ogg_tags t;
		t.parse(p);
		for(auto& i : t.comments) {
			try {
				regex_results r = regex("SKY-([^-]+)-([^=]+)=(.*)", i);
				if(!r)
					continue;
				uint32_t lsid = register_lsid(r[2], ctx.psid);
				if(r[1] == "START")
					stransitions[lsid].start_pts = parse_value<uint64_t>(r[3]) + ctx.pregap;
				if(r[1] == "FADE")
					stransitions[lsid].xfade_pts = parse_value<uint64_t>(r[3]) + ctx.pregap;
				if(r[1] == "END")
					stransitions[lsid].end_pts = parse_value<uint64_t>(r[3]) + ctx.pregap;
				if(r[1] == "NEXT") {
					std::string next = r[3];
					for(auto& token : token_iterator<char>::foreach(next, {","})) {
						uint32_t lsid2 = register_lsid(token);
						stransitions[lsid].next_subsongs.insert(lsid2);
					}
				}
				if(r[1] == "ENTRY") {
					entry.insert(lsid);
				}
			} catch(std::exception& e) {
				messages << "Warning: " << debug.stream_debug_id() << " tag '" << i << "': "
					<< e.what() << std::endl;
			}
		}
		//Make sure substream 0 exits.
		register_lsid((stringfmt() << "PSID" << ctx.psid).str(), ctx.psid);
	}

	void song_buffer::parse_ogg_data(ogg::packet& p, subsong_context& ctx, const ogg::page& debug)
	{
		std::pair<uint32_t, uint64_t> ptsx = std::make_pair(ctx.psid, ctx.pts);
		packetdata[ptsx] = p.get_vector();
		uint8_t t = opus::packet_tick_count(&packetdata[ptsx][0], packetdata[ptsx].size());
		ctx.pts += 120 * t;
		if(p.get_last_page()) {
			uint64_t samples = p.get_granulepos() - ctx.last_granule;
			if(samples > ctx.pts - ctx.last_pts) {
				//If there is only one data page, it is assumed to have zero base granulepos.
				//But for multiple pages, the first granulepos is arbitrary.
				if(ctx.pages > 2 || p.get_on_eos_page())
					messages << "Warning: " << debug.page_debug_id() << " Granulepos says there "
						<< "are " << samples << " samples, found " << ctx.pts - ctx.last_pts
						<< std::endl;
			} else if(p.get_on_eos_page())
				//On EOS page, clip.
				ctx.pts = ctx.last_pts + samples;
			else if(samples < ctx.pts - ctx.last_pts)
				messages << "Warning: " << debug.page_debug_id() << " Granulepos says there are "
					<< samples << " samples, found " << ctx.pts - ctx.last_pts
					<< std::endl;
			ctx.last_pts = ctx.pts;
			ctx.last_granule = p.get_granulepos();
		}
	}

	packet_decoder::packet_decoder()
	{
		d = NULL;
		channels = 0;
	}

	void packet_decoder::set_multistream(const struct multistream_characteristics& c)
	{
		try {
			size_t msstate_size = opus::multistream_decoder::size(c.streams, c.coupled);
			msstate_size += sizeof(opus::multistream_decoder) + alignof(opus::multistream_decoder);
			size_t dmix_offset = msstate_size;
			msstate_size += 5760 * c.channels * sizeof(float);
			if(memory.size() < msstate_size)
				memory.resize(msstate_size);
			uint8_t* a = &memory[0];
			if(reinterpret_cast<uint64_t>(a) % alignof(opus::multistream_decoder))
				a++;
			uint8_t* b = a + sizeof(opus::multistream_decoder);
			d = new(a) opus::multistream_decoder(opus::samplerate::r48k, c.channels, c.streams, c.coupled,
				c.mapping, reinterpret_cast<char*>(b));
			dmem = reinterpret_cast<float*>(a + dmix_offset);
			channels = c.channels;
			float gain_factor = 2 * pow(10, c.gain / 5120.0);
			for(unsigned i = 0; i < channels; i++) {
				downmix_l[i] = gain_factor * c.downmix_l[i];
				downmix_r[i] = gain_factor * c.downmix_r[i];
			}
		} catch(opus::not_loaded l) {
			d = NULL;
			channels = c.channels;
		}
	}

	void packet_decoder::decode_packet(const std::vector<uint8_t>& data)
	{
		size_t s = 120;
		try {
			if(d)
				s = d->decode(&data[0], data.size(), dmem, 5760);
			else {
				//Insert silence.
				uint8_t ticks = opus::packet_tick_count(&data[0], data.size());
				if(!ticks)
					ticks = 1;	//Try to recover.
				memset(pcmbuf, 0, 240 * ticks * sizeof(int16_t));
				pcmpos = 0;
				pcmlen = 120 * ticks;
				return;
			}
		} catch(std::exception& e) {
			//Try to insert silence.
			messages << "Failed to decode opus packet: " << e.what() << std::endl;
			uint8_t ticks = opus::packet_tick_count(&data[0], data.size());
			if(!ticks)
				ticks = 1;	//Try to recover.
			memset(pcmbuf, 0, 240 * ticks * sizeof(int16_t));
			pcmpos = 0;
			pcmlen = 120 * ticks;
			return;
		}
		for(unsigned i = 0; i < s; i++) {
			float a = 0;
			float b = 0;
			for(unsigned j = 0; j < channels; j++) {
				a += downmix_l[j] * dmem[i * channels + j];
				b += downmix_r[j] * dmem[i * channels + j];
			}
			pcmbuf[2 * i + 0] = min(max((int32_t)a, -32768), 32767);
			pcmbuf[2 * i + 1] = min(max((int32_t)b, -32768), 32767);
		}
		pcmpos = 0;
		pcmlen = s;
	}

	void packet_decoder::reset()
	{
		try {
			if(d)
				d->ctl(opus::reset);
		} catch(opus::not_loaded e) {
		}
	}

	music_player::music_player(struct music_player_memory& m, random& _rng)
		: mem(m), rng(_rng)
	{
		song = NULL;
	}

	void music_player::seek_channel(packet_decoder& i, uint64_t& spts, uint32_t subsong, uint64_t pts)
	{
		if(pts == past_end) {
			i.pcmpos = i.pcmlen = 0;
			spts = past_end;
			return;
		}
		i.reset();
		uint64_t ptsr = song->prev_timecode(subsong, (pts >= 3840) ? (pts - 3840) : 0);
		while(ptsr < pts) {
			ptsr = song->next_timecode(subsong, ptsr);
			if(ptsr == past_end)
				break;
			i.decode_packet(song->get_packet(subsong, ptsr));
			if(ptsr + i.pcmlen > pts) {
				i.pcmpos = pts - ptsr;
				ptsr = pts;
			} else
				ptsr += i.pcmlen;
		}
		spts = pts;
	}

	void music_player::song_to_beginning()
	{
		mem.pcmpos2 = past_end;
		mem.subsong2 = 0;
		if(!song) {
			mem.subsong1 = 0;
			mem.pcmpos1 = past_end;
			return;
		}
		const std::set<uint32_t>& songs = song->entrypoints();
		mem.subsong1 = pick_subsong(rng, songs);
		mem.pcmpos1 = song->transitions(mem.subsong1).start_pts;
	}

	void music_player::do_preroll()
	{
		multistream_characteristics c1, c2;
		if(song) {
			song->fill_ms_characteristics(mem.subsong1, c1);
			song->fill_ms_characteristics(mem.subsong2, c2);
		}
		i1.set_multistream(c1);
		i2.set_multistream(c2);
		if(!song)
			return;
		uint64_t pts1p;
		uint64_t pts2p;
		seek_channel(i1, pts1p, mem.subsong1, mem.pcmpos1);
		seek_channel(i2, pts2p, mem.subsong2, mem.pcmpos2);
	}

	void music_player::decode(std::pair<int16_t, int16_t>* output, size_t samples)
	{
		if(!song) {
			memset(output, 0, samples * sizeof(std::pair<int16_t, int16_t>));
			return;
		}
		const subsong_transition* strans1 = &song->transitions(mem.subsong1);
		const subsong_transition* strans2 = &song->transitions(mem.subsong2);
		for(; samples > 0; output++, samples--) {
			if(strans1->xfade_pts == mem.pcmpos1) {
				mem.subsong2 = pick_subsong(rng, strans1->next_subsongs);
				const subsong_transition& st = song->transitions(mem.subsong2);
				seek_channel(i2, mem.pcmpos2, mem.subsong2, st.start_pts);
				strans2 = &song->transitions(mem.subsong2);
			}
			if(strans2->xfade_pts == mem.pcmpos2) {
				mem.subsong1 = pick_subsong(rng, strans2->next_subsongs);
				const subsong_transition& st = song->transitions(mem.subsong1);
				seek_channel(i1, mem.pcmpos1, mem.subsong1, st.start_pts);
				strans1 = &song->transitions(mem.subsong1);
			}
			if(strans1->end_pts == mem.pcmpos1)
				seek_channel(i1, mem.pcmpos1, mem.subsong1, past_end);
			if(strans2->end_pts == mem.pcmpos2)
				seek_channel(i2, mem.pcmpos2, mem.subsong2, past_end);
			if(i1.pcmpos == i1.pcmlen && mem.pcmpos1 != past_end) {
				uint64_t pts = song->next_timecode(mem.subsong1, mem.pcmpos1);
				if(pts != past_end)
					i1.decode_packet(song->get_packet(mem.subsong1, mem.pcmpos1));
			}
			if(i2.pcmpos == i2.pcmlen && mem.pcmpos2 != past_end) {
				uint64_t pts = song->next_timecode(mem.subsong2, mem.pcmpos2);
				if(pts != past_end)
					i2.decode_packet(song->get_packet(mem.subsong2, mem.pcmpos2));
			}
			int32_t cf = 0, icf = 0;
			if(mem.pcmpos1 >= strans1->end_pts)
				icf = 256;
			else if(mem.pcmpos2 >= strans2->end_pts)
				cf = 256;
			else if(strans1->xfade_pts < mem.pcmpos1) {
				uint64_t cfstart = strans1->xfade_pts;
				uint64_t cflen = strans1->end_pts - strans1->xfade_pts;
				cf = 256 - 256 * (mem.pcmpos1 - cfstart) / cflen;
				icf = 256 - cf;
			} else if(strans2->xfade_pts < mem.pcmpos2) {
				uint64_t cfstart = strans2->xfade_pts;
				uint64_t cflen = strans2->end_pts - strans2->xfade_pts;
				icf = 256 - 256 * (mem.pcmpos2 - cfstart) / cflen;
				cf = 256 - icf;
			}
			int32_t l = (cf * i1.pcmbuf[2 * i1.pcmpos + 0] + icf * i2.pcmbuf[2 * i2.pcmpos + 0]) >> 8;
			int32_t r = (cf * i1.pcmbuf[2 * i1.pcmpos + 1] + icf * i2.pcmbuf[2 * i2.pcmpos + 1]) >> 8;
			output->first = max(min(l, 32767), -32768);
			output->second = max(min(r, 32767), -32768);
			if(i1.pcmpos < i1.pcmlen)
				i1.pcmpos++;
			if(i2.pcmpos < i2.pcmlen)
				i2.pcmpos++;
			if(mem.pcmpos1 != past_end)
				mem.pcmpos1++;
			if(mem.pcmpos2 != past_end)
				mem.pcmpos2++;
		}
	}
}
