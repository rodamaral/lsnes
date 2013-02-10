#include "ogg.hpp"
#include "serialization.hpp"
#include <cstring>
#include <zlib.h>
#include <algorithm>
#include <iostream>

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

	uint32_t ogg_crc32(uint32_t chain, const uint8_t* data, size_t size)
	{
		if(!data)
			return 0;
		for(size_t i = 0; i < size; i++)
			chain = (chain << 8) ^ crc_lookup[(chain >> 24) ^ data[i]];
		return chain;
	}
}


ogg_page::ogg_page() throw()
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

ogg_page::ogg_page(const char* buffer, size_t& advance) throw(std::runtime_error)
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
	for(unsigned i = 0; i < (unsigned char)buffer[26]; i++)
		b += (unsigned char)buffer[27 + i];
	//Check the CRC.
	uint32_t claimed = read32ule(buffer + 22);
	uint32_t x = 0;
	uint32_t actual = ogg_crc32(0, NULL, 0);
	actual = ogg_crc32(actual, reinterpret_cast<const uint8_t*>(buffer), 22);
	actual = ogg_crc32(actual, reinterpret_cast<const uint8_t*>(&x), 4);
	actual = ogg_crc32(actual, reinterpret_cast<const uint8_t*>(buffer + 26), b - 26);
	if(claimed != actual)
		throw std::runtime_error("Bad Ogg page checksum");
	//This packet is valid.
	version = buffer[4];
	uint8_t flags = buffer[5];
	flag_continue = (flags & 1);
	flag_bos = (flags & 2);
	flag_eos = (flags & 4);
	granulepos = read64ule(buffer + 6);
	stream = read32ule(buffer + 14);
	sequence = read32ule(buffer + 18);
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

bool ogg_page::scan(const char* buffer, size_t bufferlen, bool eof, size_t& advance) throw()
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
		uint32_t claimed = read32ule(_buffer + 22);
		uint32_t x = 0;
		uint32_t actual = ogg_crc32(0, NULL, 0);
		actual = ogg_crc32(actual, reinterpret_cast<const uint8_t*>(_buffer), 22);
		actual = ogg_crc32(actual, reinterpret_cast<const uint8_t*>(&x), 4);
		actual = ogg_crc32(actual, reinterpret_cast<const uint8_t*>(_buffer + 26), b - 26);
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

bool ogg_page::append_packet(const uint8_t* _data, size_t datalen) throw()
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

bool ogg_page::append_packet_incomplete(const uint8_t*& _data, size_t& datalen) throw()
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

void ogg_page::serialize(char* buffer) const throw()
{
	memcpy(buffer, "OggS", 4);
	buffer[4] = version;
	buffer[5] = (flag_continue ? 1 : 0) | (flag_bos ? 2 : 0) | (flag_eos ? 4 : 0);
	write64ule(buffer + 6, granulepos);
	write32ule(buffer + 14, stream);
	write32ule(buffer + 18, sequence);
	write32ule(buffer + 22, 0);	//CRC will be fixed later.
	buffer[26] = segment_count;
	memcpy(buffer + 27, segments, segment_count);
	memcpy(buffer + 27 + segment_count, data, data_count);
	size_t plen = 27 + segment_count + data_count;
	//Fix the CRC.
	write32ule(buffer + 22, ogg_crc32(ogg_crc32(0, NULL, 0), reinterpret_cast<uint8_t*>(buffer), plen));
}

const uint64_t ogg_page::granulepos_none = 0xFFFFFFFFFFFFFFFFULL;


struct oggopus_header parse_oggopus_header(struct ogg_page& page) throw(std::runtime_error)
{
	struct oggopus_header h;
	if(page.get_packet_count() != 1 || page.get_last_packet_incomplete() || page.get_continue())
		throw std::runtime_error("OggOpus header page must have one complete packet");
	if(!page.get_bos() || page.get_eos())
		throw std::runtime_error("OggOpus header page must be first but not last page");
	auto p = page.get_packet(0);
	if(p.second < 8 || memcmp(p.first, "OpusHead", 8))
		throw std::runtime_error("Bad OggOpus header magic");
	if(p.second < 19 || (p.first[18] && p.second < 21U + p.first[5]))
		throw std::runtime_error("OggOpus header packet truncated");
	if(!p.first[9])
		throw std::runtime_error("Zero channels not allowed");
	if(p.first[8] & 0xF0)
		throw std::runtime_error("Unsupported OggOpus version");
	h.version = p.first[8];
	h.channels = p.first[9];
	h.preskip = read16ule(p.first + 10);
	h.rate = read32ule(p.first + 12);
	h.gain = read16sle(p.first + 16);
	h.map_family = p.first[18];
	memset(h.chanmap, 255, sizeof(h.chanmap));
	if(h.map_family) {
		h.streams = p.first[19];
		h.coupled = p.first[20];
		if(h.coupled > h.streams)
			throw std::runtime_error("More coupled streams than total streams.");
		if(static_cast<int>(h.streams) > 255 - h.coupled)
			throw std::runtime_error("Maximum of 255 physical channels exceeded");
		memcpy(h.chanmap, p.first + 21, h.channels);
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

struct oggopus_tags parse_oggopus_tags(struct ogg_page& page) throw(std::bad_alloc, std::runtime_error)
{
	struct oggopus_tags h;
	if(page.get_packet_count() != 1 || page.get_last_packet_incomplete() || page.get_continue())
		throw std::runtime_error("OggOpus tags page must have one complete packet");
	if(page.get_bos())
		throw std::runtime_error("OggOpus tags page must not be first page");
	auto p = page.get_packet(0);
	if(p.second < 8 || memcmp(p.first, "OpusTags", 8))
		throw std::runtime_error("Bad OggOpus tags magic");
	if(p.second < 12)
		throw std::runtime_error("OggOpus header packet truncated");
	//Scan the thing.
	size_t itr = 8;
	size_t oitr = 8;
	itr = itr + 4 + read32ule(p.first + itr);
	if(itr + 4 > p.second)
		throw std::runtime_error("OggOpus header packet truncated");
	h.vendor = std::string(p.first + oitr + 4, p.first + itr);
	oitr = itr;
	uint32_t headers = read32ule(p.first + itr);
	itr += 4;
	for(uint32_t i = 0; i < headers; i++) {
		if(itr + 4 > p.second)
			throw std::runtime_error("OggOpus header packet truncated");
		itr = itr + 4 + read32ule(p.first + itr);
		h.comments.push_back(std::string(p.first + oitr + 4, p.first + itr));
		oitr = itr;
	}
	if(itr > p.second)
		throw std::runtime_error("OggOpus header packet truncated");
	return h;
}

struct ogg_page serialize_oggopus_header(struct oggopus_header& header) throw(std::runtime_error)
{
	struct ogg_page page;
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
	write64ube(buffer, 0x4F70757348656164ULL);
	buffer[8] = header.version;
	buffer[9] = header.channels;
	write16ule(buffer + 10, header.preskip);
	write32ule(buffer + 12, header.rate);
	write16sle(buffer + 16, header.gain);
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

struct ogg_page serialize_oggopus_tags(struct oggopus_tags& tags) throw(std::runtime_error)
{
	struct ogg_page page;
	size_t needed = 8;
	bool toolarge = false;
	toolarge |= (tags.vendor.length() > 65016);
	needed += tags.vendor.length();
	toolarge |= (tags.comments.size() > 16254);
	for(auto i : tags.comments) {
		toolarge |= (tags.comments.size() > 65016);
		needed += (i.length() + 4);
	}
	if(needed > 65016 || toolarge)
		throw std::runtime_error("Set of comments too large");
	uint8_t buffer[65024];
	size_t itr = 0;
	write64ube(buffer, 0x4F70757354616773ULL);
	write32ule(buffer + 8, tags.vendor.length());
	std::copy(tags.vendor.begin(), tags.vendor.end(), reinterpret_cast<char*>(buffer + 12));
	itr = 12 + tags.vendor.length();
	write32ule(buffer + itr, tags.comments.size());
	itr += 4;
	for(auto i : tags.comments) {
		write32ule(buffer + itr, i.length());
		std::copy(i.begin(), i.end(), reinterpret_cast<char*>(buffer + itr + 4));
		itr += (i.length() + 4);
	}
	if(!page.append_packet(buffer, itr))
		throw std::runtime_error("Comment packet too large");
	page.set_granulepos(0);
	page.set_sequence(1);
	return page;
}

ogg_stream_reader::ogg_stream_reader() throw()
{
	eof = false;
	left = 0;
	errors_to = &std::cerr;
}

ogg_stream_reader::~ogg_stream_reader() throw()
{
}

void ogg_stream_reader::set_errors_to(std::ostream& os)
{
	errors_to = &os;
}

bool ogg_stream_reader::get_page(ogg_page& page) throw(std::exception)
{
	size_t advance;
	bool f;
try_again:
	fill_buffer();
	if(eof && !left)
		return false;
	f = ogg_page::scan(buffer, left, eof, advance);
	if(advance) {
		//The ogg stream resyncs.
		(*errors_to) << "Warning: Ogg stream: Recapture after " << advance << " bytes." << std::endl;
		discard_buffer(advance);
		goto try_again;
	}
	if(!f)
		goto try_again;
	page = ogg_page(buffer, advance);
	discard_buffer(advance);
	return true;
}

void ogg_stream_reader::fill_buffer()
{
	size_t r;
	if(!eof && left < sizeof(buffer)) {
		left += (r = read(buffer + left, sizeof(buffer) - left));
		if(!r)
			eof = true;
	}
}

void ogg_stream_reader::discard_buffer(size_t amount)
{
	if(amount < left)
		memmove(buffer, buffer + amount, left - amount);
	left -= amount;
}

ogg_stream_writer::ogg_stream_writer() throw()
{
}

ogg_stream_writer::~ogg_stream_writer() throw()
{
}

void ogg_stream_writer::put_page(const ogg_page& page) throw(std::exception)
{
	char buffer[65536];
	size_t s = page.serialize_size();
	page.serialize(buffer);
	write(buffer, s);
}

ogg_stream_reader_iostreams::ogg_stream_reader_iostreams(std::istream& stream)
	: is(stream)
{
}

ogg_stream_reader_iostreams::~ogg_stream_reader_iostreams() throw()
{
}

size_t ogg_stream_reader_iostreams::read(char* buffer, size_t size) throw(std::exception)
{
	if(!is)
		return 0;
	is.read(buffer, size);
	return is.gcount();
}

ogg_stream_writer_iostreams::ogg_stream_writer_iostreams(std::ostream& stream)
	: os(stream)
{
}

ogg_stream_writer_iostreams::~ogg_stream_writer_iostreams() throw()
{
}

void ogg_stream_writer_iostreams::write(const char* buffer, size_t size) throw(std::exception)
{
	if(!os)
		throw std::runtime_error("Error writing data");
	os.write(buffer, size);
	if(!os)
		throw std::runtime_error("Error writing data");
}
