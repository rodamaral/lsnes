#include "video/avi/codec.hpp"
#include "core/advdumper.hpp"
#include "core/misc.hpp"
#include "library/serialization.hpp"

avi_video_codec::~avi_video_codec() {};
avi_audio_codec::~avi_audio_codec() {};

avi_video_codec::format::format(uint32_t _width, uint32_t _height, uint32_t _compression, uint16_t bitcount)
{
	width = _width;
	height = _height;
	suggested_buffer_size = 1000000;
	max_bytes_per_sec = 10000000;
	planes = 1;
	bit_count = bitcount;
	compression = _compression;
	resolution_x = 4000;
	resolution_y = 4000;
	quality = 9999;
	clr_used = 0;
	clr_important = 0;
}

void avi_video_codec::send_performance_counters(uint64_t b, uint64_t w)
{
}

avi_audio_codec::format::format(uint16_t tag)
{
	max_bytes_per_sec = 200000;
	suggested_buffer_size = 16384;
	format_tag = tag;
	average_rate = 176400;
	alignment = 4;
	bitdepth = 16;
	quality = 9999;
}


uint32_t get_actual_packet_type(uint8_t trackid, uint16_t typecode)
{
	uint32_t t1 = trackid / 10 + 48;
	uint32_t t2 = trackid % 10 + 48;
	return t1 | t2 << 8 | static_cast<uint32_t>(typecode) << 16;
}

#define PADGRANULARITY 2


namespace
{
	void write_pkt(struct avi_file_structure& avifile, const avi_packet& pkt, uint8_t track)
	{
		uint32_t fulltype = get_actual_packet_type(track, pkt.typecode);
		char buf[8 + PADGRANULARITY];
		serialization::u32l(buf + 0, fulltype);
		serialization::u32l(buf + 4, pkt.payload.size());
		serialization::u16l(buf + 8, 0);
		size_t padding = (PADGRANULARITY - pkt.payload.size() % PADGRANULARITY) % PADGRANULARITY;
		avifile.outstream->write(buf, 8);
		avifile.outstream->write(&pkt.payload[0], pkt.payload.size());
		avifile.outstream->write(buf + 8, padding);
		if(!*avifile.outstream)
			throw std::runtime_error("Can't write AVI packet");
		if(!pkt.hidden)
			avifile.idx1.add_entry(index_entry(fulltype, pkt.indexflags, avifile.movi.payload_size + 4,
				pkt.payload.size()));
		avifile.movi.payload_size += (pkt.payload.size() + 8 + padding);
	}
}

avi_output_stream::avi_output_stream()
	: video_timer(60), audio_timer(60)
{
	in_segment = false;
}

avi_output_stream::~avi_output_stream()
{
	try {
		if(in_segment)
			end();
	} catch(...) {
	}
}

void avi_output_stream::start(std::ostream& out, avi_video_codec& _vcodec, avi_audio_codec& _acodec, uint32_t width,
	uint32_t height, uint32_t fps_n, uint32_t fps_d, uint32_t samplerate, uint16_t channels)
{
	if(in_segment)
		end();
	in_segment = false;

	avi_audio_codec::format afmt = _acodec.reset(samplerate, channels);
	avi_video_codec::format vfmt = _vcodec.reset(width, height, fps_n, fps_d);

	header_list avih;
	avih.avih.microsec_per_frame = (uint64_t)1000000 * fps_d / fps_n;
	avih.avih.max_bytes_per_sec = afmt.max_bytes_per_sec + vfmt.max_bytes_per_sec;
	avih.avih.padding_granularity = 0;
	avih.avih.flags = 2064;		//Trust chunk types, has index.
	avih.avih.initial_frames = 0;
	avih.avih.suggested_buffer_size = 1048576;	//Just some value.
	avih.videotrack.strh.handler = 0;
	avih.videotrack.strh.flags = 0;
	avih.videotrack.strh.priority = 0;
	avih.videotrack.strh.language = 0;
	avih.videotrack.strh.initial_frames = 0;
	avih.videotrack.strh.start = 0;
	avih.videotrack.strh.suggested_buffer_size = vfmt.suggested_buffer_size;
	avih.videotrack.strh.quality = vfmt.quality;
	avih.videotrack.strf.width = vfmt.width;
	avih.videotrack.strf.height = vfmt.height;
	avih.videotrack.strf.planes = vfmt.planes;
	avih.videotrack.strf.bit_count = vfmt.bit_count;
	avih.videotrack.strf.compression = vfmt.compression;
	avih.videotrack.strf.size_image = (vfmt.bit_count + 7) / 8 * width * height;
	avih.videotrack.strf.resolution_x = vfmt.resolution_x;
	avih.videotrack.strf.resolution_y = vfmt.resolution_y;
	avih.videotrack.strf.clr_used = vfmt.clr_used;
	avih.videotrack.strf.clr_important = vfmt.clr_important;
	avih.videotrack.strf.fps_n = fps_n;
	avih.videotrack.strf.fps_d = fps_d;
	avih.audiotrack.strh.handler = 0;
	avih.audiotrack.strh.flags = 0;
	avih.audiotrack.strh.priority = 0;
	avih.audiotrack.strh.language = 0;
	avih.audiotrack.strh.initial_frames = 0;
	avih.audiotrack.strh.start = 0;
	avih.audiotrack.strh.suggested_buffer_size = afmt.suggested_buffer_size;
	avih.audiotrack.strh.quality = afmt.quality;
	avih.audiotrack.strf.format_tag = afmt.format_tag;
	avih.audiotrack.strf.channels = channels;
	avih.audiotrack.strf.samples_per_second = samplerate;
	avih.audiotrack.strf.average_bytes_per_second = afmt.average_rate;
	avih.audiotrack.strf.block_align = afmt.alignment;
	avih.audiotrack.strf.bits_per_sample = afmt.bitdepth;
	avih.audiotrack.strf.blocksize = channels * ((afmt.bitdepth + 7) / 8);

	avifile.hdrl = avih;
	avifile.start_data(out);
	acodec = &_acodec;
	vcodec = &_vcodec;
	achans = channels;
	video_timer.rate(fps_n, fps_d);
	audio_timer.rate(samplerate);

	while(!vcodec->ready())
		write_pkt(avifile, vcodec->getpacket(), 0);
	while(!acodec->ready())
		write_pkt(avifile, acodec->getpacket(), 1);
	in_segment = true;
}

void avi_output_stream::frame(uint32_t* frame, uint32_t stride)
{
	if(!in_segment)
		throw std::runtime_error("Trying to write to non-open AVI");
	vcodec->frame(frame, stride);
	while(!vcodec->ready())
		write_pkt(avifile, vcodec->getpacket(), 0);
	avifile.hdrl.videotrack.strh.add_frames(1);
}

void avi_output_stream::samples(int16_t* samples, size_t samplecount)
{
	if(!in_segment)
		throw std::runtime_error("Trying to write to non-open AVI");
	acodec->samples(samples, samplecount);
	while(!acodec->ready())
		write_pkt(avifile, acodec->getpacket(), 1);
	avifile.hdrl.audiotrack.strh.add_frames(samplecount);
	for(size_t i = 0; i < samplecount; i++)
		audio_timer.increment();
}

void avi_output_stream::flushaudio()
{
	if(!in_segment)
		throw std::runtime_error("Trying to write to non-open AVI");
	acodec->flush();
	while(!acodec->ready())
		write_pkt(avifile, acodec->getpacket(), 1);
}

void avi_output_stream::end()
{
	flushaudio();	//In case audio codec uses internal buffering...
	avifile.finish_avi();
	in_segment = false;
}

size_t avi_output_stream::framesamples()
{
	uint64_t next_frame_at = video_timer.read_next();
	timer tmp_audio_timer = audio_timer;
	size_t samples = 0;
	while(tmp_audio_timer.read() < next_frame_at) {
		tmp_audio_timer.increment();
		samples++;
	}
	return samples;
}

uint64_t avi_output_stream::get_size_estimate()
{
	if(!in_segment)
		return 0;
	return avifile.movi.payload_size;
}

bool avi_output_stream::readqueue(uint32_t* _frame, uint32_t* oframe, uint32_t stride, sample_queue& aqueue,
	bool force)
{
	if(!in_segment)
		throw std::runtime_error("Trying to write to non-open AVI");
	size_t fsamples = framesamples();
	if(!force && aqueue.available() < fsamples)
		return false;
	std::vector<int16_t> tmp;
	tmp.resize(fsamples * achans);
	aqueue.pull(&tmp[0], tmp.size());
	frame(_frame, stride);
	video_timer.increment();
	samples(&tmp[0], fsamples);
	delete[] oframe;
	return true;
}

void avi_audio_codec::flush()
{
	//Do nothing.
}

template<typename T>
avi_codec_type<T>::avi_codec_type(const char* _iname, const char* _hname, T* (*_instance)())
{
	iname = _iname;
	hname = _hname;
	instance = _instance;
	codecs()[iname] = this;
	//Make UI rereread available dumpers.
	if(!in_global_ctors())
		dumper_factory_base::run_notify();
}

template<typename T>
avi_codec_type<T>::~avi_codec_type()
{
	codecs().erase(iname);
	//Make UI rereread available dumpers.
	if(!in_global_ctors())
		dumper_factory_base::run_notify();
}

template<typename T>
avi_codec_type<T>* avi_codec_type<T>::find(const std::string& iname)
{
	if(!codecs().count(iname))
		return NULL;
	return codecs()[iname];
}

template<typename T>
avi_codec_type<T>* avi_codec_type<T>::find_next(avi_codec_type<T>* type)
{
	typename std::map<std::string, avi_codec_type<T>*>::iterator i;
	if(!type)
		i = codecs().lower_bound("");
	else
		i = codecs().upper_bound(type->iname);
	if(i == codecs().end())
		return NULL;
	else
		return i->second;
}

template<typename T> std::string avi_codec_type<T>::get_iname() { return iname; }
template<typename T> std::string avi_codec_type<T>::get_hname() { return hname; }
template<typename T> T* avi_codec_type<T>::get_instance() { return instance(); }

template<typename T> std::map<std::string, avi_codec_type<T>*>& avi_codec_type<T>::codecs()
{
	static std::map<std::string, avi_codec_type<T>*> x;
	return x;
}

template struct avi_codec_type<avi_video_codec>;
template struct avi_codec_type<avi_audio_codec>;
