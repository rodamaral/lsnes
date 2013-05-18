#ifdef WITH_OPUS_CODEC
#define OPUS_BUILD
#include "opus.hpp"
#include <opus.h>
#include <opus_defines.h>
#include <opus_multistream.h>
#include <sstream>
#include <cstring>

//Some of these might not be in stable.
#ifndef OPUS_SET_GAIN_REQUEST
#define OPUS_SET_GAIN_REQUEST 4034
#endif
#ifndef OPUS_GET_GAIN_REQUEST
#define OPUS_GET_GAIN_REQUEST 4045
#endif
#ifndef OPUS_SET_LSB_DEPTH_REQUEST
#define OPUS_SET_LSB_DEPTH_REQUEST 4036
#endif
#ifndef OPUS_GET_LSB_DEPTH_REQUEST
#define OPUS_GET_LSB_DEPTH_REQUEST 4037
#endif
#ifndef OPUS_GET_LAST_PACKET_DURATION_REQUEST
#define OPUS_GET_LAST_PACKET_DURATION_REQUEST 4039
#endif

namespace opus
{
samplerate samplerate::r8k(8000);
samplerate samplerate::r12k(12000);
samplerate samplerate::r16k(16000);
samplerate samplerate::r24k(24000);
samplerate samplerate::r48k(48000);
bitrate bitrate::_auto(OPUS_AUTO);
bitrate bitrate::max(OPUS_BITRATE_MAX);
vbr vbr::cbr(false);
vbr vbr::_vbr(true);
vbr_constraint vbr_constraint::unconstrained(false);
vbr_constraint vbr_constraint::constrained(true);
force_channels force_channels::_auto(OPUS_AUTO);
force_channels force_channels::mono(1);
force_channels force_channels::stereo(2);
max_bandwidth max_bandwidth::narrow(OPUS_BANDWIDTH_NARROWBAND);
max_bandwidth max_bandwidth::medium(OPUS_BANDWIDTH_MEDIUMBAND);
max_bandwidth max_bandwidth::wide(OPUS_BANDWIDTH_WIDEBAND);
max_bandwidth max_bandwidth::superwide(OPUS_BANDWIDTH_SUPERWIDEBAND);
max_bandwidth max_bandwidth::full(OPUS_BANDWIDTH_FULLBAND);
bandwidth bandwidth::_auto(OPUS_AUTO);
bandwidth bandwidth::narrow(OPUS_BANDWIDTH_NARROWBAND);
bandwidth bandwidth::medium(OPUS_BANDWIDTH_MEDIUMBAND);
bandwidth bandwidth::wide(OPUS_BANDWIDTH_WIDEBAND);
bandwidth bandwidth::superwide(OPUS_BANDWIDTH_SUPERWIDEBAND);
bandwidth bandwidth::full(OPUS_BANDWIDTH_FULLBAND);
signal signal::_auto(OPUS_AUTO);
signal signal::music(OPUS_SIGNAL_MUSIC);
signal signal::voice(OPUS_SIGNAL_VOICE);
application application::audio(OPUS_APPLICATION_AUDIO);
application application::voice(OPUS_APPLICATION_VOIP);
application application::lowdelay(OPUS_APPLICATION_RESTRICTED_LOWDELAY);
fec fec::disabled(false);
fec fec::enabled(true);
dtx dtx::disabled(false);
dtx dtx::enabled(true);
lsbdepth lsbdepth::d8(8);
lsbdepth lsbdepth::d16(16);
lsbdepth lsbdepth::d24(24);
_reset reset;
_finalrange finalrange;
_pitch pitch;
_pktduration pktduration;
_lookahead lookahead;

bad_argument::bad_argument()
	: std::runtime_error("Invalid argument") {}

buffer_too_small::buffer_too_small()
	: std::runtime_error("Buffer too small") {}

internal_error::internal_error()
	: std::runtime_error("Internal error") {}

invalid_packet::invalid_packet()
	: std::runtime_error("Invalid packet") {}

unimplemented::unimplemented()
	: std::runtime_error("Not implemented") {}

invalid_state::invalid_state()
	: std::runtime_error("Invalid state") {}

const unsigned f1_streams[] = {0, 1, 1, 2, 2, 3, 4, 4, 5};
const unsigned f1_coupled[] = {0, 0, 1, 1, 2, 2, 2, 3, 3};
const char* chanmap[] = {"", "A", "AB", "ACB", "ABCD", "ACDEB", "ACDEBF", "ACBFDEG", "ACDEFGBH"};

void lookup_params(unsigned channels, unsigned family, int& streams, int& coupled)
{
	if(channels == 0)
		throw bad_argument();
	if(family == 0) {
		if(channels > 2)
			throw unimplemented();
		streams = 1;
		coupled = (channels == 2) ? 1 : 0;
	} else if(family == 1) {
		if(channels > 8)
			throw unimplemented();
		streams = f1_streams[channels];
		coupled = f1_coupled[channels];
	} else
		throw unimplemented();
}

void generate_mapping(unsigned channels, unsigned family, unsigned char* mapping)
{
	for(unsigned i = 0; i < channels; i++)
		mapping[i] = chanmap[channels][i] - 'A';
}

char* alignptr(char* ptr, size_t align)
{
	while((intptr_t)ptr % align)
		ptr++;
	return ptr;
}

int32_t throwex(int32_t ret)
{
	if(ret >= 0)
		return ret;
	if(ret == OPUS_BAD_ARG)
		throw bad_argument();
	if(ret == OPUS_BUFFER_TOO_SMALL)
		throw buffer_too_small();
	if(ret == OPUS_INTERNAL_ERROR)
		throw internal_error();
	if(ret == OPUS_INVALID_PACKET)
		throw invalid_packet();
	if(ret == OPUS_UNIMPLEMENTED)
		throw unimplemented();
	if(ret == OPUS_INVALID_STATE)
		throw invalid_state();
	if(ret == OPUS_ALLOC_FAIL)
		throw std::bad_alloc();
	std::ostringstream s;
	s << "Unknown error code " << ret << " from libopus.";
	throw std::runtime_error(s.str());
}

template<typename T> struct get_ctlnum { const static int32_t num; static T errordefault(); };

template<> const int32_t get_ctlnum<complexity>::num = OPUS_GET_COMPLEXITY_REQUEST;
template<> const int32_t get_ctlnum<bitrate>::num = OPUS_GET_BITRATE_REQUEST;
template<> const int32_t get_ctlnum<vbr>::num = OPUS_GET_VBR_REQUEST;
template<> const int32_t get_ctlnum<vbr_constraint>::num = OPUS_GET_VBR_CONSTRAINT_REQUEST;
template<> const int32_t get_ctlnum<force_channels>::num = OPUS_GET_FORCE_CHANNELS_REQUEST;
template<> const int32_t get_ctlnum<max_bandwidth>::num = OPUS_GET_MAX_BANDWIDTH_REQUEST;
template<> const int32_t get_ctlnum<bandwidth>::num = OPUS_GET_BANDWIDTH_REQUEST;
template<> const int32_t get_ctlnum<signal>::num = OPUS_GET_SIGNAL_REQUEST;
template<> const int32_t get_ctlnum<application>::num = OPUS_GET_APPLICATION_REQUEST;
template<> const int32_t get_ctlnum<fec>::num = OPUS_GET_INBAND_FEC_REQUEST;
template<> const int32_t get_ctlnum<lossperc>::num = OPUS_GET_PACKET_LOSS_PERC_REQUEST;
template<> const int32_t get_ctlnum<dtx>::num = OPUS_GET_DTX_REQUEST;
template<> const int32_t get_ctlnum<lsbdepth>::num = OPUS_GET_LSB_DEPTH_REQUEST;
template<> const int32_t get_ctlnum<gain>::num = OPUS_GET_GAIN_REQUEST;

template<> complexity get_ctlnum<complexity>::errordefault() { return complexity(10); }
template<> bitrate get_ctlnum<bitrate>::errordefault() { return bitrate::_auto; }
template<> vbr get_ctlnum<vbr>::errordefault() { return vbr::_vbr; }
template<> vbr_constraint get_ctlnum<vbr_constraint>::errordefault() { return vbr_constraint::unconstrained; }
template<> force_channels get_ctlnum<force_channels>::errordefault() { return force_channels::_auto; }
template<> max_bandwidth get_ctlnum<max_bandwidth>::errordefault() { return max_bandwidth::full; }
template<> bandwidth get_ctlnum<bandwidth>::errordefault() { return bandwidth::_auto; }
template<> signal get_ctlnum<signal>::errordefault() { return signal::_auto; }
template<> application get_ctlnum<application>::errordefault() { return application::audio; }
template<> fec get_ctlnum<fec>::errordefault() { return fec::disabled; }
template<> lossperc get_ctlnum<lossperc>::errordefault() { return lossperc(0); }
template<> dtx get_ctlnum<dtx>::errordefault() { return dtx::disabled; }
template<> lsbdepth get_ctlnum<lsbdepth>::errordefault() { return lsbdepth(24); }
template<> gain get_ctlnum<gain>::errordefault() { return gain(0); }


OpusEncoder* E(encoder& e) { return reinterpret_cast<OpusEncoder*>(e.getmem()); }
OpusDecoder* D(decoder& d) { return reinterpret_cast<OpusDecoder*>(d.getmem()); }
OpusRepacketizer* R(repacketizer& r) { return reinterpret_cast<OpusRepacketizer*>(r.getmem()); }
OpusMSEncoder* ME(multistream_encoder& e) { return reinterpret_cast<OpusMSEncoder*>(e.getmem()); }
OpusMSEncoder* ME(surround_encoder& e) { return reinterpret_cast<OpusMSEncoder*>(e.getmem()); }
OpusMSDecoder* MD(multistream_decoder& d) { return reinterpret_cast<OpusMSDecoder*>(d.getmem()); }

template<typename T> T generic_ctl(encoder& e, int32_t ctl)
{
	T val;
	throwex(opus_encoder_ctl(E(e), ctl, &val));
	return val;
}

template<typename T> T generic_ctl(encoder& e, int32_t ctl, T val)
{
	throwex(opus_encoder_ctl(E(e), ctl, val));
}

template<typename T> T generic_ctl(decoder& d, int32_t ctl)
{
	T val;
	throwex(opus_decoder_ctl(D(d), ctl, &val));
	return val;
}

template<typename T> T generic_ctl(decoder& d, int32_t ctl, T val)
{
	throwex(opus_decoder_ctl(D(d), ctl, val));
}

template<typename T> T generic_ctl(multistream_encoder& e, int32_t ctl, T val)
{
	throwex(opus_multistream_encoder_ctl(ME(e), ctl, val));
}

template<typename T> T generic_ctl(surround_encoder& e, int32_t ctl, T val)
{
	throwex(opus_multistream_encoder_ctl(ME(e), ctl, val));
}

template<typename T> T generic_ctl(multistream_encoder& e, int32_t ctl)
{
	T val;
	throwex(opus_multistream_encoder_ctl(ME(e), ctl, &val));
	return val;
}

template<typename T> T generic_ctl(surround_encoder& e, int32_t ctl)
{
	T val;
	throwex(opus_multistream_encoder_ctl(ME(e), ctl, &val));
	return val;
}


template<typename T> T generic_ctl(multistream_decoder& d, int32_t ctl, T val)
{
	throwex(opus_multistream_decoder_ctl(MD(d), ctl, val));
}

template<typename T> T generic_ctl(multistream_decoder& d, int32_t ctl)
{
	T val;
	throwex(opus_multistream_decoder_ctl(MD(d), ctl, &val));
	return val;
}


template<typename T, typename U> T do_generic_get(U& e)
{
	return T(generic_ctl<int32_t>(e, get_ctlnum<T>::num));
}

template<typename T> T generic_eget<T>::operator()(encoder& e) const
{
	return do_generic_get<T>(e);
}

template<typename T> T generic_meget<T>::operator()(multistream_encoder& e) const
{
	return do_generic_get<T>(e);
}

template<typename T> T generic_meget<T>::operator()(surround_encoder& e) const
{
	return do_generic_get<T>(e);
}

template<typename T> T generic_dget<T>::operator()(decoder& d) const
{
	return do_generic_get<T>(d);
}

template<typename T> T generic_mdget<T>::operator()(multistream_decoder& d) const
{
	return do_generic_get<T>(d);
}

template<typename T> T generic_get<T>::operator()(decoder& d) const
{
	return do_generic_get<T>(d);
}

template<typename T> T generic_get<T>::operator()(encoder& e) const
{
	return do_generic_get<T>(e);
}

template<typename T> T generic_mget<T>::operator()(multistream_encoder& e) const
{
	return do_generic_get<T>(e);
}

template<typename T> T generic_mget<T>::operator()(surround_encoder& e) const
{
	return do_generic_get<T>(e);
}

template<typename T> T generic_mget<T>::operator()(multistream_decoder& d) const
{
	return do_generic_get<T>(d);
}

template<typename T> T generic_eget<T>::errordefault() const
{
	return get_ctlnum<T>::errordefault();
}

template<typename T> T generic_dget<T>::errordefault() const
{
	return get_ctlnum<T>::errordefault();
}

template<typename T> T generic_get<T>::errordefault() const
{
	return get_ctlnum<T>::errordefault();
}

samplerate samplerate::operator()(encoder& e) const
{
	return samplerate(generic_ctl<int32_t>(e, OPUS_GET_SAMPLE_RATE_REQUEST));
}

void complexity::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_COMPLEXITY_REQUEST, c);
}

generic_meget<complexity> complexity::get;

void bitrate::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_BITRATE_REQUEST, b);
}

void bitrate::operator()(multistream_encoder& e) const
{
	generic_ctl(e, OPUS_SET_BITRATE_REQUEST, b);
}

void bitrate::operator()(surround_encoder& e) const
{
	generic_ctl(e, OPUS_SET_BITRATE_REQUEST, b);
}

generic_meget<bitrate> bitrate::get;

void vbr::operator()(encoder& e) const
{
	generic_ctl<int32_t>(e, OPUS_SET_VBR_REQUEST, (int32_t)(v ? 1 : 0));
}

generic_meget<vbr> vbr::get;

void vbr_constraint::operator()(encoder& e) const
{
	generic_ctl<int32_t>(e, OPUS_SET_VBR_CONSTRAINT_REQUEST, (int32_t)(c ? 1 : 0));
}

generic_meget<vbr_constraint> vbr_constraint::get;

void force_channels::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_FORCE_CHANNELS_REQUEST, f);
}

generic_meget<force_channels> force_channels::get;

void max_bandwidth::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_MAX_BANDWIDTH_REQUEST, bw);
}

generic_eget<max_bandwidth> max_bandwidth::get;

void bandwidth::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_BANDWIDTH_REQUEST, bw);
}

generic_mget<bandwidth> bandwidth::get;

void signal::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_SIGNAL_REQUEST, s);
}

generic_meget<signal> signal::get;

void application::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_APPLICATION_REQUEST, app);
}

generic_meget<application> application::get;

_lookahead _lookahead::operator()(encoder& e) const
{
	return _lookahead(generic_ctl<int32_t>(e, OPUS_GET_LOOKAHEAD_REQUEST));
}

void fec::operator()(encoder& e) const
{
	generic_ctl<int32_t>(e, OPUS_SET_INBAND_FEC_REQUEST, (int32_t)(f ? 1 : 0));
}

generic_meget<fec> fec::get;

void lossperc::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_PACKET_LOSS_PERC_REQUEST, (int32_t)loss);
}

generic_meget<lossperc> lossperc::get;

void dtx::operator()(encoder& e) const
{
	generic_ctl<int32_t>(e, OPUS_SET_DTX_REQUEST, (int32_t)(d ? 1 : 0));
}

generic_meget<dtx> dtx::get;

void lsbdepth::operator()(encoder& e) const
{
	generic_ctl(e, OPUS_SET_LSB_DEPTH_REQUEST, (int32_t)depth);
}

generic_meget<lsbdepth> lsbdepth::get;

_pktduration _pktduration::operator()(decoder& d) const
{
	return _pktduration(generic_ctl<int32_t>(d, OPUS_GET_LAST_PACKET_DURATION_REQUEST));
}

void _reset::operator()(decoder& d) const
{
	throwex(opus_decoder_ctl(D(d), OPUS_RESET_STATE));
}

void _reset::operator()(encoder& e) const
{
	throwex(opus_encoder_ctl(E(e), OPUS_RESET_STATE));
}

void _reset::operator()(multistream_decoder& d) const
{
	throwex(opus_multistream_decoder_ctl(MD(d), OPUS_RESET_STATE));
}

void _reset::operator()(multistream_encoder& e) const
{
	throwex(opus_multistream_encoder_ctl(ME(e), OPUS_RESET_STATE));
}

void _reset::operator()(surround_encoder& e) const
{
	throwex(opus_multistream_encoder_ctl(ME(e), OPUS_RESET_STATE));
}

_finalrange _finalrange::operator()(decoder& d) const
{
	return _finalrange(generic_ctl<uint32_t>(d, OPUS_GET_FINAL_RANGE_REQUEST));
}

_finalrange _finalrange::operator()(encoder& e) const
{
	return _finalrange(generic_ctl<uint32_t>(e, OPUS_GET_FINAL_RANGE_REQUEST));
}

_pitch _pitch::operator()(encoder& e) const
{
	return _pitch(generic_ctl<uint32_t>(e, OPUS_GET_PITCH_REQUEST));
}

_pitch _pitch::operator()(decoder& d) const
{
	return _pitch(generic_ctl<uint32_t>(d, OPUS_GET_PITCH_REQUEST));
}

void gain::operator()(decoder& d) const
{
	generic_ctl(d, OPUS_SET_GAIN_REQUEST, g);
}

generic_mdget<gain> gain::get;

void set_control_int::operator()(encoder& e) const
{
	generic_ctl(e, ctl, val);
}

void set_control_int::operator()(decoder& d) const
{
	generic_ctl(d, ctl, val);
}

void set_control_int::operator()(multistream_encoder& e) const
{
	generic_ctl(e, ctl, val);
}

void set_control_int::operator()(surround_encoder& e) const
{
	generic_ctl(e, ctl, val);
}

void set_control_int::operator()(multistream_decoder& d) const
{
	generic_ctl(d, ctl, val);
}

int32_t get_control_int::operator()(encoder& e) const
{
	return generic_ctl<int32_t>(e, ctl);
}

int32_t get_control_int::operator()(decoder& d) const
{
	return generic_ctl<int32_t>(d, ctl);
}

int32_t get_control_int::operator()(multistream_encoder& e) const
{
	return generic_ctl<int32_t>(e, ctl);
}

int32_t get_control_int::operator()(surround_encoder& e) const
{
	return generic_ctl<int32_t>(e, ctl);
}

int32_t get_control_int::operator()(multistream_decoder& d) const
{
	return generic_ctl<int32_t>(d, ctl);
}

void force_instantiate()
{
	surround_encoder::stream_format sf;
	encoder e(samplerate::r48k, true, application::audio);
	decoder d(samplerate::r48k, true);
	multistream_encoder me(samplerate::r48k, 1, 1, 0, NULL, application::audio);
	multistream_decoder md(samplerate::r48k, 1, 1, 0, NULL);
	surround_encoder se(samplerate::r48k, 1, 0, application::audio, sf);

	complexity::get(e);
	bitrate::get(e);
	vbr::get(e);
	vbr_constraint::get(e);
	force_channels::get(e);
	max_bandwidth::get(e);
	bandwidth::get(e);
	bandwidth::get(d);
	signal::get(e);
	application::get(e);
	fec::get(e);
	lossperc::get(e);
	dtx::get(e);
	lsbdepth::get(e);
	gain::get(d);
	bitrate::get(me);
	lsbdepth::get(me);
	vbr::get(me);
	vbr_constraint::get(me);
	application::get(me);
	bandwidth::get(me);
	bandwidth::get(md);
	complexity::get(me);
	lossperc::get(me);
	dtx::get(me);
	signal::get(me);
	fec::get(me);
	force_channels::get(me);
	bitrate::get(se);
	lsbdepth::get(se);
	vbr::get(se);
	vbr_constraint::get(se);
	application::get(se);
	bandwidth::get(se);
	complexity::get(se);
	lossperc::get(se);
	dtx::get(se);
	signal::get(se);
	fec::get(se);
	force_channels::get(se);

	complexity::get.errordefault();
	bitrate::get.errordefault();
	vbr::get.errordefault();
	vbr_constraint::get.errordefault();
	force_channels::get.errordefault();
	max_bandwidth::get.errordefault();
	bandwidth::get.errordefault();
	signal::get.errordefault();
	application::get.errordefault();
	fec::get.errordefault();
	lossperc::get.errordefault();
	dtx::get.errordefault();
	lsbdepth::get.errordefault();
	gain::get.errordefault();

	d.ctl(opus::pktduration);
	//d.ctl_quiet(opus::pktduration);
	e.ctl_quiet(opus::reset);
	e.ctl_quiet(opus::finalrange);
	e.ctl_quiet(opus::signal::get);
}

encoder::~encoder()
{
	if(!user)
		delete[] reinterpret_cast<char*>(memory);
}

encoder& encoder::operator=(const encoder& e)
{
	if(stereo != e.stereo)
		throw std::runtime_error("Channel mismatch in assignment");
	size_t s = size(stereo);
	if(memory != e.memory)
		memcpy(memory, e.memory, s);
	return *this;
}

encoder::encoder(samplerate rate, bool _stereo, application app, char* _memory)
{
	size_t s = size(_stereo);
	memory = _memory ? _memory : new char[s];
	stereo = _stereo;
	user = (_memory != NULL);
	try {
		throwex(opus_encoder_init(E(*this), rate, _stereo ? 2 : 1, app));
	} catch(...) {
		if(!user)
			delete[] reinterpret_cast<char*>(memory);
		throw;
	}
}

encoder::encoder(const encoder& e)
{
	size_t s = size(e.stereo);
	memory = new char[s];
	memcpy(memory, e.memory, s);
	stereo = e.stereo;
	user = false;
}

encoder::encoder(const encoder& e, char* _memory)
{
	size_t s = size(e.stereo);
	memory = _memory;
	memcpy(memory, e.memory, s);
	stereo = e.stereo;
	user = true;
}

encoder::encoder(void* state, bool _stereo)
{
	memory = state;
	stereo = _stereo;
	user = true;
}

size_t encoder::size(bool stereo)
{
	return opus_encoder_get_size(stereo ? 2 : 1);
}

size_t encoder::encode(const int16_t* in, uint32_t inframes, unsigned char* out, uint32_t maxout)
{
	return throwex(opus_encode(E(*this), in, inframes, out, maxout));
}

size_t encoder::encode(const float* in, uint32_t inframes, unsigned char* out, uint32_t maxout)
{
	return throwex(opus_encode_float(E(*this), in, inframes, out, maxout));
}

decoder::~decoder()
{
	if(!user)
		delete[] reinterpret_cast<char*>(memory);
}

decoder::decoder(samplerate rate, bool _stereo, char* _memory)
{
	size_t s = size(_stereo);
	memory = _memory ? _memory : new char[s];
	stereo = _stereo;
	user = (_memory != NULL);
	try {
		throwex(opus_decoder_init(D(*this), rate, _stereo ? 2 : 1));
	} catch(...) {
		if(!user)
			delete[] reinterpret_cast<char*>(memory);
		throw;
	}
}

decoder& decoder::operator=(const decoder& d)
{
	if(stereo != d.stereo)
		throw std::runtime_error("Channel mismatch in assignment");
	size_t s = size(stereo);
	if(memory != d.memory)
		memcpy(memory, d.memory, s);
	return *this;
}

size_t decoder::size(bool stereo)
{
	return opus_decoder_get_size(stereo ? 2 : 1);
}

decoder::decoder(const decoder& d)
{
	size_t s = size(d.stereo);
	memory = new char[s];
	memcpy(memory, d.memory, s);
	stereo = d.stereo;
	user = false;
}

decoder::decoder(const decoder& d, char* _memory)
{
	size_t s = size(d.stereo);
	memory = _memory;
	memcpy(memory, d.memory, s);
	stereo = d.stereo;
	user = true;
}

decoder::decoder(void* state, bool _stereo)
{
	memory = state;
	stereo = _stereo;
	user = true;
}

size_t decoder::decode(const unsigned char* in, uint32_t insize, int16_t* out, uint32_t maxframes, bool decode_fec)
{
	return throwex(opus_decode(D(*this), in, insize, out, maxframes, decode_fec ? 1 : 0));
}

size_t decoder::decode(const unsigned char* in, uint32_t insize, float* out, uint32_t maxframes, bool decode_fec)
{
	return throwex(opus_decode_float(D(*this), in, insize, out, maxframes, decode_fec ? 1 : 0));
}

uint32_t decoder::get_nb_samples(const unsigned char* buf, size_t bufsize)
{
	return throwex(opus_decoder_get_nb_samples(D(*this), buf, bufsize));
}

repacketizer::repacketizer(char* _memory)
{
	size_t s = size();
	memory = _memory ? _memory : new char[s];
	user = false;
	opus_repacketizer_init(R(*this));
}

repacketizer::repacketizer(const repacketizer& rp)
{
	size_t s = size();
	memory = new char[s];
	user = false;
	memcpy(memory, rp.memory, s);
}

repacketizer::repacketizer(const repacketizer& rp, char* _memory)
{
	memory = _memory;
	user = true;
	memcpy(memory, rp.memory, size());
}

repacketizer& repacketizer::operator=(const repacketizer& rp)
{
	if(memory != rp.memory)
		memcpy(memory, rp.memory, size());
	return *this;
}

repacketizer::~repacketizer()
{
	if(!user)
		delete[] reinterpret_cast<char*>(memory);
}

size_t repacketizer::size()
{
	return opus_repacketizer_get_size();
}

void repacketizer::cat(const unsigned char* data, size_t len)
{
	throwex(opus_repacketizer_cat(R(*this), data, len));
}

size_t repacketizer::out(unsigned char* data, size_t maxlen)
{
	return throwex(opus_repacketizer_out(R(*this), data, maxlen));
}

size_t repacketizer::out(unsigned char* data, size_t maxlen, unsigned begin, unsigned end)
{
	return throwex(opus_repacketizer_out_range(R(*this), begin, end, data, maxlen));
}

unsigned repacketizer::get_nb_frames()
{
	return opus_repacketizer_get_nb_frames(R(*this));
}

encoder& multistream_encoder::operator[](size_t idx)
{
	if(idx > (size_t)streams)
		throw opus::bad_argument();
	return *reinterpret_cast<encoder*>(substream(idx));
}

size_t multistream_encoder::size(unsigned streams, unsigned coupled_streams)
{
	return alignof(encoder) + streams * sizeof(encoder) + opus_multistream_encoder_get_size(streams,
		coupled_streams);
}

multistream_encoder::~multistream_encoder()
{
	if(!user)
		delete[] memory;
}

multistream_encoder::multistream_encoder(samplerate rate, unsigned _channels, unsigned _streams, 
	unsigned coupled_streams, const unsigned char* mapping, application app, char* _memory)
{
	user = (_memory != NULL);
	memory = _memory ? alignptr(_memory, alignof(encoder)) : new char[size(streams, coupled_streams)];
	try {
		offset = _streams * sizeof(encoder);
		throwex(opus_multistream_encoder_init(ME(*this), rate, channels, _streams, coupled_streams, mapping,
			app));
		init_structures(_channels, _streams, coupled_streams);
	} catch(...) {
		if(!user)
			delete[] memory;
		throw;
	}
}

multistream_encoder::multistream_encoder(const multistream_encoder& e)
{
	init_copy(e, new char[e.size()]);
	user = false;
}

multistream_encoder::multistream_encoder(const multistream_encoder& e, char* memory)
{
	init_copy(e, memory);
	user = true;
}

multistream_encoder& multistream_encoder::operator=(const multistream_encoder& e)
{
	if(this == &e)
		return *this;
	if(streams != e.streams || coupled != e.coupled)
		throw std::runtime_error("Stream mismatch in assignment");
	memcpy(ME(*this), e.memory + e.offset, opussize);
	channels = e.channels;
	return *this;
}

void multistream_encoder::init_structures(unsigned _channels, unsigned _streams, unsigned _coupled)
{
	channels = _channels;
	streams = _streams;
	coupled = _coupled;
	opussize = opus_multistream_encoder_get_size(streams, coupled);
	for(int32_t i = 0; i < streams; i++) {
		OpusEncoder* e;
		opus_multistream_encoder_ctl(ME(*this), OPUS_MULTISTREAM_GET_ENCODER_STATE(i, &e));
		new(substream(i)) encoder(e, i < coupled);
	}
}

void multistream_encoder::init_copy(const multistream_encoder& e, char* _memory)
{
	user = (_memory != NULL);
	memory = _memory ? alignptr(_memory, alignof(encoder)) : new char[e.size()];
	offset = e.offset;
	memcpy(ME(*this), e.memory + e.offset, e.opussize);
	init_structures(e.channels, e.streams, e.coupled);
}

size_t multistream_encoder::encode(const int16_t* in, uint32_t inframes, unsigned char* out, uint32_t maxout)
{
	return throwex(opus_multistream_encode(ME(*this), in, inframes, out, maxout));
}

size_t multistream_encoder::encode(const float* in, uint32_t inframes, unsigned char* out, uint32_t maxout)
{
	return throwex(opus_multistream_encode_float(ME(*this), in, inframes, out, maxout));
}

char* multistream_encoder::substream(size_t idx)
{
	return memory + idx * sizeof(encoder);
}

encoder& surround_encoder::operator[](size_t idx)
{
	if(idx > (size_t)streams)
		throw opus::bad_argument();
	return *reinterpret_cast<encoder*>(substream(idx));
}

size_t surround_encoder::size(unsigned channels, unsigned family)
{
	//Be conservative with memory, as stream count is not known.
#ifdef OPUS_SUPPORTS_SURROUND
	return alignof(encoder) + channels * sizeof(encoder) + opus_multistream_surround_encoder_get_size(channels,
		family);
#else
	int streams, coupled;
	lookup_params(channels, family, streams, coupled);
	//We use channels and not streams here to keep compatiblity on fallback.
	return alignof(encoder) + channels * sizeof(encoder) + opus_multistream_encoder_get_size(streams, coupled);
#endif
}

surround_encoder::~surround_encoder()
{
	if(!user)
		delete[] memory;
}

surround_encoder::surround_encoder(samplerate rate, unsigned _channels, unsigned _family, application app,
	stream_format& format, char* _memory)
{
	user = (_memory != NULL);
	memory = _memory ? alignptr(_memory, alignof(encoder)) : new char[size(channels, family)];
	try {
		int rstreams, rcoupled;
		unsigned char rmapping[256];
		offset = _channels * sizeof(encoder);  //Conservative.
#ifdef OPUS_SUPPORTS_SURROUND
		throwex(opus_multistream_surround_encoder_init(ME(*this), rate, _channels, _family, &rstreams, 
			&rcoupled, rmapping, app));
#else
		lookup_params(_channels, _family, rstreams, rcoupled);
		generate_mapping(_channels, _family, rmapping);
		throwex(opus_multistream_encoder_init(ME(*this), rate, channels, rstreams, rcoupled, rmapping, app));
#endif
		init_structures(_channels, rstreams, rcoupled, _family);
		format.channels = channels;
		format.family = family;
		format.streams = streams;
		format.coupled = coupled;
		memcpy(format.mapping, rmapping, channels);
	} catch(...) {
		if(!user)
			delete[] memory;
		throw;
	}
}

surround_encoder::surround_encoder(const surround_encoder& e)
{
	init_copy(e, new char[e.size()]);
	user = false;
}

surround_encoder::surround_encoder(const surround_encoder& e, char* memory)
{
	init_copy(e, memory);
	user = true;
}

surround_encoder& surround_encoder::operator=(const surround_encoder& e)
{
	if(this == &e)
		return *this;
	if(channels != e.channels || family != e.family)
		throw std::runtime_error("Stream mismatch in assignment");
	memcpy(ME(*this), e.memory + e.offset, opussize);
	return *this;
}

void surround_encoder::init_structures(unsigned _channels, unsigned _streams, unsigned _coupled, unsigned _family)
{
	channels = _channels;
	streams = _streams;
	coupled = _coupled;
	family = _family;
#if OPUS_SUPPORTS_SURROUND
	opussize = opus_multistream_surround_encoder_get_size(_channels, family);
#else
	opussize = opus_multistream_encoder_get_size(streams, coupled);
#endif
	for(int32_t i = 0; i < streams; i++) {
		OpusEncoder* e;
		opus_multistream_encoder_ctl(ME(*this), OPUS_MULTISTREAM_GET_ENCODER_STATE(i, &e));
		new(substream(i)) encoder(e, i < coupled);
	}
}

void surround_encoder::init_copy(const surround_encoder& e, char* _memory)
{
	user = (_memory != NULL);
	memory = _memory ? alignptr(_memory, alignof(encoder)) : new char[e.size()];
	offset = e.offset;
	memcpy(ME(*this), e.memory + e.offset, e.opussize);
	init_structures(e.channels, e.streams, e.coupled, e.family);
}

size_t surround_encoder::encode(const int16_t* in, uint32_t inframes, unsigned char* out, uint32_t maxout)
{
	return throwex(opus_multistream_encode(ME(*this), in, inframes, out, maxout));
}

size_t surround_encoder::encode(const float* in, uint32_t inframes, unsigned char* out, uint32_t maxout)
{
	return throwex(opus_multistream_encode_float(ME(*this), in, inframes, out, maxout));
}

char* surround_encoder::substream(size_t idx)
{
	return memory + idx * sizeof(encoder);
}

decoder& multistream_decoder::operator[](size_t idx)
{
	if(idx > (size_t)streams)
		throw opus::bad_argument();
	return *reinterpret_cast<decoder*>(substream(idx));
}

size_t multistream_decoder::size(unsigned streams, unsigned coupled_streams)
{
	return streams * sizeof(decoder) + opus_multistream_decoder_get_size(streams,
		coupled_streams);
}

multistream_decoder::multistream_decoder(samplerate rate, unsigned _channels, unsigned _streams,
	unsigned coupled_streams, const unsigned char* mapping, char* _memory)
{
	user = (_memory != NULL);
	memory = _memory ? alignptr(_memory, alignof(decoder)) : new char[size(streams, coupled_streams)];
	try {
		throwex(opus_multistream_decoder_init(MD(*this), rate, channels, streams, coupled_streams, mapping));
		init_structures(_channels, _streams, coupled_streams);
	} catch(...) {
		if(!user)
			delete[] memory;
		throw;
	}
}
multistream_decoder::multistream_decoder(const multistream_decoder& e)
{
	init_copy(e, NULL);
}

multistream_decoder::multistream_decoder(const multistream_decoder& e, char* memory)
{
	init_copy(e, memory);
}

multistream_decoder::~multistream_decoder()
{
	if(!user)
		delete[] memory;
}

multistream_decoder& multistream_decoder::operator=(const multistream_decoder& d)
{
	if(this == &d)
		return *this;
	if(streams != d.streams || coupled != d.coupled)
		throw std::runtime_error("Stream mismatch in assignment");
	memcpy(MD(*this), d.memory + d.offset, opussize);
	channels = d.channels;
	return *this;
}

void multistream_decoder::init_structures(unsigned _channels, unsigned _streams, unsigned _coupled)
{
	channels = _channels;
	streams = _streams;
	coupled = _coupled;
	opussize = opus_multistream_decoder_get_size(streams, coupled);
	for(int32_t i = 0; i < (size_t)streams; i++) {
		OpusDecoder* d;
		opus_multistream_decoder_ctl(MD(*this), OPUS_MULTISTREAM_GET_DECODER_STATE(i, &d));
		new(substream(i)) decoder(d, i < coupled);
	}
}

void multistream_decoder::init_copy(const multistream_decoder& e, char* _memory)
{
	user = (_memory != NULL);
	memory = _memory ? alignptr(_memory, alignof(decoder)) : new char[e.size()];
	offset = e.offset;
	memcpy(MD(*this), e.memory + e.offset, e.opussize);
	init_structures(e.channels, e.streams, e.coupled);
}

size_t multistream_decoder::decode(const unsigned char* in, uint32_t insize, int16_t* out, uint32_t maxframes,
	bool decode_fec)
{
	return throwex(opus_multistream_decode(MD(*this), in, insize, out, maxframes, decode_fec));
}

size_t multistream_decoder::decode(const unsigned char* in, uint32_t insize, float* out, uint32_t maxframes,
	bool decode_fec)
{
	return throwex(opus_multistream_decode_float(MD(*this), in, insize, out, maxframes, decode_fec));
}

char* multistream_decoder::substream(size_t idx)
{
	return memory + idx * sizeof(encoder);
}

uint32_t packet_get_nb_frames(const unsigned char* packet, size_t len)
{
	return throwex(opus_packet_get_nb_frames(packet, len));
}

uint32_t packet_get_samples_per_frame(const unsigned char* data, samplerate fs)
{
	return throwex(opus_packet_get_samples_per_frame(data, fs));
}

uint32_t packet_get_nb_samples(const unsigned char* packet, size_t len, samplerate fs)
{
	return packet_get_nb_frames(packet, len) * packet_get_samples_per_frame(packet, fs);
}

uint32_t packet_get_nb_channels(const unsigned char* packet)
{
	return throwex(opus_packet_get_nb_channels(packet));
}

bandwidth packet_get_bandwidth(const unsigned char* packet)
{
	return bandwidth(throwex(opus_packet_get_bandwidth(packet)));
}

parsed_packet packet_parse(const unsigned char* packet, size_t len)
{
	parsed_packet p;
	unsigned char toc;
	const unsigned char* frames[48];
	short size[48];
	int payload_offset;
	uint32_t framecnt = throwex(opus_packet_parse(packet, len, &toc, frames, size, &payload_offset));
	p.toc = toc;
	p.payload_offset = payload_offset;
	p.frames.resize(framecnt);
	for(unsigned i = 0; i < framecnt; i++) {
		p.frames[i].ptr = frames[i];
		p.frames[i].size = size[i];
	}
	return p;
}

std::string version()
{
	return opus_get_version_string();
}

}
#endif
