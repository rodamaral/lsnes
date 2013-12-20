#include "ogg.hpp"
#include "serialization.hpp"
#include "minmax.hpp"
#include "hex.hpp"
#include <cstring>
#include <zlib.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include "string.hpp"

namespace ogg
{
namespace {
	const uint32_t crc_lookup[256]= {
		0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,
		0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,
		0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,
		0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
		0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,
		0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,
		0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,
		0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
		0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,
		0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,
		0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,
		0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
		0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,
		0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,
		0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,
		0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
		0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,
		0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,
		0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,
		0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
		0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,
		0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,
		0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,
		0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
		0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,
		0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,
		0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,
		0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
		0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,
		0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,
		0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,
		0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
		0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,
		0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,
		0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,
		0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
		0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,
		0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,
		0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,
		0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
		0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,
		0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,
		0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,
		0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
		0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,
		0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,
		0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,
		0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
		0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,
		0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,
		0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,
		0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
		0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,
		0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,
		0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,
		0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
		0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,
		0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,
		0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,
		0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
		0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,
		0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,
		0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,
		0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4
	};

	//Grr... Ogg doesn't use the same CRC32 as zlib...
	uint32_t oggcrc32(uint32_t chain, const uint8_t* data, size_t size)
	{
		if(!data)
			return 0;
		for(size_t i = 0; i < size; i++)
			chain = (chain << 8) ^ crc_lookup[(chain >> 24) ^ data[i]];
		return chain;
	}
}


packet::packet(uint64_t granule, bool first, bool last, bool spans, bool eos, bool bos, 
	const std::vector<uint8_t>& d)
{
	data = d;
	granulepos = granule;
	first_page = first;
	last_page = last;
	spans_page = spans;
	eos_page = eos;
	bos_page = bos;
}

demuxer::demuxer(std::ostream& _errors_to)
	: errors_to(_errors_to)
{
	seen_page = false;
	imprint_stream = 0;
	page_seq = 0;
	page_era = 0;
	dpacket = 0;
	packets = 0;
	ended = false;
	damaged_packet = false;
	last_granulepos = 0;
}

uint64_t demuxer::page_fullseq(uint32_t seq)
{
	if(seq < page_seq)
		return (static_cast<uint64_t>(page_era + 1) << 32) + seq;
	else
		return (static_cast<uint64_t>(page_era) << 32) + seq;
}

void demuxer::update_pageseq(uint32_t new_seq)
{
	if(new_seq < page_seq)
		page_era++;
	page_seq = new_seq;
}

bool demuxer::complain_lost_page(uint32_t new_seq, uint32_t stream)
{
	if(new_seq != static_cast<uint32_t>(page_seq + 1)) {
		//Some pages are missing!
		uint64_t first_missing = page_fullseq(page_seq) + 1;
		uint64_t last_missing = page_fullseq(new_seq) - 1;
		if(first_missing == last_missing)
			errors_to << "Warning: Ogg demux: Page " << first_missing << " missing on stream "
				<< hex::to(stream) << std::endl;
		else
			errors_to << "Warning: Ogg demux: Pages " << first_missing << "-" << last_missing
				<< " missing on stream " << hex::to(stream) << std::endl;
		return true;
	}
	return false;
}

void demuxer::complain_continue_errors(unsigned flags, uint32_t seqno, uint32_t stream, uint32_t pkts,
	uint64_t granule)
{
	bool continued = (flags & 1);
	bool bos = (flags & 2);
	bool eos = (flags & 4);
	bool incomplete = (flags & 8);
	bool damaged = (flags & 16);
	bool gap = (flags & 32);
	bool data = (flags & 64);
	if(!continued) {
		if(data && !damaged && !gap)
			errors_to << "Warning: Ogg demux: Data spilled from previous page but not continued (page "
				<< page_fullseq(seqno) << " of " << stream << ")" << std::endl;
		if(data && !damaged && gap)
			errors_to << "Warning: Ogg demux: Packet continues to lost page" << std::endl;
	} else {
		if(bos)
			errors_to << "Warning: Ogg demux: BOS page has CONTINUED set (stream " << stream << ")."
				<< std::endl;
		else if(!data && !damaged && !gap)
			errors_to << "Warning: Ogg demux: No Data spilled from previous page but continued (page "
				<< page_fullseq(seqno) << " of " << stream << ")" << std::endl;
		if(data && !damaged && gap)
			errors_to << "Warning: Ogg demux: Packet continues to/from lost page" << std::endl;
		if(!data && !damaged && gap)
			errors_to << "Warning: Ogg demux: Packet continues from lost page" << std::endl;
	}
	if(incomplete && eos)
		errors_to << "Warning: Ogg demux: EOS page with incomplete packet (stream " << stream << ")."
			<< std::endl;
	if(incomplete && pkts == 1 && granule != page::granulepos_none)
		errors_to << "Warning: Ogg demux: Page should have granulepos NONE (page " << page_fullseq(seqno)
			<< " of " << stream << ")" << std::endl;
	if((!incomplete || pkts > 1) && granule == page::granulepos_none)
		errors_to << "Warning: Ogg demux: Page should not have granulepos NONE (page " << page_fullseq(seqno)
			<< " of " << stream << ")" << std::endl;
}

bool demuxer::page_in(const page& p)
{
	//Is this from the right stream? If not, ignore page.
	uint32_t stream = p.get_stream();
	if(seen_page && stream != imprint_stream)
		return false;		//Wrong stream.
	if(!wants_page_in())
		throw std::runtime_error("Not ready for page");
	std::vector<uint8_t> newbuffer;
	uint32_t sequence = p.get_sequence();
	uint32_t pkts = p.get_packet_count();
	uint64_t granulepos = p.get_granulepos();
	bool bos = p.get_bos();
	bool eos = p.get_eos();
	bool continued = p.get_continue();
	bool incomplete = p.get_last_packet_incomplete();
	//BOS flag can only be set on first page.
	if(granulepos < last_granulepos && granulepos != page::granulepos_none)
		errors_to << "Warning: Ogg demux: Non-monotonic granulepos" << std::endl;
	if(!seen_page && !bos)
		errors_to << "Warning: Ogg demux: First page does not have BOS set." << std::endl;
	if(seen_page && bos)
		errors_to << "Warning: Ogg demux: Seen another BOS on stream" << std::endl;
	//Complain about any gaps in stream.
	bool gap = seen_page && complain_lost_page(sequence, stream);
	//Complain about continuation errors.
	unsigned flags = 0;
	flags |= continued ? 1 : 0;
	flags |= bos ? 2 : 0;
	flags |= eos ? 4 : 0;
	flags |= incomplete ? 8 : 0;
	flags |= damaged_packet ? 16 : 0;
	flags |= gap ? 32 : 0;
	flags |= !partial.empty() ? 64 : 0;
	complain_continue_errors(flags, sequence, stream, pkts, granulepos);
	if(continued) {
		if(pkts == 1 && incomplete) {
			//Nothing finishes on this page either.
			auto frag = p.get_packet(0);
			newbuffer.resize(partial.size() + frag.second);
			memcpy(&newbuffer[0], &partial[0], partial.size());
			memcpy(&newbuffer[partial.size()], frag.first, frag.second);
			std::swap(newbuffer, partial);
			damaged_packet = damaged_packet || gap;
			seen_page = true;
			imprint_stream = stream;
			update_pageseq(sequence);
			ended = eos;
			if(granulepos != page::granulepos_none)
				last_granulepos = granulepos;
			return true;
		} else if(pkts == 2 && incomplete && (partial.empty() || damaged_packet || gap)) {
			//The first packet is busted and the second is incomplete. Load the rest.
			auto frag = p.get_packet(1);
			newbuffer.resize(frag.second);
			memcpy(&newbuffer[0], frag.first, frag.second);
			std::swap(newbuffer, partial);
			seen_page = true;
			imprint_stream = stream;
			update_pageseq(sequence);
			damaged_packet = false;
			ended = eos;
			dpacket = 1;
			packets = 1;
			if(granulepos != page::granulepos_none)
				last_granulepos = granulepos;
			return true;
		}
	}
	dpacket = (continued && (partial.empty() || damaged_packet || gap)) ? 1 : 0;	//Busted?
	packets = pkts;
	if(incomplete)
		packets--;
	last_page = p;
	damaged_packet = false;
	seen_page = true;
	imprint_stream = stream;
	update_pageseq(sequence);
	ended = eos;
	if(granulepos != page::granulepos_none)
		last_granulepos = granulepos;
	return true;
}

void demuxer::packet_out(ogg::packet& pkt)
{
	if(!wants_packet_out())
		throw std::runtime_error("Not ready for packet");
	bool firstfrag = (dpacket == 0 && last_page.get_continue());
	bool lastfrag = (dpacket == packets - 1 && last_page.get_last_packet_incomplete());
	if(!firstfrag) {
		//Wholly on this page.
		std::vector<uint8_t> newbuffer;
		auto frag = last_page.get_packet(dpacket);
		newbuffer.resize(frag.second);
		memcpy(&newbuffer[0], frag.first, frag.second);
		pkt = packet(last_page.get_granulepos(), dpacket == 0, (dpacket == packets - 1), false,
			last_page.get_eos(), last_page.get_bos(), newbuffer);
	} else {
		//Continued from the last page.
		std::vector<uint8_t> newbuffer;
		auto frag = last_page.get_packet(0);
		newbuffer.resize(partial.size() + frag.second);
		memcpy(&newbuffer[0], &partial[0], partial.size());
		memcpy(&newbuffer[partial.size()], frag.first, frag.second);
		pkt = packet(last_page.get_granulepos(), true, (packets == 1), true, last_page.get_eos(),
			started_bos, newbuffer);
	}
	if(lastfrag) {
		//Load the next packet fragment
		auto frag2 = last_page.get_packet(dpacket + 1);
		std::vector<uint8_t> newbuffer;
		newbuffer.resize(frag2.second);
		memcpy(&newbuffer[0], frag2.first, frag2.second);
		std::swap(newbuffer, partial);
		started_bos = last_page.get_bos();
	}
	dpacket++;
}

void demuxer::discard_packet()
{
	if(!wants_packet_out())
		throw std::runtime_error("Not ready for packet");
	bool lastfrag = (dpacket == packets - 1 && last_page.get_last_packet_incomplete());
	if(lastfrag) {
		//Load the next packet fragment
		auto frag2 = last_page.get_packet(dpacket + 1);
		std::vector<uint8_t> newbuffer;
		newbuffer.resize(frag2.second);
		memcpy(&newbuffer[0], frag2.first, frag2.second);
		std::swap(newbuffer, partial);
	}
	dpacket++;
}

muxer::muxer(uint32_t streamid, uint64_t _seq)
{
	strmid = streamid;
	written = 0;
	eos_asserted = false;
	seq = _seq;
	granulepos = page::granulepos_none;
	buffer.set_granulepos(page::granulepos_none);
}


bool muxer::packet_fits(size_t pktsize) const throw()
{
	return pktsize <= buffer.get_max_complete_packet();
}

bool muxer::wants_packet_in() const throw()
{
	return buffered.size() == written && !eos_asserted;
}

bool muxer::has_page_out() const throw()
{
	return buffer.get_packet_count() > 0;
}

void muxer::signal_eos()
{
	eos_asserted = true;
}

void muxer::packet_in(const std::vector<uint8_t>& data, uint64_t granule)
{
	if(!wants_packet_in() || eos_asserted)
		throw std::runtime_error("Muxer not ready for packet");
	buffered = data;
	//Try direct write.
	const uint8_t* _data = &data[0];
	size_t _len = data.size();
	bool r = buffer.append_packet_incomplete(_data, _len);
	if(r) {
		written = data.size();
		buffer.set_granulepos(granule);
		return;		//Complete write.
	}
	granulepos = granule;
	written = data.size() - _len;
}

void muxer::page_out(page& p)
{
	if(!has_page_out())
		throw std::runtime_error("Muxer not ready for page");
	if(eos_asserted && written == buffered.size())
		buffer.set_eos(true);	//This is the end.
	buffer.set_bos(seq == 0);
	buffer.set_sequence(seq);
	buffer.set_stream(strmid);
	p = buffer;
	buffer = page();
	seq++;
	//Now we have a fresh page, flush buffer there.
	if(written < buffered.size()) {
		const uint8_t* _data = &buffered[written];
		size_t _len = buffered.size() - written;
		bool r = buffer.append_packet_incomplete(_data, _len);
		if(r) {
			written = buffered.size();
			buffer.set_granulepos(granulepos);
			buffer.set_continue(written != 0);
			granulepos = page::granulepos_none;
			return;
		}
		written = buffered.size() - _len;
	}
	buffer.set_granulepos(page::granulepos_none);
}

page::page() throw()
{
	version = 0;
	flag_continue = false;
	flag_bos = false;
	flag_eos = false;
	last_incomplete = false;
	granulepos = granulepos_none;
	stream = 0;
	sequence = 0;
	segment_count = 0;
	packet_count = 0;
	data_count = 0;
	memset(data, 0, sizeof(data));
	memset(segments, 0, sizeof(segments));
	memset(packets, 0, sizeof(packets));
}

page::page(const char* buffer, size_t& advance) throw(std::runtime_error)
{
	//Check validity of page header.
	if(buffer[0] != 'O' || buffer[1] != 'g' || buffer[2] != 'g' || buffer[3] != 'S')
		throw std::runtime_error("Bad Ogg page header");
	if(buffer[4] != 0)
		throw std::runtime_error("Bad Ogg page version");
	if(buffer[5] & 0xF8)
		throw std::runtime_error("Bad Ogg page flags");
	//Compute length.
	size_t b = 27 + (unsigned char)buffer[26];
	data_count = 0;
	for(unsigned i = 0; i < (unsigned char)buffer[26]; i++) {
		b += (unsigned char)buffer[27 + i];
		data_count += (unsigned char)buffer[27 + i];
	}
	//Check the CRC.
	uint32_t claimed = serialization::u32l(buffer + 22);
	uint32_t x = 0;
	uint32_t actual = oggcrc32(0, NULL, 0);
	actual = oggcrc32(actual, reinterpret_cast<const uint8_t*>(buffer), 22);
	actual = oggcrc32(actual, reinterpret_cast<const uint8_t*>(&x), 4);
	actual = oggcrc32(actual, reinterpret_cast<const uint8_t*>(buffer + 26), b - 26);
	if(claimed != actual)
		throw std::runtime_error("Bad Ogg page checksum");
	//This packet is valid.
	version = buffer[4];
	uint8_t flags = buffer[5];
	flag_continue = (flags & 1);
	flag_bos = (flags & 2);
	flag_eos = (flags & 4);
	granulepos = serialization::u64l(buffer + 6);
	stream = serialization::u32l(buffer + 14);
	sequence = serialization::u32l(buffer + 18);
	segment_count = buffer[26];
	memset(segments, 0, sizeof(segments));
	if(segment_count)
		memcpy(segments, buffer + 27, segment_count);
	memset(data, 0, sizeof(data));
	if(b > 27U + segment_count)
		memcpy(data, buffer + 27 + segment_count, b - 27 - segment_count);
	packet_count = 0;
	memset(packets, 0, sizeof(packets));
	if(segment_count > 0)
		packets[packet_count++] = 0;
	uint16_t dptr = 0;
	for(unsigned i = 0; i < segment_count; i++) {
		dptr += segments[i];
		if(segment_count > i + 1 && segments[i] < 255)
			packets[packet_count++] = dptr;
	}
	packets[packet_count] = dptr;
	last_incomplete = (!flag_eos && segment_count > 0 && segments[segment_count - 1] == 255);
	advance = b;
}

bool page::scan(const char* buffer, size_t bufferlen, bool eof, size_t& advance) throw()
{
	const char* _buffer = buffer;
	size_t buffer_left = bufferlen;
	advance = 0;
	while(buffer_left >= 27) {
		//Check capture pattern.
		if(_buffer[0] != 'O' || _buffer[1] != 'g' || _buffer[2] != 'g' || _buffer[3] != 'S') {
			advance++;
			_buffer++;
			buffer_left--;
			continue;
		}
		//Check that version is valid.
		if(_buffer[4] != 0) {
			advance++;
			_buffer++;
			buffer_left--;
			continue;
		}
		//Check that flags are valid.
		if(_buffer[5] & 0xF8) {
			advance++;
			_buffer++;
			buffer_left--;
			continue;
		}
		//Check that segment table is present. If not, more data can uncover a page here.
		if(27U + (unsigned char)_buffer[26] > buffer_left) {
			if(!eof) {
				return false;
			} else {
				advance++;
				_buffer++;
				buffer_left--;
				continue;
			}
		}
		//Check that all data is there. If not, more data can uncover a page here.
		size_t b = 27 + (unsigned char)_buffer[26];
		for(unsigned i = 0; i < (unsigned char)_buffer[26]; i++)
			b += (unsigned char)_buffer[27 + i];
		if(b > buffer_left) {
			if(!eof) {
				return false;
			} else {
				advance++;
				_buffer++;
				buffer_left--;
				continue;
			}
		}
		//Check the CRC.
		uint32_t claimed = serialization::u32l(_buffer + 22);
		uint32_t x = 0;
		uint32_t actual = oggcrc32(0, NULL, 0);
		actual = oggcrc32(actual, reinterpret_cast<const uint8_t*>(_buffer), 22);
		actual = oggcrc32(actual, reinterpret_cast<const uint8_t*>(&x), 4);
		actual = oggcrc32(actual, reinterpret_cast<const uint8_t*>(_buffer + 26), b - 26);
		if(claimed != actual) {
			//CRC check fails. Advance.
			advance++;
			_buffer++;
			buffer_left--;
			continue;
		}
		return true;	//Here is a packet.
	}
	if(eof && buffer_left < 27) {
		//Advance to the end.
		advance += buffer_left;
	}
	return false;
}

std::string page::stream_debug_id() const throw(std::bad_alloc)
{
	return (stringfmt() << "Stream " << hex::to(stream)).str();
}

std::string page::page_debug_id() const throw(std::bad_alloc)
{
	return (stringfmt() << stream_debug_id() << " page " << sequence).str();
}

size_t page::get_max_complete_packet() const throw()
{
	if(segment_count == 255)
		return 0;
	return (255 - segment_count) * 255 - 1;
}

bool page::append_packet(const uint8_t* _data, size_t datalen) throw()
{
	//Compute the smallest amount of data we can't write.
	size_t imin = (255 - segment_count) * 255;
	if(datalen >= imin)
		return false;		//Can't write.
	//Okay, it fits. Write.
	packets[packet_count++] = data_count;
	bool terminate = false;
	while(datalen > 0) {
		if(datalen >= 255) {
			segments[segment_count++] = 255;
			memcpy(data + data_count, _data, 255);
			data_count += 255;
			_data += 255;
			datalen -= 255;
		} else {
			segments[segment_count++] = datalen;
			memcpy(data + data_count, _data, datalen);
			data_count += datalen;
			_data += datalen;
			datalen = 0;
			terminate = true;
		}
	}
	if(!terminate)
		segments[segment_count++] = 0;
	packets[packet_count] = data_count;
	last_incomplete = false;
	return true;
}

bool page::append_packet_incomplete(const uint8_t*& _data, size_t& datalen) throw()
{
	//If we have absolutely no space, don't flag a packet.
	if(segment_count == 255)
		return false;
	packets[packet_count++] = data_count;
	//Append segments, one by one.
	while(segment_count < 255) {
		if(datalen >= 255) {
			segments[segment_count++] = 255;
			memcpy(data + data_count, _data, 255);
			data_count += 255;
			_data += 255;
			datalen -= 255;
		} else {
			//Final segment of packet.
			segments[segment_count++] = datalen;
			memcpy(data + data_count, _data, datalen);
			data_count += datalen;
			_data += datalen;
			datalen = 0;
			packets[packet_count] = data_count;
			last_incomplete = false;
			return true;
		}
	}
	packets[packet_count] = data_count;
	last_incomplete = true;
	return false;
}

void page::serialize(char* buffer) const throw()
{
	memcpy(buffer, "OggS", 4);
	buffer[4] = version;
	buffer[5] = (flag_continue ? 1 : 0) | (flag_bos ? 2 : 0) | (flag_eos ? 4 : 0);
	serialization::u64l(buffer + 6, granulepos);
	serialization::u32l(buffer + 14, stream);
	serialization::u32l(buffer + 18, sequence);
	serialization::u32l(buffer + 22, 0);	//CRC will be fixed later.
	buffer[26] = segment_count;
	memcpy(buffer + 27, segments, segment_count);
	memcpy(buffer + 27 + segment_count, data, data_count);
	size_t plen = 27 + segment_count + data_count;
	//Fix the CRC.
	serialization::u32l(buffer + 22, oggcrc32(oggcrc32(0, NULL, 0), reinterpret_cast<uint8_t*>(buffer), plen));
}

const uint64_t page::granulepos_none = 0xFFFFFFFFFFFFFFFFULL;



stream_reader::stream_reader() throw()
{
	eof = false;
	left = 0;
	errors_to = &std::cerr;
	last_offset = 0;
	start_offset = 0;
}

stream_reader::~stream_reader() throw()
{
}

void stream_reader::set_errors_to(std::ostream& os)
{
	errors_to = &os;
}

bool stream_reader::get_page(page& spage) throw(std::exception)
{
	size_t advance;
	bool f;
try_again:
	fill_buffer();
	if(eof && !left)
		return false;
	f = page::scan(buffer, left, eof, advance);
	if(advance) {
		//The ogg stream resyncs.
		(*errors_to) << "Warning: Ogg stream: Recapture after " << advance << " bytes." << std::endl;
		discard_buffer(advance);
		goto try_again;
	}
	if(!f)
		goto try_again;
	spage = page(buffer, advance);
	last_offset = start_offset;
	discard_buffer(advance);
	return true;
}

void stream_reader::fill_buffer()
{
	size_t r;
	if(!eof && left < sizeof(buffer)) {
		left += (r = read(buffer + left, sizeof(buffer) - left));
		if(!r)
			eof = true;
	}
}

void stream_reader::discard_buffer(size_t amount)
{
	if(amount < left)
		memmove(buffer, buffer + amount, left - amount);
	left -= amount;
	start_offset += amount;
}

stream_writer::stream_writer() throw()
{
}

stream_writer::~stream_writer() throw()
{
}

void stream_writer::put_page(const page& page) throw(std::exception)
{
	char buffer[65536];
	size_t s = page.serialize_size();
	page.serialize(buffer);
	write(buffer, s);
}

stream_reader_iostreams::stream_reader_iostreams(std::istream& stream)
	: is(stream)
{
}

stream_reader_iostreams::~stream_reader_iostreams() throw()
{
}

size_t stream_reader_iostreams::read(char* buffer, size_t size) throw(std::exception)
{
	if(!is)
		return 0;
	is.read(buffer, size);
	return is.gcount();
}

stream_writer_iostreams::stream_writer_iostreams(std::ostream& stream)
	: os(stream)
{
}

stream_writer_iostreams::~stream_writer_iostreams() throw()
{
}

void stream_writer_iostreams::write(const char* buffer, size_t size) throw(std::exception)
{
	if(!os)
		throw std::runtime_error("Error writing data");
	os.write(buffer, size);
	if(!os)
		throw std::runtime_error("Error writing data");
}
}
