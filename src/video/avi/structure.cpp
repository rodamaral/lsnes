#include "video/avi/structure.hpp"
#include "library/serialization.hpp"
#include <cstring>

stream_format_base::~stream_format_base() {}
stream_format_video::~stream_format_video() {}
stream_format_audio::~stream_format_audio() {}


uint32_t stream_format_video::type() { return 0x73646976; }
uint32_t stream_format_video::scale() { return fps_d; }
uint32_t stream_format_video::rate() { return fps_n; }
uint32_t stream_format_video::rect_left() { return 0; }
uint32_t stream_format_video::rect_top() { return 0; }
uint32_t stream_format_video::rect_right() { return width; }
uint32_t stream_format_video::rect_bottom() { return height; }
uint32_t stream_format_video::sample_size() { return (bit_count + 7) / 8; }
size_t stream_format_video::size() { return 48 + extra.size(); }
void stream_format_video::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(size());
	serialization::u32l(&buf[0], 0x66727473UL);	//Type
	serialization::u32l(&buf[4], size() - 8);	//Size.
	serialization::u32l(&buf[8], 40 + extra.size());	//BITMAPINFOHEADER size.
	serialization::u32l(&buf[12], width);
	serialization::u32l(&buf[16], height);
	serialization::u16l(&buf[20], planes);
	serialization::u16l(&buf[22], bit_count);
	serialization::u32l(&buf[24], compression);
	serialization::u32l(&buf[28], size_image);
	serialization::u32l(&buf[32], resolution_x);
	serialization::u32l(&buf[36], resolution_y);
	serialization::u32l(&buf[40], clr_used);
	serialization::u32l(&buf[44], clr_important);
	memcpy(&buf[48], &extra[0], extra.size());
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write strf (video)");
}

uint32_t stream_format_audio::type() { return 0x73647561; }
uint32_t stream_format_audio::scale() { return 1; }
uint32_t stream_format_audio::rate() { return samples_per_second; }
uint32_t stream_format_audio::rect_left() { return 0; }
uint32_t stream_format_audio::rect_top() { return 0; }
uint32_t stream_format_audio::rect_right() { return 0; }
uint32_t stream_format_audio::rect_bottom() { return 0; }
uint32_t stream_format_audio::sample_size() { return blocksize; }
size_t stream_format_audio::size() { return (29 + extra.size()) / 4 * 4; }

void stream_format_audio::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(size());
	serialization::u32l(&buf[0], 0x66727473UL);	//Type
	serialization::u32l(&buf[4], size() - 8);	//Size.
	serialization::u16l(&buf[8], format_tag);
	serialization::u16l(&buf[10], channels);
	serialization::u32l(&buf[12], samples_per_second);
	serialization::u32l(&buf[16], average_bytes_per_second);
	serialization::u16l(&buf[20], block_align);
	serialization::u16l(&buf[22], bits_per_sample);
	serialization::u16l(&buf[24], extra.size());	//Extension data.
	memset(&buf[26], 0, size() - 26);	//Pad
	memcpy(&buf[26], &extra[0], extra.size());
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write strf (audio)");
}

size_t stream_header::size() { return 72; }
stream_header::stream_header() { length = 0; }
void stream_header::reset() { length = 0; }
void stream_header::add_frames(size_t count) { length = length + count; }

void stream_header::serialize(std::ostream& out, struct stream_format_base& format)
{
	std::vector<char> buf;
	buf.resize(size());
	serialization::u32l(&buf[0], 0x68727473UL);	//Type
	serialization::u32l(&buf[4], size() - 8);	//Size.
	serialization::u32l(&buf[8], format.type());
	serialization::u32l(&buf[12], handler);
	serialization::u32l(&buf[16], flags);
	serialization::u16l(&buf[20], priority);
	serialization::u16l(&buf[22], language);
	serialization::u32l(&buf[24], initial_frames);
	serialization::u32l(&buf[28], format.scale());
	serialization::u32l(&buf[32], format.rate());
	serialization::u32l(&buf[36], start);
	serialization::u32l(&buf[40], length);
	serialization::u32l(&buf[44], suggested_buffer_size);
	serialization::u32l(&buf[48], quality);
	serialization::u32l(&buf[52], format.sample_size());
	serialization::u32l(&buf[56], format.rect_left());
	serialization::u32l(&buf[60], format.rect_top());
	serialization::u32l(&buf[64], format.rect_right());
	serialization::u32l(&buf[68], format.rect_bottom());
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write strh");
}

template<class format>
size_t stream_header_list<format>::size() { return 12 + strh.size() + strf.size(); }

template<class format>
void stream_header_list<format>::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(12);
	serialization::u32l(&buf[0], 0x5453494CUL);		//List.
	serialization::u32l(&buf[4], size() - 8);
	serialization::u32l(&buf[8], 0x6c727473UL);		//Type.
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write strl");
	strh.serialize(out, strf);
	strf.serialize(out);
}

template<class format>
void stream_header_list<format>::reset() { strh.reset(); }

size_t avi_header::size() { return 64; }
void avi_header::serialize(std::ostream& out, stream_header_list<stream_format_video>& videotrack, uint32_t tracks)
{
	std::vector<char> buf;
	buf.resize(size());
	serialization::u32l(&buf[0], 0x68697661);	//Type.
	serialization::u32l(&buf[4], size() - 8);
	serialization::u32l(&buf[8], microsec_per_frame);
	serialization::u32l(&buf[12], max_bytes_per_sec);
	serialization::u32l(&buf[16], padding_granularity);
	serialization::u32l(&buf[20], flags);
	serialization::u32l(&buf[24], videotrack.strh.length);
	serialization::u32l(&buf[28], initial_frames);
	serialization::u32l(&buf[32], tracks);
	serialization::u32l(&buf[36], suggested_buffer_size);
	serialization::u32l(&buf[40], videotrack.strf.width);
	serialization::u32l(&buf[44], videotrack.strf.height);
	serialization::u32l(&buf[48], 0);
	serialization::u32l(&buf[52], 0);
	serialization::u32l(&buf[56], 0);
	serialization::u32l(&buf[60], 0);
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write avih");
}

size_t header_list::size() { return 12 + avih.size() + videotrack.size() + audiotrack.size(); }
void header_list::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(12);
	serialization::u32l(&buf[0], 0x5453494CUL);		//List.
	serialization::u32l(&buf[4], size() - 8);
	serialization::u32l(&buf[8], 0x6c726468UL);		//Type.
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write hdrl");
	avih.serialize(out, videotrack, 2);
	videotrack.serialize(out);
	audiotrack.serialize(out);
}

void header_list::reset()
{
	audiotrack.reset();
	videotrack.reset();
}

size_t movi_chunk::write_offset() { return 12; }
size_t movi_chunk::size() { return 12 + payload_size; }
movi_chunk::movi_chunk() { payload_size = 0; }
void movi_chunk::reset() { payload_size = 0; }
void movi_chunk::add_payload(size_t s) { payload_size = payload_size + s; }
void movi_chunk::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(12);
	serialization::u32l(&buf[0], 0x5453494CUL);		//List.
	serialization::u32l(&buf[4], size() - 8);
	serialization::u32l(&buf[8], 0x69766f6d);	//Type.
	out.write(&buf[0], buf.size());
	out.seekp(payload_size, std::ios_base::cur);
	if(!out)
		throw std::runtime_error("Can't write movi");
}

size_t index_entry::size() { return 16; }
index_entry::index_entry(uint32_t _chunk_type, uint32_t _flags, uint32_t _offset, uint32_t _length)
{
	chunk_type = _chunk_type;
	flags = _flags;
	offset = _offset;
	length = _length;
}

void index_entry::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(16);
	serialization::u32l(&buf[0], chunk_type);
	serialization::u32l(&buf[4], flags);
	serialization::u32l(&buf[8], offset);
	serialization::u32l(&buf[12], length);
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write index entry");
}

void idx1_chunk::add_entry(const index_entry& entry) { entries.push_back(entry); }
size_t idx1_chunk::size()
{
	size_t s = 8;
	//Not exactly right, but much faster than the proper way.
	if(entries.empty())
		return s;
	s = s + entries.begin()->size() * entries.size();
	return s;
}

void idx1_chunk::serialize(std::ostream& out)
{
	std::vector<char> buf;
	buf.resize(8);
	serialization::u32l(&buf[0], 0x31786469UL);	//Type.
	serialization::u32l(&buf[4], size() - 8);
	out.write(&buf[0], buf.size());
	if(!out)
		throw std::runtime_error("Can't write idx1");
	for(std::list<index_entry>::iterator i = entries.begin(); i != entries.end(); i++)
		i->serialize(out);
}

void idx1_chunk::reset()
{
	entries.clear();
}

size_t avi_file_structure::write_offset() { return 12 + hdrl.size() + movi.write_offset(); }
size_t avi_file_structure::size() { return 12 + hdrl.size() + movi.size() + idx1.size(); }
void avi_file_structure::serialize()
{
	std::vector<char> buf;
	buf.resize(12);
	serialization::u32l(&buf[0], 0x46464952UL);		//RIFF.
	serialization::u32l(&buf[4], size() - 8);
	serialization::u32l(&buf[8], 0x20495641UL);		//Type.
	outstream->write(&buf[0], buf.size());
	if(!*outstream)
		throw std::runtime_error("Can't write AVI header");
	hdrl.serialize(*outstream);
	movi.serialize(*outstream);
	idx1.serialize(*outstream);
}

void avi_file_structure::start_data(std::ostream& out)
{
	movi.reset();
	idx1.reset();
	out.seekp(0, std::ios_base::beg);
	size_t reserved_for_header = write_offset();
	std::vector<char> tmp;
	tmp.resize(reserved_for_header);
	out.write(&tmp[0], tmp.size());
	if(!out)
		throw std::runtime_error("Can't write dummy header");
	outstream = &out;
}

void avi_file_structure::finish_avi()
{
	outstream->seekp(0, std::ios_base::beg);
	serialize();
	if(!*outstream)
		throw std::runtime_error("Can't finish AVI");
}
