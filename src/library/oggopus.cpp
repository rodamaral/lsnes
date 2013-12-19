#include "oggopus.hpp"
#include <cstring>
#include "serialization.hpp"
#include "minmax.hpp"

struct oggopus_header parse_oggopus_header(struct ogg::packet& packet) throw(std::runtime_error)
{
	struct oggopus_header h;
	if(!packet.get_atomic())
		throw std::runtime_error("OggOpus header page must have one complete packet");
	if(packet.get_granulepos() != 0)
		throw std::runtime_error("OggOpus header page must have granulepos 0");
	if(!packet.get_on_bos_page() || packet.get_on_eos_page())
		throw std::runtime_error("OggOpus header page must be first but not last page");
	const std::vector<uint8_t>& p = packet.get_vector();
	if(p.size() < 9 || memcmp(&p[0], "OpusHead", 8))
		throw std::runtime_error("Bad OggOpus header magic");
	if(p[8] & 0xF0)
		throw std::runtime_error("Unsupported OggOpus version");
	if(p.size() < 19 || (p[18] && p.size() < 21U + p[9]))
		throw std::runtime_error("OggOpus header packet truncated");
	if(!p[9])
		throw std::runtime_error("Zero channels not allowed");
	h.version = p[8];
	h.channels = p[9];
	h.preskip = serialization::u16l(&p[10]);
	h.rate = serialization::u32l(&p[12]);
	h.gain = serialization::s16l(&p[16]);
	h.map_family = p[18];
	memset(h.chanmap, 255, sizeof(h.chanmap));
	if(h.map_family) {
		h.streams = p[19];
		h.coupled = p[20];
		if(h.coupled > h.streams)
			throw std::runtime_error("More coupled streams than total streams.");
		if(static_cast<int>(h.streams) > 255 - h.coupled)
			throw std::runtime_error("Maximum of 255 physical channels exceeded");
		memcpy(h.chanmap, &p[21], h.channels);
		for(unsigned i = 0; i < h.channels; i++)
			if(h.chanmap[i] != 255 && h.chanmap[i] > h.streams + h.coupled)
				throw std::runtime_error("Logical channel mapped to invalid physical channel");
	} else {
		h.streams = 1;
		if(h.channels > 2)
			throw std::runtime_error("Only 1 or 2 channels allowed with mapping family 0");
		h.coupled = (h.channels == 2) ? 1 : 0;
		h.chanmap[0] = 0;
		if(h.channels == 2) h.chanmap[1] = 1;
	}
	return h;
}

struct oggopus_tags parse_oggopus_tags(struct ogg::packet& packet) throw(std::bad_alloc, std::runtime_error)
{
	struct oggopus_tags h;
	if(!packet.get_first_page() || !packet.get_last_page())
		throw std::runtime_error("OggOpus tags packet must be alone on its pages");
	if(packet.get_granulepos() != 0)
		throw std::runtime_error("OggOpus header page must have granulepos 0");
	if(packet.get_on_bos_page())
		throw std::runtime_error("OggOpus tags page must not be first page");
	const std::vector<uint8_t>& p = packet.get_vector();
	if(p.size() < 8 || memcmp(&p[0], "OpusTags", 8))
		throw std::runtime_error("Bad OggOpus tags magic");
	if(p.size() < 12)
		throw std::runtime_error("OggOpus header packet truncated");
	//Scan the thing.
	size_t itr = 8;
	size_t oitr = 8;
	itr = itr + 4 + serialization::u32l(&p[itr]);
	if(itr + 4 > p.size())
		throw std::runtime_error("OggOpus header packet truncated");
	h.vendor = std::string(&p[oitr + 4], &p[itr]);
	if(itr + 4 > p.size())
		throw std::runtime_error("OggOpus header packet truncated");
	uint32_t headers = serialization::u32l(&p[itr]);
	itr += 4;
	for(uint32_t i = 0; i < headers; i++) {
		if(itr + 4 > p.size())
			throw std::runtime_error("OggOpus header packet truncated");
		itr = itr + 4 + serialization::u32l(&p[itr]);
		if(itr > p.size())
			throw std::runtime_error("OggOpus header packet truncated");
		h.comments.push_back(std::string(&p[oitr + 4], &p[itr]));
		oitr = itr;
	}
	return h;
}

struct ogg::page serialize_oggopus_header(struct oggopus_header& header) throw(std::runtime_error)
{
	struct ogg::page page;
	unsigned char buffer[276];
	size_t bsize = 19;
	if(header.version != 1)
		throw std::runtime_error("Don't how to serialize this oggopus version");
	if(!header.channels || (header.channels > 2 && !header.map_family))
		throw std::runtime_error("Illegal channel count");
	if(header.map_family && static_cast<int>(header.streams) > 255 - header.coupled)
		throw std::runtime_error("Maximum of 255 physical channels exceeded");
	if(header.map_family)
		for(unsigned i = 0; i < header.channels; i++)
			if(header.chanmap[i] != 255 && header.chanmap[i] > header.streams + header.coupled)
				throw std::runtime_error("Logical channel mapped to invalid physical channel");
	serialization::u64b(buffer, 0x4F70757348656164ULL);
	buffer[8] = header.version;
	buffer[9] = header.channels;
	serialization::u16l(buffer + 10, header.preskip);
	serialization::u32l(buffer + 12, header.rate);
	serialization::s16l(buffer + 16, header.gain);
	buffer[18] = header.map_family;
	if(header.map_family) {
		buffer[19] = header.streams;
		buffer[20] = header.coupled;
		memcpy(buffer + 21, header.chanmap, header.channels);
		bsize = 21 + header.channels;
	} else
		bsize = 19;
	if(!page.append_packet(buffer, bsize))
		throw std::runtime_error("Header packet too large");
	page.set_granulepos(0);
	page.set_sequence(0);
	page.set_bos(true);
	return page;
}

uint32_t serialize_oggopus_tags(struct oggopus_tags& tags, std::function<void(const ogg::page& p)> output,
	uint32_t strmid) throw(std::bad_alloc, std::runtime_error)
{
	size_t needed = 8;
	needed += tags.vendor.length();
	needed += 4;
	for(auto i : tags.comments)
		needed += (i.length() + 4);

	//TODO: Do without this buffer.
	std::vector<uint8_t> contents;
	contents.resize(needed);
	size_t itr = 0;
	serialization::u64b(&contents[0], 0x4F70757354616773ULL);
	serialization::u32l(&contents[8], tags.vendor.length());
	std::copy(tags.vendor.begin(), tags.vendor.end(), reinterpret_cast<char*>(&contents[12]));
	itr = 12 + tags.vendor.length();
	serialization::u32l(&contents[itr], tags.comments.size());
	itr += 4;
	for(auto i : tags.comments) {
		serialization::u32l(&contents[itr], i.length());
		std::copy(i.begin(), i.end(), reinterpret_cast<char*>(&contents[itr + 4]));
		itr += (i.length() + 4);
	}

	uint32_t next_page = 1;
	size_t written = 0;
	while(true) {
		ogg::page q;
		q.set_continue(next_page != 1);
		q.set_bos(false);
		q.set_eos(false);
		q.set_granulepos(0);
		q.set_stream(strmid);
		q.set_sequence(next_page++);
		const uint8_t* ptr = &contents[written];
		size_t szr = needed - written;
		bool complete = q.append_packet_incomplete(ptr, szr);
		output(q);
		if(complete)
			break;
		written = ptr - &contents[0];
	}
}

uint8_t opus_packet_tick_count(const uint8_t* packet, size_t packetsize)
{
	if(packetsize < 1)
		return 0;
	uint8_t x = ((packet[0] >= 0x70) ? 1 : 4) << ((packet[0] >> 3) & 3);
	x = min(x, (uint8_t)24);
	uint8_t y = (packetsize < 2) ? 255 : (packet[1] & 0x3F);
	uint16_t z = (uint16_t)x * y;
	switch(packet[0] & 3) {
	case 0:		return x;
	case 1:		return x << 1;
	case 2:		return x << 1;
	case 3:		return (z <= 48) ? z : 0;
	};
}
