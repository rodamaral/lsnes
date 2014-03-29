#include "opus-ogg.hpp"
#include <cstring>
#include "serialization.hpp"
#include "minmax.hpp"

namespace opus
{
void ogg_header::parse(struct ogg::packet& packet) throw(std::runtime_error)
{
	struct ogg_header h;
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
	*this = h;
}

void ogg_tags::parse(struct ogg::packet& packet) throw(std::bad_alloc, std::runtime_error)
{
	struct ogg_tags h;
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
	*this = h;
}

ogg::page ogg_header::serialize() throw(std::runtime_error)
{
	struct ogg::page page;
	unsigned char buffer[276];
	size_t bsize = 19;
	if(version != 1)
		throw std::runtime_error("Don't how to serialize this oggopus version");
	if(!channels || (channels > 2 && !map_family))
		throw std::runtime_error("Illegal channel count");
	if(map_family && static_cast<int>(streams) > 255 - coupled)
		throw std::runtime_error("Maximum of 255 physical channels exceeded");
	if(map_family)
		for(unsigned i = 0; i < channels; i++)
			if(chanmap[i] != 255 && chanmap[i] > streams + coupled)
				throw std::runtime_error("Logical channel mapped to invalid physical channel");
	serialization::u64b(buffer, 0x4F70757348656164ULL);
	buffer[8] = version;
	buffer[9] = channels;
	serialization::u16l(buffer + 10, preskip);
	serialization::u32l(buffer + 12, rate);
	serialization::s16l(buffer + 16, gain);
	buffer[18] = map_family;
	if(map_family) {
		buffer[19] = streams;
		buffer[20] = coupled;
		memcpy(buffer + 21, chanmap, channels);
		bsize = 21 + channels;
	} else
		bsize = 19;
	if(!page.append_packet(buffer, bsize))
		throw std::runtime_error("Header packet too large");
	page.set_granulepos(0);
	page.set_sequence(0);
	page.set_bos(true);
	return page;
}

uint32_t ogg_tags::serialize(std::function<void(const ogg::page& p)> output,
	uint32_t strmid) throw(std::bad_alloc, std::runtime_error)
{
	size_t needed = 8;
	needed += vendor.length();
	needed += 4;
	for(auto i : comments)
		needed += (i.length() + 4);

	//TODO: Do without this buffer.
	std::vector<uint8_t> contents;
	contents.resize(needed);
	size_t itr = 0;
	serialization::u64b(&contents[0], 0x4F70757354616773ULL);
	serialization::u32l(&contents[8], vendor.length());
	std::copy(vendor.begin(), vendor.end(), reinterpret_cast<char*>(&contents[12]));
	itr = 12 + vendor.length();
	serialization::u32l(&contents[itr], comments.size());
	itr += 4;
	for(auto i : comments) {
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
	return next_page;
}
}
