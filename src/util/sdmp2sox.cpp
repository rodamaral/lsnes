#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>

#define FLAG_WIDTH 1
#define FLAG_HEIGHT 2
#define FLAG_FRAMERATE 4
#define FLAG_FULLRANGE 8
#define FLAG_ITU601 0
#define FLAG_ITU709 16
#define FLAG_SMPTE240M 32
#define FLAG_CS_MASK 48
#define FLAG_8BIT 64
#define FLAG_FAKENLARGE 128
#define FLAG_DEDUP 256
#define FLAG_OFFSET2 512
#define FLAG_10FRAMES 1024
#define FLAG_AR_CORRECT 2048

#define CMD_HIRES 1
#define CMD_INTERLACED 2
#define CMD_OVERSCAN 4
#define CMD_PAL 8

#define MAX_DEDUP 9

//This buffer needs to be big enough to store 640x480 16-bit YCbCr 4:4:4 (6 bytes per pixel) image.
#define MAXYUVSIZE (640 * 480 * 6)
unsigned char yuv_buffer[MAXYUVSIZE];
unsigned char old_yuv_buffer[MAXYUVSIZE];

//30 bit values.
uint32_t ymatrix[0x80000];
uint32_t cbmatrix[0x80000];
uint32_t crmatrix[0x80000];


#define TOYUV(src, idx) do {\
	uint32_t c = (static_cast<uint32_t>(src[(idx) + 0]) << 24) |\
	(static_cast<uint32_t>(src[(idx) + 1]) << 16) |\
	(static_cast<uint32_t>(src[(idx) + 2]) << 8) |\
	static_cast<uint32_t>(src[(idx) + 3]);\
	Y += ymatrix[c & 0x7FFFF];\
	Cb += cbmatrix[c & 0x7FFFF];\
	Cr += crmatrix[c & 0x7FFFF];\
	} while(0)

#define RGB2YUV_SHIFT 14

class sox_output
{
public:
	sox_output(std::ostream& soxs, uint32_t apurate, bool silence2);
	void close();
	void add_sample(unsigned char* buf);
	uint64_t get_samples();
private:
	std::ostream& strm;
	uint64_t samples;
};

class sdmp_input_stream
{
public:
	sdmp_input_stream(std::istream& sdmp);
	uint32_t get_cpurate();
	uint32_t get_apurate();
	int read_command();
	void read_linepair(unsigned char* buffer);
	void copy_audio_sample(sox_output& audio_out);
private:
	std::istream& strm;
	uint32_t cpurate;
	uint32_t apurate;
};

class time_tracker
{
public:
	time_tracker(uint32_t _cpurate);
	void add_2s();
	void advance(bool pal, bool interlaced);
	uint64_t get_ts();
private:
	uint32_t cpurate;
	uint64_t w;
	uint64_t n;
};

class dup_tracker
{
public:
	dup_tracker(std::ostream& _tcfile, uint32_t _flags);
	uint32_t process(unsigned char* buffer, size_t bufsize, int pkt_type, time_tracker& ts);
private:
	std::ostream& tcfile;
	uint32_t flags;
	uint32_t counter;
};


struct store16
{
	static const size_t esize = 2;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		*reinterpret_cast<uint16_t*>(buffer + idx) = (v1 >> RGB2YUV_SHIFT);
		*reinterpret_cast<uint16_t*>(buffer + idx + psep) = (v2 >> RGB2YUV_SHIFT);
		*reinterpret_cast<uint16_t*>(buffer + idx + 2 * psep) = (v3 >> RGB2YUV_SHIFT);
	}
};

struct store8
{
	static const size_t esize = 1;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		buffer[idx] = (v1 >> (RGB2YUV_SHIFT + 8));
		buffer[idx + psep] = (v2 >> (RGB2YUV_SHIFT + 8));
		buffer[idx + 2 * psep] = (v3 >> (RGB2YUV_SHIFT + 8));
	}
};

template<class store, size_t ioff1, size_t ioff2, size_t ioff3, size_t ooff1, size_t ooff2, size_t ooff3>
struct loadstore
{
	static const size_t esize = store::esize;
	static void convert(unsigned char* obuffer, size_t oidx, const unsigned char* ibuffer, size_t iidx,
		size_t psep)
	{
		//Compiler should be able to eliminate every if out of this.
		uint32_t Y = 0;
		uint32_t Cb = 0;
		uint32_t Cr = 0;
		TOYUV(ibuffer, iidx);
		if(ioff1 > 0)
			TOYUV(ibuffer, iidx + ioff1 * 4);
		if(ioff2 > 0)
			TOYUV(ibuffer, iidx + ioff2 * 4);
		if(ioff3 > 0)
			TOYUV(ibuffer, iidx + ioff3 * 4);
		if(ioff1 > 0 && ioff2 > 0 && ioff3 > 0) {
			Y >>= 2;
			Cb >>= 2;
			Cr >>= 2;
		} else if(ioff1 > 0) {
			Y >>= 1;
			Cb >>= 1;
			Cr >>= 1;
		}
		store::store(obuffer, oidx, psep, Y, Cb, Cr);
		if(ooff1 > 0)
			store::store(obuffer, oidx + ooff1 * store::esize, psep, Y, Cb, Cr);
		if(ooff2 > 0)
			store::store(obuffer, oidx + ooff2 * store::esize, psep, Y, Cb, Cr);
		if(ooff3 > 0)
			store::store(obuffer, oidx + ooff3 * store::esize, psep, Y, Cb, Cr);
	}
};

template<class proc, size_t lim, size_t igap, size_t ogap>
struct loop
{
	static void f(unsigned char* buffer, const unsigned char* src, size_t psep)
	{
		for(size_t i = 0; i < lim; i++)
			proc::convert(buffer, proc::esize * ogap * i, src, 4 * igap * i, psep);
	}
};

//Render a line pair of YUV with 256x224/240
template<class store>
void render_yuv_256_240(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires, bool interlaced)
{
	if(hires)
		if(interlaced)
			loop<loadstore<store, 1, 512, 513, 0, 0, 0>, 256, 2, 1>::f(buffer, src, psep);
		else
			loop<loadstore<store, 1, 0, 0, 0, 0, 0>, 256, 2, 1>::f(buffer, src, psep);
	else
		if(interlaced)
			loop<loadstore<store, 512, 0, 0, 0, 0, 0>, 256, 1, 1>::f(buffer, src, psep);
		else
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 256, 1, 1>::f(buffer, src, psep);
}

//Render a line pair of YUV with 512x224/240
template<class store>
void render_yuv_512_240(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires, bool interlaced)
{
	if(hires)
		if(interlaced)
			loop<loadstore<store, 512, 0, 0, 0, 0, 0>, 512, 1, 1>::f(buffer, src, psep);
		else
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 512, 1, 1>::f(buffer, src, psep);
	else
		if(interlaced)
			loop<loadstore<store, 512, 0, 0, 1, 0, 0>, 256, 1, 2>::f(buffer, src, psep);
		else
			loop<loadstore<store, 0, 0, 0, 1, 0, 0>, 256, 1, 2>::f(buffer, src, psep);
}

//Render a line pair of YUV with 256x448/480
template<class store>
void render_yuv_256_480(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires, bool interlaced)
{
	if(hires)
		if(interlaced) {
			loop<loadstore<store, 1, 0, 0, 0, 0, 0>, 256, 2, 1>::f(buffer, src, psep);
			loop<loadstore<store, 1, 0, 0, 0, 0, 0>, 256, 2, 1>::f(buffer + 256 * store::esize,
				src + 2048, psep);
		} else
			loop<loadstore<store, 1, 0, 0, 256, 0, 0>, 256, 2, 1>::f(buffer, src, psep);
	else
		if(interlaced) {
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 256, 1, 1>::f(buffer, src, psep);
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 256, 1, 1>::f(buffer + 256 * store::esize,
				src + 2048, psep);
		} else
			loop<loadstore<store, 0, 0, 0, 256, 0, 0>, 256, 1, 1>::f(buffer, src, psep);
}

//Render a line pair of YUV with 512x448/480
template<class store>
void render_yuv_512_480(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires, bool interlaced)
{
	if(hires)
		if(interlaced) {
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 512, 1, 1>::f(buffer, src, psep);
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 512, 1, 1>::f(buffer + 512 * store::esize,
				src + 2048, psep);
		} else
			loop<loadstore<store, 0, 0, 0, 512, 0, 0>, 512, 1, 1>::f(buffer, src, psep);
	else
		if(interlaced) {
			loop<loadstore<store, 0, 0, 0, 1, 0, 0>, 256, 1, 2>::f(buffer, src, psep);
			loop<loadstore<store, 0, 0, 0, 1, 0, 0>, 256, 1, 2>::f(buffer + 512 * store::esize,
				src + 2048, psep);
		} else
			loop<loadstore<store, 0, 0, 0, 1, 512, 513>, 256, 1, 2>::f(buffer, src, psep);
}

//Render a line pair of YUV with 512x448/480 fakeexpand
template<class store>
void render_yuv_fe(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires, bool interlaced)
{
	if(hires)
		if(interlaced) {
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 512, 1, 1>::f(buffer, src, psep);
			loop<loadstore<store, 0, 0, 0, 0, 0, 0>, 512, 1, 1>::f(buffer + 512 * store::esize,
				src + 2048, psep);
		} else
			loop<loadstore<store, 1, 0, 0, 1, 512, 513>, 256, 2, 2>::f(buffer, src, psep);
	else
		if(interlaced) {
			loop<loadstore<store, 0, 0, 0, 1, 0, 0>, 256, 1, 2>::f(buffer, src, psep);
			loop<loadstore<store, 0, 0, 0, 1, 0, 0>, 256, 1, 2>::f(buffer + 512 * store::esize,
				src + 2048, psep);
		} else
			loop<loadstore<store, 0, 0, 0, 1, 256, 257>, 256, 1, 2>::f(buffer, src, psep);
}

typedef void (*render_yuv_t)(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires,
	bool interlaced);

render_yuv_t get_renderer_for(int32_t flags)
{
	int32_t mode = flags & (FLAG_WIDTH | FLAG_HEIGHT | FLAG_8BIT | FLAG_FAKENLARGE);
	if(mode == 0)
		return render_yuv_256_240<store16>;
	if(mode == FLAG_WIDTH)
		return render_yuv_512_240<store16>;
	if(mode == FLAG_HEIGHT)
		return render_yuv_256_480<store16>;
	if(mode == (FLAG_WIDTH | FLAG_HEIGHT))
		return render_yuv_512_480<store16>;
	if(mode == (FLAG_WIDTH | FLAG_HEIGHT | FLAG_FAKENLARGE))
		return render_yuv_fe<store16>;
	if(mode == FLAG_8BIT)
		return render_yuv_256_240<store8>;
	if(mode == (FLAG_WIDTH | FLAG_8BIT))
		return render_yuv_512_240<store8>;
	if(mode == (FLAG_HEIGHT | FLAG_8BIT))
		return render_yuv_256_480<store8>;
	if(mode == (FLAG_WIDTH | FLAG_HEIGHT | FLAG_8BIT))
		return render_yuv_512_480<store8>;
	if(mode == (FLAG_WIDTH | FLAG_HEIGHT | FLAG_FAKENLARGE | FLAG_8BIT))
		return render_yuv_fe<store8>;
	throw std::runtime_error("get_renderer_for: Unknown flags combination");
}

void init_matrix(double Kb, double Kr, bool fullrange)
{
	double RY = Kr;
	double GY = 1 - Kr  - Kb;
	double BY = Kb;
	double RPb = -0.5 * Kr / (1 - Kb);
	double GPb = -0.5 * (1 - Kr - Kb) / (1 - Kb);
	double BPb = 0.5;
	double RPr = 0.5;
	double GPr = -0.5 * (1 - Kr - Kb) / (1 - Kr);
	double BPr = -0.5 * Kb / (1 - Kr);
	for(uint32_t i = 0; i < 0x80000; i++) {
		uint32_t l = 1 + ((i >> 15) & 0xF);
		//Range of (r,g,b) is 0...496.
		uint32_t r = (l * ((i >> 0) & 0x1F));
		uint32_t g = (l * ((i >> 5) & 0x1F));
		uint32_t b = (l * ((i >> 10) & 0x1F));
		double Y = (RY * r + GY * g + BY * b) / 496 * (fullrange ? 255 : 219) + (fullrange ? 0 : 16);
		double Cb = (RPb * r + GPb * g + BPb * b) / 496 * (fullrange ? 255 : 224) + 128;
		double Cr = (RPr * r + GPr * g + BPr * b) / 496 * (fullrange ? 255 : 224) + 128;
		ymatrix[i] = static_cast<uint32_t>(Y * 4194304 + 0.5);
		cbmatrix[i] = static_cast<uint32_t>(Cb * 4194304 + 0.5);
		crmatrix[i] = static_cast<uint32_t>(Cr * 4194304 + 0.5);
	}
}

//Load RGB to YUV conversion matrix.
void load_rgb2yuv_matrix(uint32_t flags)
{
	switch(flags & (FLAG_CS_MASK))
	{
	case FLAG_ITU601:
		init_matrix(0.114, 0.229, flags & FLAG_FULLRANGE);
		break;
	case FLAG_ITU709:
		init_matrix(0.0722, 0.2126, flags & FLAG_FULLRANGE);
		break;
	case FLAG_SMPTE240M:
		init_matrix(0.087, 0.212, flags & FLAG_FULLRANGE);
		break;
	default:
		init_matrix(0.114, 0.229, flags & FLAG_FULLRANGE);
		break;
	}
}

uint64_t double_to_ieeefp(double v)
{
	unsigned mag = 1023;
	while(v >= 2) {
		mag++;
		v /= 2;
	}
	while(v < 1) {
		mag--;
		v *= 2;
	}
	uint64_t v2 = mag;
	v -= 1;
	for(unsigned i = 0; i < 52; i++) {
		v *= 2;
		v2 = 2 * v2 + ((v >= 1) ? 1 : 0);
		if(v >= 1)
			v -= 1;
	}
	return v2;
}


sox_output::sox_output(std::ostream& soxs, uint32_t apurate, bool silence2)
	: strm(soxs)
{
	uint64_t sndrateR = double_to_ieeefp(static_cast<double>(apurate) / 768.0);
	unsigned char sox_header[32] = {0};
	sox_header[0] = 0x2E;		//Magic
	sox_header[1] = 0x53;		//Magic
	sox_header[2] = 0x6F;		//Magic
	sox_header[3] = 0x58;		//Magic
	sox_header[4] = 0x1C;		//Magic
	sox_header[16] = sndrateR;
	sox_header[17] = sndrateR >> 8;
	sox_header[18] = sndrateR >> 16;
	sox_header[19] = sndrateR >> 24;
	sox_header[20] = sndrateR >> 32;
	sox_header[21] = sndrateR >> 40;
	sox_header[22] = sndrateR >> 48;
	sox_header[23] = sndrateR >> 56;
	sox_header[24] = 2;
	strm.write(reinterpret_cast<char*>(sox_header), 32);
	if(!strm)
		throw std::runtime_error("Can't write audio header");
	samples = 0;
	if(silence2) {
		uint64_t nullsamples = apurate / 384;
		samples = nullsamples;
		const size_t bufsz = 512;
		char nbuffer[8 * bufsz] = {0};
		while(nullsamples > bufsz) {
			strm.write(nbuffer, 8 * bufsz);
			nullsamples -= bufsz;
		}
		strm.write(nbuffer, 8 * nullsamples);
		if(!strm)
			throw std::runtime_error("Can't write 2 second silence");
	}
}

void sox_output::close()
{
	//Sox internally multiplies sample count by channel count.
	unsigned char sox_header[8];
	sox_header[0] = samples << 1;
	sox_header[1] = samples >> 7;
	sox_header[2] = samples >> 15;
	sox_header[3] = samples >> 23;
	sox_header[4] = samples >> 31;
	sox_header[5] = samples >> 39;
	sox_header[6] = samples >> 47;
	sox_header[7] = samples >> 55;
	strm.seekp(8, std::ios::beg);
	if(!strm)
		throw std::runtime_error("Can't seek to fix .sox header");
	strm.write(reinterpret_cast<char*>(sox_header), 8);
	if(!strm)
		throw std::runtime_error("Can't fix audio header");
}

void sox_output::add_sample(unsigned char* buf)
{
	strm.write(reinterpret_cast<char*>(buf), 8);
	if(!strm)
		throw std::runtime_error("Can't write audio sample");
	samples++;
}

uint64_t sox_output::get_samples()
{
	return samples;
}

sdmp_input_stream::sdmp_input_stream(std::istream& sdmp)
	: strm(sdmp)
{
	unsigned char header[12];
	strm.read(reinterpret_cast<char*>(header), 12);
	if(!strm)
		throw std::runtime_error("Can't read sdump header");
	if(header[0] != 'S' || header[1] != 'D' || header[2] != 'M' || header[3] != 'P')
		throw std::runtime_error("Bad sdump magic");
	cpurate = (static_cast<uint32_t>(header[4]) << 24) |
		(static_cast<uint32_t>(header[5]) << 16) |
		(static_cast<uint32_t>(header[6]) << 8) |
		static_cast<uint32_t>(header[7]);
	apurate = (static_cast<uint32_t>(header[8]) << 24) |
		(static_cast<uint32_t>(header[9]) << 16) |
		(static_cast<uint32_t>(header[10]) << 8) |
		static_cast<uint32_t>(header[11]);
}

uint32_t sdmp_input_stream::get_cpurate()
{
	return cpurate;
}

uint32_t sdmp_input_stream::get_apurate()
{
	return apurate;
}

int sdmp_input_stream::read_command()
{
	unsigned char cmd;
	strm >> cmd;
	if(!strm)
		return -1;
	return cmd;
}

void sdmp_input_stream::read_linepair(unsigned char* buffer)
{
	strm.read(reinterpret_cast<char*>(buffer), 4096);
	if(!strm)
		throw std::runtime_error("Can't read picture payload");
}

void sdmp_input_stream::copy_audio_sample(sox_output& audio_out)
{
	unsigned char ibuf[4];
	unsigned char obuf[8];
	strm.read(reinterpret_cast<char*>(ibuf), 4);
	if(!strm)
		throw std::runtime_error("Can't read sound packet payload");
	obuf[0] = 0;
	obuf[1] = 0;
	obuf[2] = ibuf[1];
	obuf[3] = ibuf[0];
	obuf[4] = 0;
	obuf[5] = 0;
	obuf[6] = ibuf[3];
	obuf[7] = ibuf[2];
	audio_out.add_sample(obuf);
}

time_tracker::time_tracker(uint32_t _cpurate)
{
	cpurate = _cpurate;
	w = n = 0;
}

void time_tracker::add_2s()
{
	w += 2000;
}

void time_tracker::advance(bool pal, bool interlaced)
{
	uint64_t tcc = pal ? 425568000 : (interlaced ? 357368000 : 357366000);
	w += tcc / cpurate;
	n += tcc % cpurate;
	w += n / cpurate;
	n %= cpurate;
}

uint64_t time_tracker::get_ts()
{
	return w;
}

dup_tracker::dup_tracker(std::ostream& _tcfile, uint32_t _flags)
	: tcfile(_tcfile)
{
	flags = _flags;
	counter = 0;
}

uint32_t dup_tracker::process(unsigned char* buffer, size_t bufsize, int pkt_type, time_tracker& ts)
{
	if(flags & FLAG_DEDUP) {
		if(memcmp(buffer, old_yuv_buffer, bufsize)) {
			memcpy(old_yuv_buffer, buffer, bufsize);
			counter = 0;
		} else
			counter = (counter + 1) % MAX_DEDUP;
		if(counter)
			return 0;
		else {
			tcfile << ts.get_ts() << std::endl;
			if(!tcfile)
				throw std::runtime_error("Can't write frame timestamp");
			return 1;
		}
	}
	if(pkt_type & CMD_PAL)
		return 1;		//No wrong framerate correction in PAL mode.
	bool framerate_flag = (flags & FLAG_FRAMERATE);
	bool interlaced = (pkt_type & CMD_INTERLACED);
	if(!framerate_flag && interlaced) {
		//This uses 357368 TU instead of 357366 TU.
		//-> Every 178683rd frame is duplicated.
		counter = (counter + 1) % 178683;
		if(counter)
			return 2;
	}
	if(framerate_flag && !interlaced) {
		//This uses 357366 TU instead of 357368 TU.
		//-> Every 178684th frame is dropped.
		counter = (counter + 1) % 178684;
		if(!counter)
			return 0;
	}
	return 1;
}

size_t calculate_line_separation(int32_t flags, int pkt_type)
{
	size_t s = 256;
	if(flags & FLAG_AR_CORRECT)
		if(pkt_type & CMD_PAL)
			s = (flags & FLAG_WIDTH) ? 640 : 320;
		else
			s = (flags & FLAG_WIDTH) ? 598 : 298;
	else if(flags & FLAG_WIDTH)
			s *= 2;
	if(flags & FLAG_HEIGHT)
		s *= 2;
	if(!(flags & FLAG_8BIT))
		s *= 2;
	return s;
}

size_t calculate_plane_separation(int32_t flags, int pkt_type)
{
	size_t s = calculate_line_separation(flags, pkt_type);
	if(pkt_type & CMD_PAL)
		s *= 240;
	else
		s *= 224;
	return s;
}

bool is_renderable_line(int pkt_type, unsigned line_pair, unsigned rendered)
{
	switch(pkt_type & (CMD_OVERSCAN | CMD_PAL)) {
	case 0:
		return (line_pair >= 9 && rendered < 224);
	case CMD_OVERSCAN:
		return (line_pair >= 16 && rendered < 224);
	case CMD_PAL:
		return (line_pair >= 1 && rendered < 239);
	case CMD_PAL | CMD_INTERLACED:
		return (line_pair >= 9 && rendered < 239);
	};
	return false;
}

void lanczos_256_298_16(unsigned short* dst, unsigned short* src);
void lanczos_256_320_16(unsigned short* dst, unsigned short* src);
void lanczos_512_598_16(unsigned short* dst, unsigned short* src);
void lanczos_512_640_16(unsigned short* dst, unsigned short* src);
void lanczos_256_298_8(unsigned char* dst, unsigned char* src);
void lanczos_256_320_8(unsigned char* dst, unsigned char* src);
void lanczos_512_598_8(unsigned char* dst, unsigned char* src);
void lanczos_512_640_8(unsigned char* dst, unsigned char* src);

void do_lanczos(unsigned char* dst, unsigned char* src, bool xhi, bool yhi, bool pal, bool lc, size_t psep)
{
	unsigned char* dstN[3];
	unsigned short* dstW[3];
	unsigned char* srcN[3];
	unsigned short* srcW[3];
	dstN[0] = dst;
	dstN[1] = dst + psep;
	dstN[2] = dst + 2 * psep;
	dstW[0] = reinterpret_cast<unsigned short*>(dst);
	dstW[1] = reinterpret_cast<unsigned short*>(dst + psep);
	dstW[2] = reinterpret_cast<unsigned short*>(dst + 2 * psep);
	srcN[0] = src;
	srcN[1] = src + 2048;
	srcN[2] = src + 4096;
	srcW[0] = reinterpret_cast<unsigned short*>(src);
	srcW[1] = reinterpret_cast<unsigned short*>(src + 2048);
	srcW[2] = reinterpret_cast<unsigned short*>(src + 4096);
	unsigned doffset = pal ? (xhi ? 640 : 320) : (xhi ? 598 : 298);
	unsigned soffset = xhi ? 512 : 256;
	void (*fn8)(unsigned char* dst, unsigned char* src);
	void (*fn16)(unsigned short* dst, unsigned short* src);
	if(xhi && pal) {
		fn8 = lanczos_512_640_8;
		fn16 = lanczos_512_640_16;
	} else if(xhi && !pal) {
		fn8 = lanczos_512_598_8;
		fn16 = lanczos_512_598_16;
	} else if(!xhi && pal) {
		fn8 = lanczos_256_320_8;
		fn16 = lanczos_256_320_16;
	} else if(!xhi && !pal) {
		fn8 = lanczos_256_298_8;
		fn16 = lanczos_256_298_16;
	}
	if(lc) {
		for(unsigned i = 0; i < 3; i++)
			fn8(dstN[i], srcN[i]);
		if(yhi)
			for(unsigned i = 0; i < 3; i++)
				fn8(dstN[i] + doffset, srcN[i] + soffset);
	} else {
		for(unsigned i = 0; i < 3; i++)
			fn16(dstW[i], srcW[i]);
		if(yhi)
			for(unsigned i = 0; i < 3; i++)
				fn16(dstW[i] + doffset, srcW[i] + soffset);
	}
}

void call_render_yuv(unsigned char* buffer, unsigned char* buf, size_t psep, int pkt_type, int32_t flags)
{
	render_yuv_t render_yuv = get_renderer_for(flags);
	unsigned char tmp[6144];
	if(flags & FLAG_AR_CORRECT) {
		render_yuv(tmp, buf, 2048, pkt_type & CMD_HIRES, pkt_type & CMD_INTERLACED);
		bool xhi = (flags & FLAG_WIDTH);
		bool yhi = (flags & FLAG_HEIGHT);
		bool pal = (pkt_type & CMD_PAL);
		bool lc = (flags & FLAG_8BIT);
		do_lanczos(buffer, tmp, xhi, yhi, pal, lc, psep);
	} else
		render_yuv(buffer, buf, psep, pkt_type & CMD_HIRES, pkt_type & CMD_INTERLACED);
}

size_t render_frame(sdmp_input_stream& in, int pkt_type, int32_t flags, unsigned char* buffer)
{
	unsigned char buf[4096];
	unsigned physline = 0;
	size_t psep = calculate_plane_separation(flags, pkt_type);
	size_t lsep = calculate_line_separation(flags, pkt_type);
	for(unsigned i = 0; i < 256; i++) {
		in.read_linepair(buf);
		if(!is_renderable_line(pkt_type, i, physline))
			continue;
		call_render_yuv(buffer + physline * lsep, buf, psep, pkt_type, flags);
		physline++;
	}
	if(pkt_type & CMD_PAL) {
		//Render a black line to pad the image.
		memset(buf, 0, 4096);
		call_render_yuv(buffer + physline * lsep, buf, psep, pkt_type, flags);
	}
	return 3 * psep;
}

void write_frame(std::ostream& yout, unsigned char* buffer, size_t bufsize, unsigned times_ctr, uint64_t& frames)
{
	for(unsigned k = 0; k < times_ctr; k++)
		yout.write(reinterpret_cast<char*>(buffer), bufsize);
	if(!yout)
		throw std::runtime_error("Can't write frame");
	frames += times_ctr;
}

void sdump2sox(std::istream& in, std::ostream& yout, std::ostream& sout, std::ostream& tout, int32_t flags)
{
	sdmp_input_stream sdmp_in(in);
	sox_output sox_out(sout, sdmp_in.get_apurate(), flags & FLAG_OFFSET2);
	time_tracker ts(sdmp_in.get_cpurate());
	dup_tracker dupt(tout, flags);

	load_rgb2yuv_matrix(flags);

	if(flags & FLAG_OFFSET2)
		ts.add_2s();
	if(flags & FLAG_DEDUP) {
		tout << "# timecode format v2" << std::endl;
		if(flags & FLAG_10FRAMES)
			tout << "0\n200\n400\n600\n800\n1000\n1200\n1400\n1600\n1800" << std::endl;
	}

	uint64_t frames = 0;
	bool is_pal = false;

	while(true) {
		bool lf = false;
		int pkttype = sdmp_in.read_command();
		if(pkttype < 0)
			break;	//End of stream.
		if((pkttype & 0xF0) == 0) {
			//Picture. Read the 1MiB of picture data one line pair at a time.
			size_t fsize = render_frame(sdmp_in, pkttype, flags, yuv_buffer);
			is_pal = is_pal || (pkttype & CMD_PAL);
			uint32_t times = dupt.process(yuv_buffer, fsize, pkttype, ts);
			write_frame(yout, yuv_buffer, fsize, times, frames);
			ts.advance(pkttype & CMD_PAL, pkttype & CMD_INTERLACED);
			lf = true;
		} else if(pkttype == 16) {
			sdmp_in.copy_audio_sample(sox_out);
		} else {
			std::ostringstream str;
			str << "Unknown command byte " << static_cast<unsigned>(pkttype);
			throw std::runtime_error(str.str());
		}
		if(lf && frames % 100 == 0) {
			std::cout << "\e[1G" << frames << " frames, " << sox_out.get_samples() << " samples."
				<< std::flush;
		}
	}
	sox_out.close();
	std::cout << "Sound sampling rate is " << static_cast<double>(sdmp_in.get_apurate()) / 768.0 << "Hz"
		<< std::endl;
	std::cout << "Wrote " << sox_out.get_samples() << " samples." << std::endl;
	std::cout << "Audio length is " << 768.0 * sox_out.get_samples() / sdmp_in.get_apurate() << "s." << std::endl;
	double vrate = 0;
	double vrate2 = 0;
	if(is_pal)
		vrate2 = 425568.0;
	else if(flags & FLAG_FRAMERATE)
		vrate2 = 357368.0;
	else
		vrate2 = 357366.0;
	vrate = sdmp_in.get_cpurate() / vrate2;
	std::cout << "Video frame rate is " << sdmp_in.get_cpurate() << "/" << vrate2 << "Hz" << std::endl;
	std::cout << "Wrote " << frames << " frames." << std::endl;
	std::cout << "Video length is " << frames / vrate << "s." << std::endl;
}

void syntax()
{
	std::cerr << "Syntax: sdump2sox [<options>] <input-file> <yuv-output-file> <sox-output-file> "
		<< "[<tc-output-file>]" << std::endl;
	std::cerr << "-W\tDump 512-wide instead of 256-wide." << std::endl;
	std::cerr << "-H\tDump 448/480-high instead of 224/240-high." << std::endl;
	std::cerr << "-D\tDedup the output (also uses exact timecodes)." << std::endl;
	std::cerr << "-h\tDump 512x448/480, doing blending for 512x224/240." << std::endl;
	std::cerr << "-F\tDump at interlaced framerate instead of non-interlaced (no effect if dedup)." << std::endl;
	std::cerr << "-l\tOffset timecodes by inserting 10 frames spanning 2 seconds (dedup only)." << std::endl;
	std::cerr << "-L\tOffset timecodes by 2 seconds (dedup only)." << std::endl;
	std::cerr << "-A\tDo output AR correction." << std::endl;
	std::cerr << "-f\tDump using full range instead of TV range." << std::endl;
	std::cerr << "-7\tDump using ITU.709 instead of ITU.601." << std::endl;
	std::cerr << "-2\tDump using SMPTE-240M instead of ITU.601." << std::endl;
	std::cerr << "-8\tDump using 8 bits instead of 16 bits." << std::endl;
}

void reached_main();

int main(int argc, char** argv)
{
	reached_main();
	if(argc < 4) {
		syntax();
		return 1;
	}
	uint32_t flags = 0;
	uint32_t idx1 = 0;
	uint32_t idx2 = 0;
	uint32_t idx3 = 0;
	uint32_t idx4 = 0;
	for(unsigned i = 1; i < argc; i++) {
		if(argv[i][0] == '-')
			for(unsigned j = 1; argv[i][j]; j++)
				switch(argv[i][j]) {
				case 'W':
					flags |= FLAG_WIDTH;
					break;
				case 'H':
					flags |= FLAG_HEIGHT;
					break;
				case 'F':
					flags |= FLAG_FRAMERATE;
					break;
				case 'D':
					flags |= FLAG_DEDUP;
					break;
				case 'f':
					flags |= FLAG_FULLRANGE;
					break;
				case 'h':
					flags |= (FLAG_FAKENLARGE | FLAG_WIDTH | FLAG_HEIGHT);
					break;
				case 'l':
					flags |= (FLAG_10FRAMES | FLAG_OFFSET2);
					break;
				case 'L':
					flags |= FLAG_OFFSET2;
					break;
				case 'A':
					flags |= FLAG_AR_CORRECT;
					break;
				case '7':
					if(flags & FLAG_CS_MASK) {
						syntax();
						return 1;
					}
					flags |= FLAG_ITU709;
					break;
				case '2':
					if(flags & FLAG_CS_MASK) {
						syntax();
						return 1;
					}
					flags |= FLAG_SMPTE240M;
					break;
				case '8':
					flags |= FLAG_8BIT;
					break;
				default:
					syntax();
					return 1;
				}
		else if(!idx1)
			idx1 = i;
		else if(!idx2)
			idx2 = i;
		else if(!idx3)
			idx3 = i;
		else if(!idx4)
			idx4 = i;
		else {
			syntax();
			return 1;
		}
	}
	if(idx4 && !(flags & FLAG_DEDUP)) {
		syntax();
		return 1;
	}
	std::ifstream in(argv[idx1], std::ios::in | std::ios::binary);
	if(!in) {
		std::cerr << "Error: Can't open '" << argv[idx1] << "'" << std::endl;
		return 2;
	}
	std::ofstream yout(argv[idx2], std::ios::out | std::ios::binary);
	if(!yout) {
		std::cerr << "Error: Can't open '" << argv[idx2] << "'" << std::endl;
		return 2;
	}
	std::ofstream sout(argv[idx3], std::ios::out | std::ios::binary);
	if(!sout) {
		std::cerr << "Error: Can't open '" << argv[idx3] << "'" << std::endl;
		return 2;
	}
	std::ofstream tout;
	if(flags & FLAG_DEDUP) {
		if(idx4)
			tout.open(argv[idx4], std::ios::out);
		else
			tout.open(argv[idx2] + std::string(".tc"), std::ios::out);
		if(!tout) {
			std::cerr << "Error: Can't open '" << argv[idx2] << ".tc'" << std::endl;
			return 2;
		}
	}
	try {
		sdump2sox(in, yout, sout, tout, flags);
		in.close();
		yout.close();
		sout.close();
		tout.close();
	} catch(std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		in.close();
		yout.close();
		sout.close();
		tout.close();
		return 3;
	}
	return 0;
}
