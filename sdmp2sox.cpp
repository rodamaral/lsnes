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

//Heh, this happens to be exact hardware capacity of 1.44MB 90mm floppy. :-)
unsigned char yuv_buffer[1474560];

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

#define STORE16(buffer,idx,shift,psep,v1,v2,v3)\
	buffer[(idx)] = (v1 >> ((shift) + 8));\
	buffer[(idx) + 1] = (v1 >> (shift));\
	buffer[(idx) + (psep)] = (v2 >> ((shift) + 8));\
	buffer[(idx) + 1 + (psep)] = (v2 >> (shift));\
	buffer[(idx) + 2 * (psep)] = (v3 >> ((shift) + 8));\
	buffer[(idx) + 1 + 2 * (psep)] = (v3 >> (shift));

#define STORE8(buffer,idx,shift,psep,v1,v2,v3)\
	buffer[(idx)] = (v1 >> ((shift) + 8));\
	buffer[(idx) + (psep)] = (v2 >> ((shift) + 8));\
	buffer[(idx) + 2 * (psep)] = (v3 >> ((shift) + 8);\

template<unsigned shift>
struct store16
{
	static const size_t esize = 2;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		*reinterpret_cast<uint16_t*>(buffer + idx) = (v1 >> shift);
		*reinterpret_cast<uint16_t*>(buffer + idx + psep) = (v2 >> shift);
		*reinterpret_cast<uint16_t*>(buffer + idx + 2 * psep) = (v3 >> shift);
	}
};

template<unsigned shift>
struct store8
{
	static const size_t esize = 1;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		buffer[idx] = (v1 >> (shift + 8));
		buffer[idx + psep] = (v2 >> (shift + 8));
		buffer[idx + 2 * psep] = (v3 >> (shift + 8));
	}
};

template<typename T>
struct store_11
{
	static const size_t esize = T::esize;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		T::store(buffer, idx, psep, v1, v2, v3);
	}
};

template<typename T>
struct store_12
{
	static const size_t esize = 2 * T::esize;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		T::store(buffer, idx, psep, v1, v2, v3);
		T::store(buffer, idx + T::esize, psep, v1, v2, v3);
	}
};

template<typename T, size_t llen>
struct store_21
{
	static const size_t esize = T::esize;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		T::store(buffer, idx, psep, v1, v2, v3);
		T::store(buffer, idx + T::esize * llen, psep, v1, v2, v3);
	}
};

template<typename T, size_t llen>
struct store_22
{
	static const size_t esize = 2 * T::esize;
	static void store(unsigned char* buffer, size_t idx, size_t psep, uint32_t v1, uint32_t v2,
		uint32_t v3)
	{
		T::store(buffer, idx, psep, v1, v2, v3);
		T::store(buffer, idx + T::esize, psep, v1, v2, v3);
		T::store(buffer, idx + 2 * T::esize * llen, psep, v1, v2, v3);
		T::store(buffer, idx + 2 * T::esize * llen + T::esize, psep, v1, v2, v3);
	}
};

template<typename T>
struct convert_11
{
	static const size_t isize = 4;
	static const size_t esize = 2 * T::esize;
	static void convert(unsigned char* obuffer, size_t oidx, size_t psep, const unsigned char* ibuffer,
		size_t iidx)
	{
		uint32_t Y = 0;
		uint32_t Cb = 0;
		uint32_t Cr = 0;
		TOYUV(ibuffer, iidx);
		T::store(obuffer, oidx, psep, Y, Cb, Cr);
	}
};

template<typename T, size_t s>
struct convert_x_helper
{
	static void convert(unsigned char* obuffer, size_t oidx, size_t psep, const unsigned char* ibuffer,
		size_t iidx)
	{
		uint32_t Y = 0;
		uint32_t Cb = 0;
		uint32_t Cr = 0;
		TOYUV(ibuffer, iidx);
		TOYUV(ibuffer, iidx + s);
		T::store(obuffer, oidx, psep, Y, Cb, Cr);
	}
};

template<typename T>
struct convert_12
{
	static const size_t isize = 8;
	static const size_t esize = T::esize;
	static void convert(unsigned char* obuffer, size_t oidx, size_t psep, const unsigned char* ibuffer,
		size_t iidx)
	{
		convert_x_helper<T, 4>::convert(obuffer, oidx, psep, ibuffer, iidx);
	}
};

template<typename T>
struct convert_21
{
	static const size_t isize = 4;
	static const size_t esize = T::esize;
	static void convert(unsigned char* obuffer, size_t oidx, size_t psep, const unsigned char* ibuffer,
		size_t iidx)
	{
		convert_x_helper<T, 2048>::convert(obuffer, oidx, psep, ibuffer, iidx);
	}
};

template<typename T>
struct convert_22
{
	static const size_t isize = 8;
	static const size_t esize = T::esize;
	static void convert(unsigned char* obuffer, size_t oidx, size_t psep, const unsigned char* ibuffer,
		size_t iidx)
	{
		uint32_t Y = 0;
		uint32_t Cb = 0;
		uint32_t Cr = 0;
		TOYUV(ibuffer, iidx);
		TOYUV(ibuffer, iidx + 4);
		TOYUV(ibuffer, iidx + 2048);
		TOYUV(ibuffer, iidx + 2052);
		T::store(obuffer, oidx, psep, Y, Cb, Cr);
	}
};

template<typename T, size_t ents>
struct convert_line
{
	static void convert(unsigned char* obuffer, size_t psep, const unsigned char* ibuffer)
	{
		for(unsigned i = 0; i < ents; i++)
			T::convert(obuffer, T::esize * i, psep, ibuffer, T::isize * i);
	}
};

#define RGB2YUV_SHIFT 14

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
		uint32_t l = (i >> 15) & 0xF;
		//Range of (r,g,b) is 0...465.
		uint32_t r = (l * ((i >> 0) & 0x1F));
		uint32_t g = (l * ((i >> 5) & 0x1F));
		uint32_t b = (l * ((i >> 10) & 0x1F));
		double Y = (RY * r + GY * g + BY * b) / 465 * (fullrange ? 255 : 219) + (fullrange ? 0 : 16);
		double Cb = (RPb * r + GPb * g + BPb * b) / 465 * (fullrange ? 255 : 224) + 128;
		double Cr = (RPr * r + GPr * g + BPr * b) / 465 * (fullrange ? 255 : 224) + 128;
		ymatrix[i] = static_cast<uint32_t>(Y * 4194304 + 0.5);
		cbmatrix[i] = static_cast<uint32_t>(Cb * 4194304 + 0.5);
		crmatrix[i] = static_cast<uint32_t>(Cr * 4194304 + 0.5);
	}
}

//Load RGB to YUV conversion matrix.
void load_rgb2yuv_matrix(uint32_t flags)
{
	switch(flags & (FLAG_CS_MASK | FLAG_FULLRANGE))
	{
	case FLAG_ITU601:
		init_matrix(0.114, 0.229, false);
		break;
	case FLAG_ITU601 | FLAG_FULLRANGE:
		init_matrix(0.114, 0.229, true);
		break;
	case FLAG_ITU709:
		init_matrix(0.0722, 0.2126, false);
		break;
	case FLAG_ITU709 | FLAG_FULLRANGE:
		init_matrix(0.0722, 0.2126, true);
		break;
	case FLAG_SMPTE240M:
		init_matrix(0.087, 0.212, false);
		break;
	case FLAG_SMPTE240M | FLAG_FULLRANGE:
		init_matrix(0.087, 0.212, true);
		break;
	default:
		init_matrix(0.114, 0.229, false);
		break;
	}
}

//Render a line pair of YUV.
void render_yuv(unsigned char* buffer, const unsigned char* src, size_t psep, uint32_t flags, bool hires,
	bool interlaced)
{
	unsigned c = 0;
	if(flags & FLAG_WIDTH)
		c |= 1;
	if(flags & FLAG_HEIGHT)
		c |= 2;
	if(hires)
		c |= 4;
	if(interlaced)
		c |= 8;
	if(flags & FLAG_8BIT)
		c |= 16;
	switch(c) {
	case 0: {		//256 x 224/240 -> 256 x 224/240 16 bit.
		convert_line<convert_11<store_11<store16<14>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 1: {		//256 x 224/240 -> 512 x 224/240 16 bit.
		convert_line<convert_11<store_12<store16<14>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 2: {		//256 x 224/240 -> 256 x 448/480 16 bit.
		convert_line<convert_11<store_21<store16<14>, 256>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 3: {		//256 x 224/240 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_22<store16<14>, 256>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 4: {		//512 x 224/240 -> 256 x 224/240 16 bit.
		convert_line<convert_12<store_11<store16<15>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 5: {		//512 x 224/240 -> 512 x 224/240 16 bit.
		convert_line<convert_11<store_11<store16<14>>>, 512>::convert(buffer, psep, src);
		break;
	}
	case 6: {		//512 x 224/240 -> 256 x 448/480 16 bit.
		convert_line<convert_12<store_21<store16<15>, 256>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 7: {		//512 x 224/240 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_21<store16<14>, 512>>, 512>::convert(buffer, psep, src);
		break;
	}
	case 8: {		//256 x 448x480 -> 256 x 224/240 16 bit.
		convert_line<convert_21<store_11<store16<15>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 9: {		//256 x 448x480 -> 512 x 224/240 16 bit.
		convert_line<convert_21<store_12<store16<15>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 10: {		//256 x 448x480 -> 256 x 448/480 16 bit.
		convert_line<convert_11<store_11<store16<14>>>, 256>::convert(buffer, psep, src);
		convert_line<convert_11<store_11<store16<14>>>, 256>::convert(buffer + 512, psep, src + 2048);
		break;
	}
	case 11: {		//256 x 448x480 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_12<store16<14>>>, 256>::convert(buffer, psep, src);
		convert_line<convert_11<store_12<store16<14>>>, 256>::convert(buffer + 1024, psep, src + 2048);
		break;
	}
	case 12: {		//512 x 448x480 -> 256 x 224/240 16 bit.
		convert_line<convert_22<store_11<store16<16>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 13: {		//512 x 448x480 -> 512 x 224/240 16 bit.
		convert_line<convert_21<store_11<store16<15>>>, 512>::convert(buffer, psep, src);
		break;
	}
	case 14: {		//512 x 448x480 -> 256 x 448/480 16 bit.
		convert_line<convert_11<store_21<store16<14>, 256>>, 256>::convert(buffer, psep, src);
		convert_line<convert_11<store_21<store16<14>, 256>>, 256>::convert(buffer + 512, psep, src + 2048);
		break;
	}
	case 15: {		//512 x 448x480 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_11<store16<14>>>, 512>::convert(buffer, psep, src);
		convert_line<convert_11<store_11<store16<14>>>, 512>::convert(buffer + 1024, psep, src + 2048);
		break;
	}
	case 16: {		//256 x 224/240 -> 256 x 224/240 16 bit.
		convert_line<convert_11<store_11<store8<14>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 17: {		//256 x 224/240 -> 512 x 224/240 16 bit.
		convert_line<convert_11<store_12<store8<14>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 18: {		//256 x 224/240 -> 256 x 448/480 16 bit.
		convert_line<convert_11<store_21<store8<14>, 256>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 19: {		//256 x 224/240 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_22<store8<14>, 256>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 20: {		//512 x 224/240 -> 256 x 224/240 16 bit.
		convert_line<convert_12<store_11<store8<15>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 21: {		//512 x 224/240 -> 512 x 224/240 16 bit.
		convert_line<convert_11<store_11<store8<14>>>, 512>::convert(buffer, psep, src);
		break;
	}
	case 22: {		//512 x 224/240 -> 256 x 448/480 16 bit.
		convert_line<convert_12<store_21<store8<15>, 256>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 23: {		//512 x 224/240 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_21<store8<14>, 512>>, 512>::convert(buffer, psep, src);
		break;
	}
	case 24: {		//256 x 448x480 -> 256 x 224/240 16 bit.
		convert_line<convert_21<store_11<store8<15>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 25: {		//256 x 448x480 -> 512 x 224/240 16 bit.
		convert_line<convert_21<store_12<store8<15>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 26: {		//256 x 448x480 -> 256 x 448/480 16 bit.
		convert_line<convert_11<store_11<store8<14>>>, 256>::convert(buffer, psep, src);
		convert_line<convert_11<store_11<store8<14>>>, 256>::convert(buffer + 256, psep, src + 2048);
		break;
	}
	case 27: {		//256 x 448x480 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_12<store8<14>>>, 256>::convert(buffer, psep, src);
		convert_line<convert_11<store_12<store8<14>>>, 256>::convert(buffer + 512, psep, src + 2048);
		break;
	}
	case 28: {		//512 x 448x480 -> 256 x 224/240 16 bit.
		convert_line<convert_22<store_11<store8<16>>>, 256>::convert(buffer, psep, src);
		break;
	}
	case 29: {		//512 x 448x480 -> 512 x 224/240 16 bit.
		convert_line<convert_21<store_11<store8<15>>>, 512>::convert(buffer, psep, src);
		break;
	}
	case 30: {		//512 x 448x480 -> 256 x 448/480 16 bit.
		convert_line<convert_11<store_21<store8<14>, 256>>, 256>::convert(buffer, psep, src);
		convert_line<convert_11<store_21<store8<14>, 256>>, 256>::convert(buffer + 256, psep, src + 2048);
		break;
	}
	case 31: {		//512 x 448x480 -> 512 x 448/480 16 bit.
		convert_line<convert_11<store_11<store8<14>>>, 512>::convert(buffer, psep, src);
		convert_line<convert_11<store_11<store8<14>>>, 512>::convert(buffer + 512, psep, src + 2048);
		break;
	}
	};
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

void sdump2sox(std::istream& in, std::ostream& yout, std::ostream& sout, uint32_t flags)
{
	unsigned char header[12];
	in.read(reinterpret_cast<char*>(header), 12);
	if(!in)
		throw std::runtime_error("Can't read sdump header");
	if(header[0] != 'S' || header[1] != 'D' || header[2] != 'M' || header[3] != 'P') 
		throw std::runtime_error("Bad sdump magic");
	uint32_t apurate;
	uint32_t cpurate;
	cpurate = (static_cast<uint32_t>(header[4]) << 24) |
		(static_cast<uint32_t>(header[5]) << 16) |
		(static_cast<uint32_t>(header[6]) << 8) |
		static_cast<uint32_t>(header[7]);
	apurate = (static_cast<uint32_t>(header[8]) << 24) |
		(static_cast<uint32_t>(header[9]) << 16) |
		(static_cast<uint32_t>(header[10]) << 8) |
		static_cast<uint32_t>(header[11]);
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
	sout.write(reinterpret_cast<char*>(sox_header), 32);
	if(!sout)
		throw std::runtime_error("Can't write audio header");
	uint64_t samples = 0;
	uint64_t frames = 0;
	unsigned wrongrate = 0;
	bool is_pal = false;
	load_rgb2yuv_matrix(flags);
	while(true) {
		unsigned char cmd;
		bool lf = false;
		in >> cmd;
		if(!in)
			break;	//End of stream.
		if((cmd & 0xF0) == 0) {
			//Pictrue. Read the 1MiB of picture data one line pair at a time.
			unsigned char buf[4096];
			unsigned physline = 0;
			bool hires = (cmd & 1);
			bool interlaced = (cmd & 2);
			bool overscan = (cmd & 4);
			bool pal = (cmd & 8);
			bool ohires = (flags & FLAG_WIDTH);
			bool ointerlaced = (flags & FLAG_HEIGHT);
			bool bits8 = (flags & FLAG_8BIT);
			size_t psep = (ohires ? 512 : 256) * (ointerlaced ? 2 : 1) * (pal ? 240 : 224) *
				(bits8 ? 1 : 2);
			size_t lsep = (ohires ? 512 : 256) * (ointerlaced ? 2 : 1) * (bits8 ? 1 : 2);
			for(unsigned i = 0; i < 256; i++) {
				in.read(reinterpret_cast<char*>(buf), 4096);
				if(!in)
					throw std::runtime_error("Can't read picture payload");
				is_pal = is_pal || pal;
				if(overscan && i < 9)
					continue;
				if(!overscan && i < 1)
					continue;
				if(pal & physline >= 239)
					continue;
				if(!pal & physline >= 224)
					continue;
				render_yuv(yuv_buffer + physline * lsep, buf, psep, flags, hires, interlaced);
				physline++;
			}
			if(pal) {
				//Render a black line to pad the image.
				memset(buf, 0, 4096);
				render_yuv(yuv_buffer + 239 * lsep, buf, psep, flags, hires, interlaced);
			}
			size_t yuvsize = 3 * psep;
			unsigned times = 1;
			if((flags & FLAG_FRAMERATE) == 0 && !is_pal && interlaced) {
				//This uses 357368 TU instead of 357366 TU.
				//-> Every 178683rd frame is duplicated.
				if(wrongrate == 178682) {
					times = 2;
					wrongrate = 0;
				} else
					wrongrate++;
			}
			if((flags & FLAG_FRAMERATE) != 0 && !is_pal && !interlaced) {
				//This uses 357366 TU instead of 357368 TU.
				//-> Every 178684th frame is dropped.
				if(wrongrate == 178683) {
					times = 0;
					wrongrate = 0;
				} else
					wrongrate++;
			}
			for(unsigned k = 0; k < times; k++)
				yout.write(reinterpret_cast<char*>(yuv_buffer), yuvsize);
			if(!yout)
				throw std::runtime_error("Can't write frame");
			frames += times;
			lf = true;
		} else if(cmd == 16) {
			//Sound packet. Interesting.
			unsigned char ibuf[4];
			unsigned char obuf[8];
			in.read(reinterpret_cast<char*>(ibuf), 4);
			if(!in)
				throw std::runtime_error("Can't read sound packet payload");
			obuf[0] = 0;
			obuf[1] = 0;
			obuf[2] = ibuf[1];
			obuf[3] = ibuf[0];
			obuf[4] = 0;
			obuf[5] = 0;
			obuf[6] = ibuf[3];
			obuf[7] = ibuf[2];
			sout.write(reinterpret_cast<char*>(obuf), 8);
			if(!sout)
				throw std::runtime_error("Can't write audio sample");
			samples++;
		} else {
			std::ostringstream str;
			str << "Unknown command byte " << static_cast<unsigned>(cmd);
			throw std::runtime_error(str.str());
		}
		if(lf && frames % 100 == 0) {
			std::cout << "\e[1G" << frames << " frames, " << samples << " samples." << std::flush;
		}
	}
	//Sox internally multiplies sample count by channel count.
	sox_header[8] = samples << 1;
	sox_header[9] = samples >> 7;
	sox_header[10] = samples >> 15;
	sox_header[11] = samples >> 23;
	sox_header[12] = samples >> 31;
	sox_header[13] = samples >> 39;
	sox_header[14] = samples >> 47;
	sox_header[15] = samples >> 55;
	sout.seekp(0, std::ios::beg);
	if(!sout)
		throw std::runtime_error("Can't seek to fix .sox header");
	sout.write(reinterpret_cast<char*>(sox_header), 32);
	if(!sout)
		throw std::runtime_error("Can't fix audio header");
	std::cout << "Sound sampling rate is " << static_cast<double>(apurate) / 768.0 << "Hz" << std::endl;
	std::cout << "Wrote " << samples << " samples." << std::endl;
	std::cout << "Audio length is " << 768.0 * samples / apurate << "s." << std::endl;
	double vrate = 0;
	double vrate2 = 0;
	if(is_pal)
		vrate2 = 425568.0;
	else if(flags & FLAG_FRAMERATE)
		vrate2 = 357368.0;
	else
		vrate2 = 357366.0;
	vrate = cpurate / vrate2;
	std::cout << "Video frame rate is " << cpurate << "/" << vrate2 << "Hz" << std::endl;
	std::cout << "Wrote " << frames << " frames." << std::endl;
	std::cout << "Video length is " << frames / vrate << "s." << std::endl;
}

void syntax()
{
	std::cerr << "Syntax: sdump2sox [<options>] <input-file> <yuv-output-file> <sox-output-file>" << std::endl;
	std::cerr << "-W\tDump 512-wide instead of 256-wide." << std::endl;
	std::cerr << "-H\tDump 448/480-high instead of 224/240-high." << std::endl;
	std::cerr << "-F\tDump at interlaced framerate instead of non-interlaced." << std::endl;
	std::cerr << "-f\tDump using full range instead of TV range." << std::endl;
	std::cerr << "-7\tDump using ITU.709 instead of ITU.601." << std::endl;
	std::cerr << "-2\tDump using SMPTE-240M instead of ITU.601." << std::endl;
	std::cerr << "-8\tDump using 8 bits instead of 16 bits." << std::endl;
}

int main(int argc, char** argv)
{
	if(argc < 4) {
		syntax();
		return 1;
	}
	uint32_t flags = 0;
	uint32_t idx1 = 0;
	uint32_t idx2 = 0;
	uint32_t idx3 = 0;
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
				case 'f':
					flags |= FLAG_FULLRANGE;
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
		else {
			syntax();
			return 1;
		}
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
	try {
		sdump2sox(in, yout, sout, flags);
		in.close();
		yout.close();
		sout.close();
	} catch(std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		in.close();
		yout.close();
		sout.close();
		return 3;
	}
	return 0;
}
