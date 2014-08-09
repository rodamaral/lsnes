#ifndef _library_opus_hpp_included_
#define _library_opus_hpp_included_

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <functional>

namespace loadlib
{
class module;
}

namespace opus
{
void load_libopus(const loadlib::module& lib);
bool libopus_loaded();
size_t add_callback(std::function<void()> fun);
void cancel_callback(size_t handle);

struct bad_argument : public std::runtime_error { bad_argument(); };
struct buffer_too_small : public std::runtime_error { buffer_too_small(); };
struct internal_error : public std::runtime_error { internal_error(); };
struct invalid_packet : public std::runtime_error { invalid_packet(); };
struct unimplemented : public std::runtime_error { unimplemented(); };
struct invalid_state : public std::runtime_error { invalid_state(); };
struct not_loaded : public std::runtime_error { not_loaded(); };

class encoder;
class decoder;
class multistream_encoder;
class multistream_decoder;
class surround_encoder;

template<typename T> struct generic_eget
{
	typedef T erettype;
	T operator()(encoder& e) const;
	T errordefault() const;
};

template<typename T> struct generic_meget : public generic_eget<T>
{
	using generic_eget<T>::operator();
	T operator()(multistream_encoder& e) const;
	T operator()(surround_encoder& e) const;
};

template<typename T> struct generic_get
{
	typedef T drettype;
	typedef T erettype;
	T operator()(decoder& e) const;
	T operator()(encoder& e) const;
	T errordefault() const;
};

template<typename T> struct generic_mget : public generic_get<T>
{
	using generic_get<T>::operator();
	T operator()(multistream_decoder& e) const;
	T operator()(multistream_encoder& e) const;
	T operator()(surround_encoder& e) const;
};

template<typename T> struct generic_dget
{
	typedef T drettype;
	T operator()(decoder& e) const;
	T errordefault() const;
};

template<typename T> struct generic_mdget : public generic_dget<T>
{
	using generic_dget<T>::operator();
	T operator()(multistream_decoder& e) const;
};

struct samplerate
{
	samplerate(int32_t _fs) { fs = _fs; }
	static samplerate r8k;
	static samplerate r12k;
	static samplerate r16k;
	static samplerate r24k;
	static samplerate r48k;
	operator int32_t() { return fs; }
	typedef samplerate erettype;
	samplerate operator()(encoder& e) const;
	samplerate operator()(multistream_encoder& e) const;
	samplerate operator()(decoder& e) const;
	samplerate operator()(multistream_decoder& e) const;
	samplerate errordefault() const { return samplerate(0); }
private:
	int32_t fs;
};

struct complexity
{
	complexity(int32_t _c) { c = _c; }
	operator int32_t() { return c; }
	static generic_meget<complexity> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t c;
};

struct bitrate
{
	bitrate(int32_t _b) { b = _b; }
	static bitrate _auto;
	static bitrate max;
	operator int32_t() { return b; }
	static generic_meget<bitrate> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t b;
};

struct vbr
{
	vbr(bool _v) { v = _v; }
	static vbr cbr;
	static vbr _vbr;
	operator bool() { return v; }
	static generic_meget<vbr> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	bool v;
};

struct vbr_constraint
{
	vbr_constraint(bool _c) { c = _c; }
	static vbr_constraint unconstrained;
	static vbr_constraint constrained;
	operator bool() { return c; }
	static generic_meget<vbr_constraint> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	bool c;
};

struct force_channels
{
	force_channels(int32_t _f) { f = _f; }
	static force_channels _auto;
	static force_channels mono;
	static force_channels stereo;
	operator int32_t() { return f; }
	static generic_meget<force_channels> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t f;
};

struct max_bandwidth
{
	max_bandwidth(int32_t _bw) { bw = _bw; }
	static max_bandwidth narrow;
	static max_bandwidth medium;
	static max_bandwidth wide;
	static max_bandwidth superwide;
	static max_bandwidth full;
	operator int32_t() { return bw; }
	static generic_eget<max_bandwidth> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void errordefault() const {}
private:
	int32_t bw;
};

struct bandwidth
{
	bandwidth(int32_t _bw) { bw = _bw; }
	static bandwidth _auto;
	static bandwidth narrow;
	static bandwidth medium;
	static bandwidth wide;
	static bandwidth superwide;
	static bandwidth full;
	operator int32_t() { return bw; }
	static generic_mget<bandwidth> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t bw;
};

struct signal
{
	signal(int32_t _s) { s = _s; }
	static signal _auto;
	static signal music;
	static signal voice;
	operator int32_t() { return s; }
	static generic_meget<signal> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t s;
};

struct application
{
	application(int32_t _app) { app = _app; };
	static application audio;
	static application voice;
	static application lowdelay;
	operator int32_t() { return app; }
	static generic_meget<application> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t app;
};

struct _lookahead
{
	_lookahead() { l = 0; }
	_lookahead(uint32_t _l) { l = _l; }
	operator uint32_t() { return l; }
	typedef _lookahead erettype;
	_lookahead operator()(encoder& e) const;
	_lookahead operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	_lookahead errordefault() const { return _lookahead(0);}
private:
	uint32_t l;
};
extern _lookahead lookahead;

struct fec
{
	fec(bool _f) { f = _f; }
	static fec disabled;
	static fec enabled;
	operator bool() { return f; }
	static generic_meget<fec> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	bool f;
};

struct lossperc
{
	lossperc(uint32_t _loss) { loss = _loss; };
	operator uint32_t() { return loss; }
	static generic_meget<lossperc> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	uint32_t loss;
};

struct dtx
{
	dtx(bool _d) { d = _d; }
	static dtx disabled;
	static dtx enabled;
	operator bool() { return d; }
	static generic_meget<dtx> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	bool d;
};

struct lsbdepth
{
	lsbdepth(uint32_t _depth) { depth = _depth; };
	static lsbdepth d8;
	static lsbdepth d16;
	static lsbdepth d24;
	operator uint32_t() { return depth; }
	static generic_meget<lsbdepth> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	uint32_t depth;
};

struct _pktduration
{
	_pktduration() { d = 0; }
	_pktduration(uint32_t _d) { d = _d; }
	operator uint32_t() { return d; }
	typedef _pktduration drettype;
	_pktduration operator()(decoder& e) const;
	_pktduration operator()(multistream_decoder& e) const;
	void operator()(surround_encoder& e) const;
	_pktduration errordefault() const; /*{ return _pktduration(0); }*/
private:
	uint32_t d;
};
extern _pktduration pktduration;

struct _reset
{
	typedef void drettype;
	typedef void erettype;
	void operator()(decoder& e) const;
	void operator()(encoder& e) const;
	void operator()(multistream_decoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
};
extern _reset reset;

struct _finalrange
{
	_finalrange() { f = 0; }
	_finalrange(uint32_t _f) { f = _f; }
	operator uint32_t() { return f; }
	typedef _finalrange drettype;
	typedef _finalrange erettype;
	_finalrange operator()(decoder& e) const;
	_finalrange operator()(encoder& e) const;
	_finalrange operator()(multistream_decoder& e) const;
	_finalrange operator()(multistream_encoder& e) const;
	_finalrange operator()(surround_encoder& e) const;
	_finalrange errordefault() const { return _finalrange(0); }
private:
	uint32_t f;
};
extern _finalrange finalrange;

struct _pitch
{
	_pitch() { p = 0; }
	_pitch(uint32_t _p) { p = _p; }
	operator uint32_t() { return p; }
	typedef _pitch drettype;
	typedef _pitch erettype;
	_pitch operator()(encoder& e) const;
	_pitch operator()(decoder& e) const;
	_pitch errordefault() const { return _pitch(0); }
private:
	uint32_t p;
};
extern _pitch pitch;

struct gain
{
	gain(int32_t _g) { g = _g; };
	operator int32_t() { return g; }
	static generic_mdget<gain> get;
	typedef void drettype;
	void operator()(decoder& d) const;
	void operator()(multistream_decoder& d) const;
	gain errordefault() const { return gain(0); }
private:
	int32_t g;
};

struct prediction_disabled
{
	prediction_disabled(bool _d) { d = _d; }
	static prediction_disabled disabled;
	static prediction_disabled enabled;
	operator bool() { return d; }
	static generic_meget<prediction_disabled> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const { }
private:
	bool d;
};

struct frame_duration
{
	frame_duration(int32_t _v) { v = _v; }
	static frame_duration argument;
	static frame_duration variable;
	static frame_duration l2ms;	//Really 2.5ms.
	static frame_duration l5ms;
	static frame_duration l10ms;
	static frame_duration l20ms;
	static frame_duration l40ms;
	static frame_duration l60ms;
	operator int32_t() { return v; }
	static generic_meget<frame_duration> get;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const { }
private:
	int32_t v;
};

struct _last_packet_duration
{
	_last_packet_duration() { p = 0; }
	_last_packet_duration(uint32_t _p) { p = _p; }
	operator uint32_t() { return p; }
	typedef _last_packet_duration drettype;
	_last_packet_duration operator()(decoder& e) const;
	_last_packet_duration operator()(multistream_decoder& e) const;
	_last_packet_duration errordefault() const { return _last_packet_duration(0); }
private:
	uint32_t p;
};
extern _last_packet_duration last_packet_duration;



struct set_control_int
{
	set_control_int(int32_t _ctl, int32_t _val) { ctl = _ctl; val = _val; }
	typedef void drettype;
	typedef void erettype;
	void operator()(encoder& e) const;
	void operator()(decoder& e) const;
	void operator()(multistream_encoder& e) const;
	void operator()(multistream_decoder& e) const;
	void operator()(surround_encoder& e) const;
	void errordefault() const {}
private:
	int32_t ctl;
	int32_t val;
};

struct get_control_int
{
	get_control_int(int32_t _ctl) { ctl = _ctl; }
	typedef int32_t drettype;
	typedef int32_t erettype;
	int32_t operator()(encoder& e) const;
	int32_t operator()(decoder& e) const;
	int32_t operator()(multistream_encoder& e) const;
	int32_t operator()(multistream_decoder& e) const;
	int32_t operator()(surround_encoder& e) const;
	int32_t errordefault() const { return -1; }
private:
	int32_t ctl;
};

class encoder
{
public:
	encoder(samplerate rate, bool stereo, application app, char* memory = NULL);
	encoder(void* state, bool _stereo);
	~encoder();
	encoder(const encoder& e);
	encoder(const encoder& e, char* memory);
	static size_t size(bool stereo);
	size_t size() const { return size(stereo); }
	encoder& operator=(const encoder& e);
	template<typename T> typename T::erettype ctl(const T& c) { return c(*this); }
	template<typename T> typename T::erettype ctl_quiet(const T& c)
	{
		try { return c(*this); } catch(...) { return c.errordefault(); }
	}
	size_t encode(const int16_t* in, uint32_t inframes, unsigned char* out, uint32_t maxout);
	size_t encode(const float* in, uint32_t inframes, unsigned char* out, uint32_t maxout);
	bool is_stereo() { return stereo; }
	void* getmem() { return memory; }
private:
	void* memory;
	bool stereo;
	bool user;
};

class decoder
{
public:
	decoder(samplerate rate, bool stereo, char* memory = NULL);
	decoder(void* state, bool _stereo);
	~decoder();
	decoder(const decoder& e);
	decoder(const decoder& e, char* memory);
	static size_t size(bool stereo);
	size_t size() const { return size(stereo); }
	decoder& operator=(const decoder& e);
	template<typename T> typename T::drettype ctl(const T& c) { return c(*this); }
	template<typename T> typename T::drettype ctl_quiet(const T& c)
	{
		try { return c(*this); } catch(...) { return c.errordefault(); }
	}
	size_t decode(const unsigned char* in, uint32_t insize, int16_t* out, uint32_t maxframes,
		bool decode_fec = false);
	size_t decode(const unsigned char* in, uint32_t insize, float* out, uint32_t maxframes,
		bool decode_fec = false);
	uint32_t get_nb_samples(const unsigned char* buf, size_t bufsize);
	bool is_stereo() { return stereo; }
	void* getmem() { return memory; }
private:
	void* memory;
	bool stereo;
	bool user;
};

class repacketizer
{
public:
	repacketizer(char* memory = NULL);
	repacketizer(const repacketizer& rp);
	repacketizer(const repacketizer& rp, char* memory);
	repacketizer& operator=(const repacketizer& rp);
	~repacketizer();
	static size_t size();
	void cat(const unsigned char* data, size_t len);
	size_t out(unsigned char* data, size_t maxlen);
	size_t out(unsigned char* data, size_t maxlen, unsigned begin, unsigned end);
	unsigned get_nb_frames();
	void* getmem() { return memory; }
private:
	void* memory;
	bool user;
};

class multistream_encoder
{
public:
	multistream_encoder(samplerate rate, unsigned channels, unsigned streams,
		unsigned coupled_streams, const unsigned char* mapping, application app, char* memory = NULL);
	static size_t size(unsigned streams, unsigned coupled_streams);
	size_t size() const { return size(streams, coupled); }
	multistream_encoder(const multistream_encoder& e);
	multistream_encoder(const multistream_encoder& e, char* memory);
	multistream_encoder& operator=(const multistream_encoder& e);
	~multistream_encoder();
	encoder& operator[](size_t idx);
	size_t encode(const int16_t* in, uint32_t inframes, unsigned char* out, uint32_t maxout);
	size_t encode(const float* in, uint32_t inframes, unsigned char* out, uint32_t maxout);
	template<typename T> typename T::erettype ctl(const T& c) { return c(*this); }
	template<typename T> typename T::erettype ctl_quiet(const T& c)
	{
		try { return c(*this); } catch(...) { return c.errordefault(); }
	}
	uint8_t get_channels() { return channels; }
	uint8_t get_streams() { return streams; }
	void* getmem() { return memory + offset; }
private:
	void init_copy(const multistream_encoder& e, char* memory);
	char* substream(size_t idx);
	void init_structures(unsigned _channels, unsigned _streams, unsigned _coupled);
	char* memory;
	bool user;
	uint8_t channels;
	uint8_t streams;
	uint8_t coupled;
	size_t offset;
	size_t opussize;
};

class surround_encoder
{
public:
	struct stream_format
	{
		unsigned family;
		unsigned channels;
		unsigned streams;
		unsigned coupled;
		unsigned char mapping[256];
	};
	surround_encoder(samplerate rate, unsigned channels, unsigned family, application app,
		stream_format& format, char* memory = NULL);
	static size_t size(unsigned channels, unsigned family);
	size_t size() const { return size(channels, family); }
	surround_encoder(const surround_encoder& e);
	surround_encoder(const surround_encoder& e, char* memory);
	surround_encoder& operator=(const surround_encoder& e);
	~surround_encoder();
	encoder& operator[](size_t idx);
	size_t encode(const int16_t* in, uint32_t inframes, unsigned char* out, uint32_t maxout);
	size_t encode(const float* in, uint32_t inframes, unsigned char* out, uint32_t maxout);
	template<typename T> typename T::erettype ctl(const T& c) { return c(*this); }
	template<typename T> typename T::erettype ctl_quiet(const T& c)
	{
		try { return c(*this); } catch(...) { return c.errordefault(); }
	}
	uint8_t get_channels() { return channels; }
	uint8_t get_streams() { return streams; }
	void* getmem() { return memory + offset; }
private:
	void init_copy(const surround_encoder& e, char* memory);
	char* substream(size_t idx);
	void init_structures(unsigned _channels, unsigned _streams, unsigned _coupled, unsigned _family);
	char* memory;
	bool user;
	uint8_t channels;
	uint8_t streams;
	uint8_t coupled;
	uint8_t family;
	size_t offset;
	size_t opussize;
};

class multistream_decoder
{
public:
	multistream_decoder(samplerate rate, unsigned channels, unsigned streams,
		unsigned coupled_streams, const unsigned char* mapping, char* memory = NULL);
	static size_t size(unsigned streams, unsigned coupled_streams);
	size_t size() const { return size(streams, coupled); }
	multistream_decoder(const multistream_decoder& e);
	multistream_decoder(const multistream_decoder& e, char* memory);
	multistream_decoder& operator=(const multistream_decoder& e);
	~multistream_decoder();
	decoder& operator[](size_t idx);
	template<typename T> typename T::drettype ctl(const T& c) { return c(*this); }
	template<typename T> typename T::drettype ctl_quiet(const T& c)
	{
		try { return c(*this); } catch(...) { return c.errordefault(); }
	}
	size_t decode(const unsigned char* in, uint32_t insize, int16_t* out, uint32_t maxframes,
		bool decode_fec = false);
	size_t decode(const unsigned char* in, uint32_t insize, float* out, uint32_t maxframes,
		bool decode_fec = false);
	uint8_t get_channels() { return channels; }
	uint8_t get_streams() { return streams; }
	void* getmem() { return memory + offset; }
private:
	void init_copy(const multistream_decoder& e, char* memory);
	char* substream(size_t idx);
	void init_structures(unsigned _channels, unsigned _streams, unsigned _coupled);
	char* memory;
	bool user;
	uint8_t channels;
	uint8_t streams;
	uint8_t coupled;
	size_t offset;
	size_t opussize;
};

struct parsed_frame
{
	const unsigned char* ptr;
	short size;
};

struct parsed_packet
{
	unsigned char toc;
	std::vector<parsed_frame> frames;
	uint32_t payload_offset;
};

uint32_t packet_get_nb_frames(const unsigned char* packet, size_t len);
uint32_t packet_get_samples_per_frame(const unsigned char* data, samplerate fs);
uint32_t packet_get_nb_samples(const unsigned char* packet, size_t len, samplerate fs);
uint32_t packet_get_nb_channels(const unsigned char* packet);
bandwidth packet_get_bandwidth(const unsigned char* packet);
parsed_packet packet_parse(const unsigned char* packet, size_t len);
std::string version();

/**
 * Get tick (2.5ms) count from opus packet.
 *
 * Parameter packet: The packet data.
 * Parameter packetsize: The size of packet.
 *
 * Note: Guaranteed not to read more than 2 bytes from the packet.
 */
uint8_t packet_tick_count(const uint8_t* packet, size_t packetsize);
}
#endif
