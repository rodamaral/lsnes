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

#define MAX_DEDUP 9

//Heh, this happens to be exact hardware capacity of 1.44MB 90mm floppy. :-)
//This buffer needs to be big enough to store 512x480 16-bit YCbCr 4:4:4 (6 bytes per pixel) image.
unsigned char yuv_buffer[1474560];
unsigned char old_yuv_buffer[1474560];

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

void sdump2sox(std::istream& in, std::ostream& yout, std::ostream& sout, std::ostream& tout, int32_t flags)
{
	unsigned elided = 0;
	uint64_t ftcw = 0;
	uint64_t ftcn = 0;
	if(flags & FLAG_OFFSET2)
		ftcw += 2000;
	if(flags & FLAG_DEDUP) {
		tout << "# timecode format v2" << std::endl;
		if(flags & FLAG_10FRAMES)
			tout << "0\n200\n400\n600\n800\n1000\n1200\n1400\n1600\n1800" << std::endl;
	}
	void (*render_yuv)(unsigned char* buffer, const unsigned char* src, size_t psep, bool hires, bool interlaced);
	switch(flags & (FLAG_WIDTH | FLAG_HEIGHT | FLAG_8BIT | FLAG_FAKENLARGE)) {
	case 0:
		render_yuv = render_yuv_256_240<store16>;
		break;
	case FLAG_WIDTH:
		render_yuv = render_yuv_512_240<store16>;
		break;
	case FLAG_HEIGHT:
		render_yuv = render_yuv_256_480<store16>;
		break;
	case FLAG_WIDTH | FLAG_HEIGHT:
		render_yuv = render_yuv_512_480<store16>;
		break;
	case FLAG_WIDTH | FLAG_HEIGHT | FLAG_FAKENLARGE:
		render_yuv = render_yuv_fe<store16>;
		break;
	case FLAG_8BIT:
		render_yuv = render_yuv_256_240<store8>;
		break;
	case FLAG_WIDTH | FLAG_8BIT:
		render_yuv = render_yuv_512_240<store8>;
		break;
	case FLAG_HEIGHT | FLAG_8BIT:
		render_yuv = render_yuv_256_480<store8>;
		break;
	case FLAG_WIDTH | FLAG_HEIGHT | FLAG_8BIT:
		render_yuv = render_yuv_512_480<store8>;
		break;
	case FLAG_WIDTH | FLAG_HEIGHT | FLAG_FAKENLARGE | FLAG_8BIT:
		render_yuv = render_yuv_fe<store8>;
		break;
	}
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
	if(flags & FLAG_OFFSET2) {
		uint64_t nullsamples = apurate / 384;
		const size_t bufsz = 512;
		char nbuffer[8 * bufsz] = {0};
		while(nullsamples > bufsz) {
			sout.write(nbuffer, 8 * bufsz);
			nullsamples -= bufsz;
		}
		sout.write(nbuffer, 8 * nullsamples);
		if(!sout)
			throw std::runtime_error("Can't write 2 second silence");
	}
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
				render_yuv(yuv_buffer + physline * lsep, buf, psep, hires, interlaced);
				physline++;
			}
			if(pal) {
				//Render a black line to pad the image.
				memset(buf, 0, 4096);
				render_yuv(yuv_buffer + 239 * lsep, buf, psep, hires, interlaced);
			}
			size_t yuvsize = 3 * psep;
			unsigned times = 1;
			//If FLAG_DEDUP is set, no frames are added or dropped to match timecodes.
			if((flags & (FLAG_FRAMERATE | FLAG_DEDUP)) == 0 && !is_pal && interlaced) {
				//This uses 357368 TU instead of 357366 TU.
				//-> Every 178683rd frame is duplicated.
				if(wrongrate == 178682) {
					times = 2;
					wrongrate = 0;
				} else
					wrongrate++;
			}
			if((flags & (FLAG_FRAMERATE | FLAG_DEDUP)) == FLAG_FRAMERATE && !is_pal && !interlaced) {
				//This uses 357366 TU instead of 357368 TU.
				//-> Every 178684th frame is dropped.
				if(wrongrate == 178683) {
					times = 0;
					wrongrate = 0;
				} else
					wrongrate++;
			}
			if(flags & FLAG_DEDUP) {
				if(memcmp(old_yuv_buffer, yuv_buffer, yuvsize)) {
					memcpy(old_yuv_buffer, yuv_buffer, yuvsize);
					elided = 0;
				} else
					elided = (++elided) % MAX_DEDUP;
				if(elided)
					times = 0;
				else
					tout << ftcw << std::endl;
			}
			for(unsigned k = 0; k < times; k++)
				yout.write(reinterpret_cast<char*>(yuv_buffer), yuvsize);
			if(!yout)
				throw std::runtime_error("Can't write frame");
			frames += times;
			lf = true;
			uint64_t tcc = is_pal ? 425568000 : (interlaced ? 357368000 : 357366000);
			ftcw = ftcw + tcc / cpurate;
			ftcn = ftcn + tcc % cpurate;
			if(ftcn >= cpurate) {
				ftcw++;
				ftcn -= cpurate;
			}
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
	std::cerr << "Syntax: sdump2sox [<options>] <input-file> <yuv-output-file> <sox-output-file> "
		<< "[<tc-output-file>]" << std::endl;
	std::cerr << "-W\tDump 512-wide instead of 256-wide." << std::endl;
	std::cerr << "-H\tDump 448/480-high instead of 224/240-high." << std::endl;
	std::cerr << "-D\tDedup the output (also uses exact timecodes)." << std::endl;
	std::cerr << "-h\tDump 512x448/480, doing blending for 512x224/240." << std::endl;
	std::cerr << "-F\tDump at interlaced framerate instead of non-interlaced (no effect if dedup)." << std::endl;
	std::cerr << "-l\tOffset timecodes by inserting 10 frames spanning 2 seconds (dedup only)." << std::endl;
	std::cerr << "-L\tOffset timecodes by 2 seconds (dedup only)." << std::endl;
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
