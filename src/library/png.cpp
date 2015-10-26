#include "png.hpp"
#include "serialization.hpp"
#include "minmax.hpp"
#include "hex.hpp"
#include "zip.hpp"
#include <iostream>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <zlib.h>
#include <string.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

namespace png
{
namespace
{
	void throw_zlib_error(int x)
	{
		switch(x) {
		case Z_NEED_DICT:	throw std::runtime_error("Dictionary needed");
		case Z_ERRNO:		throw std::runtime_error(std::string("OS error: ") + strerror(errno));
		case Z_STREAM_ERROR:	throw std::runtime_error("Stream error");
		case Z_DATA_ERROR:	throw std::runtime_error("Data error");
		case Z_MEM_ERROR:	throw std::bad_alloc();
		case Z_BUF_ERROR:	throw std::runtime_error("Buffer error");
		case Z_VERSION_ERROR:	throw std::runtime_error("Version error");
		case Z_OK:
		case Z_STREAM_END:
			break;
		default:		throw std::runtime_error("Unknown error");
		};
	}

	void* zlib_alloc(void* dummy, unsigned a, unsigned b)
	{
		return calloc(a, b);
	}

	void zlib_free(void* dummy, void* addr)
	{
		free(addr);
	}

	int size_to_bits(unsigned v)
	{
		if(v > 256) return 16;
		if(v > 16) return 8;
		if(v > 4) return 4;
		if(v > 2) return 2;
		return 1;
	}

	size_t buffer_stride(size_t width, bool has_pal, bool has_trans, size_t psize)
	{
		if(!has_pal)
			return 1 + width * (has_trans ? 4 : 3);
		else
			return 1 + (width * size_to_bits(psize) + 7) / 8;
	}

	void write_row_pal1(char* output, const uint32_t* input, size_t w)
	{
		memset(output, 0, (w + 7) / 8);
		for(size_t i = 0; i < w; i++)
			output[i >> 3] |= ((input[i] & 1) << (7 -i % 8));
	}

	void write_row_pal2(char* output, const uint32_t* input, size_t w)
	{
		memset(output, 0, (w + 3) / 4);
		for(size_t i = 0; i < w; i++)
			output[i >> 2] |= ((input[i] & 3) << (2 * (3 - i % 4)));
	}

	void write_row_pal4(char* output, const uint32_t* input, size_t w)
	{
		memset(output, 0, (w + 1) / 2);
		for(size_t i = 0; i < w; i++)
			output[i >> 1] |= ((input[i] & 15) << (4 * (1 - i % 2)));
	}

	void write_row_pal8(char* output, const uint32_t* input, size_t w)
	{
		for(size_t i = 0; i < w; i++)
			output[i] = input[i];
	}

	void write_row_pal16(char* output, const uint32_t* input, size_t w)
	{
		for(size_t i = 0; i < w; i++) {
			output[2 * i + 0] = input[i] >> 8;
			output[2 * i + 1] = input[i];
		}
	}

	void write_row_rgba(char* output, const uint32_t* input, size_t w)
	{
		for(size_t i = 0; i < w; i++) {
			output[4 * i + 0] = input[i] >> 16;
			output[4 * i + 1] = input[i] >> 8;
			output[4 * i + 2] = input[i];
			output[4 * i + 3] = input[i] >> 24;
		}
	}

	void write_row_rgb(char* output, const uint32_t* input, size_t w)
	{
		for(size_t i = 0; i < w; i++) {
			output[3 * i + 0] = input[i] >> 16;
			output[3 * i + 1] = input[i] >> 8;
			output[3 * i + 2] = input[i];
		}
	}

	//=========================================================
	//==================== PNG CHUNKER ========================
	//=========================================================
	class png_chunk_output
	{
	public:
		typedef char char_type;
		struct category : boost::iostreams::closable_tag, boost::iostreams::sink_tag {};
		png_chunk_output(std::ostream& _os, uint32_t _type)
			: os(_os), type(_type)
		{
		}

		void close()
		{
			uint32_t crc = crc32(0, NULL, 0);
			char fixed[12];
			serialization::u32b(fixed, stream.size());
			serialization::u32b(fixed + 4, type);
			crc = crc32(crc, reinterpret_cast<Bytef*>(fixed + 4), 4);
			if(stream.size() > 0)
				crc = crc32(crc, reinterpret_cast<Bytef*>(&stream[0]), stream.size());
			serialization::u32b(fixed + 8, crc);
			os.write(fixed, 8);
			os.write(&stream[0], stream.size());
			os.write(fixed + 8, 4);
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			size_t oldsize = stream.size();
			stream.resize(oldsize + n);
			memcpy(&stream[oldsize], s, n);
			return n;
		}
	protected:
		std::vector<char> stream;
		std::ostream& os;
		uint32_t type;
	};


	//=========================================================
	//=================== AUTORELEASE =========================
	//=========================================================
	template <typename T>
	class autorelease
	{
	public:
		autorelease(T& _obj) : obj(_obj) {}
		~autorelease() { delete &obj; }
		T* operator->() { return &obj; }
		T& operator*() { return obj; }
	private:
		T& obj;
		autorelease(const autorelease&);
		const autorelease& operator=(const autorelease&);
	};

	//=========================================================
	//=================== PNG DECHUNKER =======================
	//=========================================================
	class png_dechunker
	{
	public:
		png_dechunker(std::istream& _stream);
		uint32_t chunk_size() { return size; }
		uint32_t chunk_type() { return type; }
		uint32_t chunk_read(uint8_t* buf, size_t limit);
		bool next_chunk();
		bool eof() { return eof_flag; }
		bool chunk_eof() { return ptr == size; }
	private:
		std::istream& stream;
		uint32_t size;
		uint32_t type;
		uint32_t ptr;
		uint32_t crc;
		bool eof_flag;
		void verify_crc();
		void load_chunk();
	};

	png_dechunker::png_dechunker(std::istream& _stream)
		: stream(_stream)
	{
		size = 0;
		type = 0;
		ptr = 0;
		eof_flag = false;
		uint8_t magic[8] = {137, 80, 78, 71, 13, 10, 26, 10};
		uint8_t magicbuf[8];
		stream.read(reinterpret_cast<char*>(magicbuf), 8);
		if(!stream || memcmp(magic, magicbuf, 8))
			throw std::runtime_error("Not a PNG file");
	}

	uint32_t png_dechunker::chunk_read(uint8_t* buf, size_t limit)
	{
		limit = min(limit, static_cast<size_t>(size - ptr));
		if(!limit)
			return 0;
		stream.read(reinterpret_cast<char*>(buf), limit);
		if(!stream)
			throw std::runtime_error("PNG file truncated");
		crc = crc32(crc, buf, limit);
		ptr += limit;
		if(ptr == size)
			verify_crc();
		return limit;
	}

	bool png_dechunker::next_chunk()
	{
		if(eof_flag)
			return false;
		while(ptr < size) {
			uint8_t buf[256];
			chunk_read(buf, 256);
		}
		load_chunk();
		return !eof_flag;
	}

	void png_dechunker::load_chunk()
	{
		uint8_t buf[8];
		stream.read(reinterpret_cast<char*>(buf), 8);
		if(!stream) {
			if(stream.gcount() == 0) {
				//EOF.
				size = 0;
				type = 0;
				ptr = 0;
				eof_flag = true;
				return;
			}
			throw std::runtime_error("PNG file truncated");
		}
		size = serialization::u32b(buf + 0);
		type = serialization::u32b(buf + 4);
		crc = crc32(0, NULL, 0);
		crc = crc32(crc, buf + 4, 4);
		ptr = 0;
		if(!size)
			verify_crc();
	}

	void png_dechunker::verify_crc()
	{
		char buf[4];
		stream.read(buf, 4);
		if(!stream)
			throw std::runtime_error("PNG file truncated");
		uint32_t claim_crc = serialization::u32b(buf);
		if(crc != claim_crc)
			throw std::runtime_error("PNG file chunk CRC check failed");
	}

	//=========================================================
	//=================== PNG IHDR CHUNK ======================
	//=========================================================
	struct ihdr_chunk
	{
		ihdr_chunk(png_dechunker& d)
		{
			if(d.chunk_type() != 0x49484452)
				throw std::runtime_error("Expected IHDR chunk");
			if(d.chunk_size() != 13)
				throw std::runtime_error("Expected IHDR chunk to be 13 bytes");
			uint8_t buf[13];
			d.chunk_read(buf, 13);
			width = serialization::u32b(buf + 0);
			height = serialization::u32b(buf + 4);
			depth = buf[8];
			type = buf[9];
			compression = buf[10];
			filter = buf[11];
			interlace = buf[12];
		}
		size_t width;
		size_t height;
		uint8_t type;
		uint8_t depth;
		uint8_t compression;
		uint8_t interlace;
		uint8_t filter;
	};

	//=========================================================
	//=================== PNG DECOMPRESSOR ====================
	//=========================================================
	class png_decompressor
	{
	public:
		png_decompressor(png_dechunker& _dechunk) : dechunk(_dechunk) {}
		virtual ~png_decompressor() {}
		virtual void decompress(uint8_t*& out, size_t& outsize) = 0;
		static png_decompressor& get(png_dechunker& dechunk, uint8_t compression);
	protected:
		png_dechunker& dechunk;
	private:
		png_decompressor(const png_decompressor&);
		png_decompressor& operator=(const png_decompressor&);
	};

	class png_decompressor_zlib : public png_decompressor
	{
	public:
		png_decompressor_zlib(png_dechunker& dechunk)
			: png_decompressor(dechunk)
		{
			memset(&z, 0, sizeof(z));
			z.zalloc = zlib_alloc;
			z.zfree = zlib_free;
			throw_zlib_error(inflateInit(&z));
			buflen = 0;
			bufptr = 0;
		}
		~png_decompressor_zlib()
		{
			inflateEnd(&z);
		}
		void decompress(uint8_t*& out, size_t& outsize)
		{
			while(true) {
				if(!buflen) {
					buflen = dechunk.chunk_read(buf, sizeof(buf));
					bufptr = 0;
				}
				z.next_in = buf + bufptr;
				z.avail_in = buflen;
				z.next_out = out;
				z.avail_out = outsize;
				int r = inflate(&z, Z_SYNC_FLUSH);
				if(r == Z_BUF_ERROR || r == Z_STREAM_END) {
					out = z.next_out;
					outsize = z.avail_out;
					bufptr = z.next_in - buf;
					buflen = z.avail_in;
					return;
				}
				throw_zlib_error(r);
				out = z.next_out;
				outsize = z.avail_out;
				bufptr = z.next_in - buf;
				buflen = z.avail_in;
			}
		}
	private:
		z_stream z;
		png_decompressor_zlib(const png_decompressor_zlib&);
		png_decompressor_zlib& operator=(const png_decompressor_zlib&);
		uint8_t buf[256];
		size_t bufptr;
		size_t buflen;
	};

	png_decompressor& png_decompressor::get(png_dechunker& dechunk, uint8_t compression)
	{
		if(compression == 0) return *new png_decompressor_zlib(dechunk);
		throw std::runtime_error("Unsupported compression method");
	}

	//=========================================================
	//=================== PNG FILTERBANK ======================
	//=========================================================
	class png_filterbank
	{
	public:
		png_filterbank(png_decompressor& _decomp, size_t _pitch, size_t _elements)
			: decomp(_decomp), pitch(_pitch), elements(_elements)
		{
		}
		virtual ~png_filterbank() {}
		size_t outsize() { return pitch * elements; }
		void adjust_row(uint8_t type, uint8_t depth, size_t width)
		{
			size_t bits = png_filterbank::get_bits(type, depth);
			size_t n_pitch = (bits >= 8) ? (bits >> 3) : 1;
			size_t pfactor = 8 / bits;
			size_t n_elements = (bits >= 8) ? width : ((width + pfactor - 1) / pfactor);
			adjusted(n_pitch, n_elements);
			pitch = n_pitch;
			elements = n_elements;
		}
		virtual bool row(uint8_t* data) = 0;
		static png_filterbank& get(png_decompressor& _decomp, uint8_t filterbank, uint8_t type, uint8_t depth,
			size_t width);
		static size_t get_bits(uint8_t type, uint8_t depth)
		{
			size_t mul[7] = {1, 0, 3, 1, 2, 0, 4};
			if(type > 6 || !mul[type]) throw std::runtime_error("Unrecognized color type");
			return mul[type] * depth;
		}
	protected:
		png_decompressor& decomp;
		size_t pitch;
		size_t elements;
		virtual void adjusted(size_t _pitch, size_t _elements) = 0;
	private:
		png_filterbank(const png_filterbank&);
		png_filterbank& operator=(const png_filterbank&);
	};

	inline uint8_t predict_none(uint8_t left, uint8_t up, uint8_t upleft)
	{
		return 0;
	}

	inline uint8_t predict_left(uint8_t left, uint8_t up, uint8_t upleft)
	{
		return left;
	}

	inline uint8_t predict_up(uint8_t left, uint8_t up, uint8_t upleft)
	{
		return up;
	}

	inline uint8_t predict_average(uint8_t left, uint8_t up, uint8_t upleft)
	{
		return (left >> 1) + (up >> 1) + (left & up & 1);
	}

	inline uint8_t predict_paeth(uint8_t left, uint8_t up, uint8_t upleft)
	{
		int16_t p = (int16_t)up + left - upleft;
		uint16_t pa = (p > left) ? (p - left) : (left - p);
		uint16_t pb = (p > up) ? (p - up) : (up - p);
		uint16_t pc = (p > upleft) ? (p - upleft) : (upleft - p);
		if(pa <= pb && pa <= pc)
			return left;
		if(pb <= pc)
			return up;
		return upleft;
	}

	template<uint8_t(*predictor)(uint8_t left, uint8_t up, uint8_t upleft)> void do_filter_3(uint8_t* target,
		const uint8_t* row, const uint8_t* above, size_t pitch, size_t length)
	{
		for(size_t i = 0; i < pitch; i++)
			target[i] = row[i] + predictor(0, above[i], 0);
		for(size_t i = pitch; i < length; i++)
			target[i] = row[i] + predictor(target[i - pitch], above[i], above[i - pitch]);
	}

	class png_filterbank_0 : public png_filterbank
	{
	public:
		png_filterbank_0(png_decompressor& decomp, size_t pitch, size_t elements)
			: png_filterbank(decomp, pitch, elements)
		{
			above.resize(pitch * elements);
			tmp.resize(pitch * elements + 1);
			tmp2 = &tmp[0];
			tmpleft = tmp.size();
		}
		~png_filterbank_0() {}
		bool row(uint8_t* data)
		{
			decomp.decompress(tmp2, tmpleft);
			if(tmpleft)
				return false;
			uint8_t filter = tmp[0];
			uint8_t* t = data;
			uint8_t* s = &tmp[1];
			uint8_t* a = &above[0];
			switch(filter) {
			case 0: do_filter_3<predict_none>(t, s, a, pitch, pitch * elements); break;
			case 1: do_filter_3<predict_left>(t, s, a, pitch, pitch * elements); break;
			case 2: do_filter_3<predict_up>(t, s, a, pitch, pitch * elements); break;
			case 3: do_filter_3<predict_average>(t, s, a, pitch, pitch * elements); break;
			case 4: do_filter_3<predict_paeth>(t, s, a, pitch, pitch * elements); break;
			default: throw std::runtime_error("Unknown filter for filter bank 0");
			};
			tmp2 = &tmp[0];
			tmpleft = tmp.size();
			memcpy(a, t, pitch * elements);
			return true;
		}
	protected:
		void adjusted(size_t _pitch, size_t _elements)
		{
			std::vector<uint8_t> nabove, ntmp;
			nabove.resize(_pitch * _elements);
			ntmp.resize(_pitch * _elements + 1);
			std::swap(above, nabove);
			std::swap(tmp, ntmp);
			tmp2 = &tmp[0];
			tmpleft = tmp.size();
		}
	private:
		png_filterbank_0(const png_filterbank_0&);
		png_filterbank_0& operator=(const png_filterbank_0&);
		std::vector<uint8_t> above;
		std::vector<uint8_t> tmp;
		uint8_t* tmp2;
		size_t tmpleft;
	};

	png_filterbank& png_filterbank::get(png_decompressor& decomp, uint8_t filterbank, uint8_t type,
		uint8_t depth, size_t width)
	{
		size_t bits = png_filterbank::get_bits(type, depth);
		size_t pitch = (bits >= 8) ? (bits >> 3) : 1;
		size_t elements = (bits >= 8) ? width : (width / (8 / bits));
		if(filterbank == 0) return *new png_filterbank_0(decomp, pitch, elements);
		throw std::runtime_error("Unknown scanline filter bank");
	}

	//=========================================================
	//=================== PNG PIXEL DECODING ==================
	//=========================================================
	class png_pixel_decoder
	{
	public:
		png_pixel_decoder(png_filterbank& _filter, uint8_t _type, uint8_t _depth, size_t _width)
			: width(_width), type(_type), depth(_depth), filter(_filter)
		{
			tmp.resize((png_filterbank::get_bits(type, depth) * width + 7) / 8);
		}
		virtual ~png_pixel_decoder() {}
		virtual bool decode(uint32_t* output, uint8_t* trans) = 0;
		static png_pixel_decoder& get(png_filterbank& _filter, uint8_t type, uint8_t depth, size_t width);
		void adjust_row(size_t _width)
		{
			std::vector<uint8_t> ntmp;
			ntmp.resize((png_filterbank::get_bits(type, depth) * _width + 7) / 8);
			filter.adjust_row(type, depth, _width);
			std::swap(tmp, ntmp);
			width = _width;
		}
	protected:
		size_t width;
		uint8_t type;
		uint8_t depth;
		png_filterbank& filter;
		std::vector<uint8_t> tmp;
		bool fill()
		{
			return filter.row(&tmp[0]);
		}
	private:
		png_pixel_decoder(const png_pixel_decoder&);
		png_pixel_decoder& operator=(const png_pixel_decoder&);
	};

	template<unsigned bits>
	inline uint32_t decode_type_0(const uint8_t* in, unsigned bit, const uint8_t* trans)
	{
		uint16_t v;
		uint32_t m;
		uint32_t s = 0;
		switch(bits) {
		case 1: v = (*in >> (7 - bit)) & 1; m = 0xFFFFFF; break;
		case 2: v = (*in >> (6 - bit)) & 3; m = 0x555555; break;
		case 4: v = (*in >> (4 - bit)) & 15; m = 0x111111; break;
		case 8: v = *in; m = 0xFFFFFF; m = 0x010101; break;
		case 16: v = serialization::u16b(in); m = 0x010101; s = 8; break;
		};
		uint32_t alpha = 0xFF000000U;
		if(trans && v == serialization::u16b(trans))
			alpha = 0;
		return alpha | (m * (v >> s));
	}

	template<unsigned bits>
	inline uint32_t decode_type_2(const uint8_t* in, unsigned bit, const uint8_t* trans)
	{
		uint32_t alpha = 0xFF000000U;
		if(trans) {
			if(bits == 8 && in[0] == trans[1] && in[1] == trans[3] && in[2] == trans[5])
				alpha = 0;
			if(bits == 16 && !memcmp(in, trans, 6))
				alpha = 0;
		}
		if(bits == 8)
			return ((uint32_t)in[0] << 16) |
				((uint32_t)in[1] << 8) |
				((uint32_t)in[2]) |
				alpha;
		else if(bits == 16)
			return ((uint32_t)in[0] << 16) |
				((uint32_t)in[2] << 8) |
				((uint32_t)in[4]) |
				alpha;
	}

	template<unsigned bits>
	inline uint32_t decode_type_3(const uint8_t* in, unsigned bit, const uint8_t* trans)
	{
		switch(bits) {
		case 1: return (*in >> (7 - bit)) & 1;
		case 2: return (*in >> (6 - bit)) & 3;
		case 4: return (*in >> (4 - bit)) & 15;
		case 8: return *in;
		case 16: return serialization::u16b(in);
		};
	}

	template<unsigned bits>
	uint32_t decode_type_4(const uint8_t* in, unsigned bit, const uint8_t* trans)
	{
		if(bits == 8)
			return ((uint32_t)in[0] << 16) |
				((uint32_t)in[0] << 8) |
				((uint32_t)in[0]) |
				((uint32_t)in[1] << 24);
		else if(bits == 16)
			return ((uint32_t)in[0] << 16) |
				((uint32_t)in[0] << 8) |
				((uint32_t)in[0]) |
				((uint32_t)in[2] << 24);
	}

	template<unsigned bits>
	uint32_t decode_type_6(const uint8_t* in, unsigned bit, const uint8_t* trans)
	{
		if(bits == 8)
			return ((uint32_t)in[0] << 16) |
				((uint32_t)in[1] << 8) |
				((uint32_t)in[2]) |
				((uint32_t)in[3] << 24);
		else if(bits == 16)
			return ((uint32_t)in[0] << 16) |
				((uint32_t)in[2] << 8) |
				((uint32_t)in[4]) |
				((uint32_t)in[6] << 24);
	}

	template<size_t bits, uint32_t (*decodefn)(const uint8_t* in, unsigned bit, const uint8_t* trans)>
	class png_pixel_decoder_fn : public png_pixel_decoder
	{
	public:
		png_pixel_decoder_fn(png_filterbank& _filter, uint8_t _type, uint8_t _depth, size_t _width)
			: png_pixel_decoder(_filter, _type, _depth, _width)
		{
		}
		~png_pixel_decoder_fn() {}
		bool decode(uint32_t* output, uint8_t* trans)
		{
			if(!fill())
				return false;
			size_t off = 0;
			unsigned bit = 0;
			for(size_t i = 0; i < width; i++) {
				output[i] = decodefn(&tmp[off], bit, trans);
				bit += bits;
				off += (bit >> 3);
				bit &= 7;
			}
			return true;
		}
	};

	png_pixel_decoder& png_pixel_decoder::get(png_filterbank& filter, uint8_t type, uint8_t depth, size_t width)
	{
		switch(type) {
		case 0:
			switch(depth) {
			case 1: return *new png_pixel_decoder_fn<1, decode_type_0<1>>(filter, type, depth, width);
			case 2: return *new png_pixel_decoder_fn<2, decode_type_0<2>>(filter, type, depth, width);
			case 4: return *new png_pixel_decoder_fn<4, decode_type_0<4>>(filter, type, depth, width);
			case 8: return *new png_pixel_decoder_fn<8, decode_type_0<8>>(filter, type, depth, width);
			case 16: return *new png_pixel_decoder_fn<16, decode_type_0<16>>(filter, type, depth, width);
			default: throw std::runtime_error("Unsupported color depth for type 0");
			};
		case 2:
			switch(depth) {
			case 8: return *new png_pixel_decoder_fn<24, decode_type_2<8>>(filter, type, depth, width);
			case 16: return *new png_pixel_decoder_fn<48, decode_type_2<16>>(filter, type, depth, width);
			default: throw std::runtime_error("Unsupported color depth for type 2");
			};
		case 3:
			switch(depth) {
			case 1: return *new png_pixel_decoder_fn<1, decode_type_3<1>>(filter, type, depth, width);
			case 2: return *new png_pixel_decoder_fn<2, decode_type_3<2>>(filter, type, depth, width);
			case 4: return *new png_pixel_decoder_fn<4, decode_type_3<4>>(filter, type, depth, width);
			case 8: return *new png_pixel_decoder_fn<8, decode_type_3<8>>(filter, type, depth, width);
			case 16: return *new png_pixel_decoder_fn<16, decode_type_3<16>>(filter, type, depth, width);
			default: throw std::runtime_error("Unsupported color depth for type 3");
			};
		case 4:
			switch(depth) {
			case 8: return *new png_pixel_decoder_fn<16, decode_type_4<8>>(filter, type, depth, width);
			case 16: return *new png_pixel_decoder_fn<32, decode_type_4<16>>(filter, type, depth, width);
			default: throw std::runtime_error("Unsupported color depth for type 4");
			};
		case 6:
			switch(depth) {
			case 8: return *new png_pixel_decoder_fn<32, decode_type_6<8>>(filter, type, depth, width);
			case 16: return *new png_pixel_decoder_fn<64, decode_type_6<16>>(filter, type, depth, width);
			default: throw std::runtime_error("Unsupported color depth for type 6");
			};
		default: throw std::runtime_error("Unsupported color type");
		}
	}

	//=========================================================
	//=================== PNG INTERLACING =====================
	//=========================================================
	class png_interlacing
	{
	public:
		struct pass_info
		{
			size_t xoff;
			size_t xmod;
			size_t yoff;
			size_t ymod;
		};
		png_interlacing()
		{
		}
		virtual ~png_interlacing() {}
		virtual size_t passes() = 0;
		virtual pass_info pass(size_t n) = 0;
		static std::pair<size_t, size_t> pass_size(size_t iwidth, size_t iheight, pass_info pinfo)
		{
			size_t width = (iwidth + (pinfo.xmod - pinfo.xoff - 1)) / pinfo.xmod;
			size_t height = (iheight + (pinfo.ymod - pinfo.yoff - 1)) / pinfo.ymod;
			if(!width || !height)
				return std::make_pair(0, 0);
			return std::make_pair(width, height);
		}
		static png_interlacing& get(uint8_t interlace);
	private:
		png_interlacing(const png_interlacing&);
		png_interlacing& operator=(const png_interlacing&);
	};

	class png_interlacing_progressive : public png_interlacing
	{
	public:
		~png_interlacing_progressive() {}
		size_t passes() { return 1; }
		png_interlacing::pass_info pass(size_t n)
		{
			png_interlacing::pass_info p;
			p.xoff = p.yoff = 0;
			p.xmod = p.ymod = 1;
			return p;
		}
	};

	class png_interlacing_adam : public png_interlacing
	{
	public:
		png_interlacing_adam(unsigned _order) : order(_order) {}
		~png_interlacing_adam() {}
		size_t passes() { return 2 * order + 1; }
		png_interlacing::pass_info pass(size_t n)
		{
			size_t one = 1;
			png_interlacing::pass_info p;
			if(n == 0) {
				p.xmod = p.ymod = (one << order);
				p.xoff = p.yoff = 0;
			} else {
				p.xoff = (n % 2) ? (one << (order - (n + 1) / 2)) : 0;
				p.yoff = (n % 2) ? 0 : (one << (order - n / 2));
				p.xmod = one << (order - n / 2);
				p.ymod = one << (order - (n - 1) / 2);
			}
			return p;
		}
	private:
		unsigned order;
	};

	png_interlacing& png_interlacing::get(uint8_t interlace)
	{
		if(interlace == 0) return *new png_interlacing_progressive();
		if(interlace == 1) return *new png_interlacing_adam(3);
		throw std::runtime_error("Unknown interlace type");
	}
}

void decoder::decode_png(std::istream& stream)
{
	png_dechunker dechunk(stream);
	if(!dechunk.next_chunk())
		throw std::runtime_error("PNG file has no chunks");
	ihdr_chunk hdr(dechunk);
	autorelease<png_decompressor> idat_decomp(png_decompressor::get(dechunk, hdr.compression));
	autorelease<png_filterbank> filterbank(png_filterbank::get(*idat_decomp, hdr.filter, hdr.type,
		hdr.depth, hdr.width));
	autorelease<png_pixel_decoder> pixdecoder(png_pixel_decoder::get(*filterbank, hdr.type, hdr.depth,
		hdr.width));
	autorelease<png_interlacing> interlace(png_interlacing::get(hdr.interlace));
	std::vector<uint32_t> ndata;
	std::vector<uint32_t> npalette;
	ndata.resize(hdr.width * hdr.height);
	if(hdr.width == 0 || hdr.height == 0)
		throw std::runtime_error("PNG file has zero width or height");
	if(ndata.size() / hdr.width != hdr.height)
		throw std::bad_alloc();
	if(hdr.type == 3) {
		npalette.resize(1 << hdr.depth);
		for(size_t i = 0; i < npalette.size(); i++)
			npalette[i] = 0xFF000000U;
	}
	if(!dechunk.next_chunk())
		throw std::runtime_error("PNG file has no chunks besides header");
	uint8_t trans[6];
	uint8_t* _trans = NULL;
	for(size_t pass = 0; pass < interlace->passes(); pass++) {
		size_t scanline = 0;
		png_interlacing::pass_info pinfo = interlace->pass(pass);
		auto resolution = png_interlacing::pass_size(hdr.width, hdr.height, pinfo);
		pixdecoder->adjust_row(resolution.first);
		std::vector<uint32_t> scanlineb;
		scanlineb.resize(resolution.first);
		while(true) {
			switch(dechunk.chunk_type()) {
			case 0x49454E44:	//IEND.
				throw std::runtime_error("Unexpected IEND chunk");
			case 0x504C5445:	//PLTE.
				if(hdr.type == 0 || hdr.type == 4)
					throw std::runtime_error("Illegal PLTE in types 0/4");
				if(hdr.type == 2 || hdr.type == 6)
					break;	//Advisory.
				if(dechunk.chunk_size() > 3 * npalette.size())
					throw std::runtime_error("PLTE too large");
				for(size_t i = 0; i < dechunk.chunk_size() / 3; i++) {
					uint8_t buf[3];
					dechunk.chunk_read(buf, 3);
					npalette[i] = (npalette[i] & 0xFF000000U) |
						((uint32_t)buf[0] << 16) |
						((uint32_t)buf[1] << 8) |
						((uint32_t)buf[2]);
				}
				break;
			case 0x74524E53:	//tRNS.
				if(hdr.type == 4 || hdr.type == 6)
					throw std::runtime_error("Illegal tRNS in types 4/6");
				else if(hdr.type == 0) {
					if(dechunk.chunk_size() != 2)
						throw std::runtime_error("Expected 2-byte tRNS for type0");
					dechunk.chunk_read(trans, 2);
				} else if(hdr.type == 2) {
					if(dechunk.chunk_size() != 6)
						throw std::runtime_error("Expected 6-byte tRNS for type2");
					dechunk.chunk_read(trans, 6);
				} else if(hdr.type == 3) {
					if(dechunk.chunk_size() > npalette.size())
						throw std::runtime_error("tRNS too large");
					for(size_t i = 0; i < dechunk.chunk_size(); i++) {
						uint8_t buf[1];
						dechunk.chunk_read(buf, 1);
						npalette[i] = (npalette[i] & 0x00FFFFFFU) |
							((uint32_t)buf[0] << 24);
					}
				}
				_trans = trans;
				break;
			case 0x49444154:	//IDAT.
				if(scanline == resolution.second)
					goto next_pass;
				while(pixdecoder->decode(&scanlineb[0], _trans)) {
					size_t rline = scanline * pinfo.ymod + pinfo.yoff;
					for(size_t i = 0; i < resolution.first; i++) {
						ndata[rline * hdr.width + (i * pinfo.xmod + pinfo.xoff)] =
							scanlineb[i];
					}
					scanline++;
					if(scanline == resolution.second)
						goto next_pass;
				}
				break;
			default:
				if((dechunk.chunk_type() & 0x20000000U) == 0)
					throw std::runtime_error("Unknown critical chunk");
				break;
			}
			dechunk.next_chunk();
		}
next_pass:
		;
	}
	while(dechunk.next_chunk()) {
		switch(dechunk.chunk_type()) {
		case 0x49454E44:	//IEND.
			goto out;
		case 0x504C5445:	//PLTE
			throw std::runtime_error("PLTE not allowed after image data");
		case 0x49444154:	//IDAT.
			break;
		default:
			if((dechunk.chunk_type() & 0x20000000U) == 0)
				throw std::runtime_error("Unknown critical chunk");
		}
	}
out:
	std::swap(data, ndata);
	std::swap(palette, npalette);
	has_palette = (hdr.type == 3);
	width = hdr.width;
	height = hdr.height;
}

decoder::decoder(const std::string& file)
{
	std::istream& s = zip::openrel(file, "");
	try {
		decode_png(s);
		delete &s;
	} catch(...) {
		delete &s;
		throw;
	}
}

decoder::decoder(std::istream& file)
{
	decode_png(file);
}

decoder::decoder()
{
	width = 0;
	height = 0;
	has_palette = false;
}

encoder::encoder()
{
	width = 0;
	height = 0;
	has_palette = false;
	has_alpha = false;
	colorkey = 0xFFFFFFFFU;
}

void encoder::encode(const std::string& file) const
{
	std::ofstream s(file, std::ios::binary);
	if(!s)
		throw std::runtime_error("Can't open file to write PNG image to");
	encode(s);
}

void encoder::encode(std::ostream& file) const
{
	size_t pbits = size_to_bits(palette.size());
	//Write the PNG magic.
	char png_magic[] = {-119, 80, 78, 71, 13, 10, 26, 10};
	file.write(png_magic, sizeof(png_magic));
	//Write the IHDR
	char ihdr[13];
	serialization::u32b(ihdr + 0, width);
	serialization::u32b(ihdr + 4, height);
	ihdr[8] = has_palette ? size_to_bits(palette.size()) : 8;
	ihdr[9] = has_palette ? 3 : (has_alpha ? 6 : 2);
	ihdr[10] = 0; //Deflate,
	ihdr[11] = 0;  //Filter bank 0
	ihdr[12] = 0;  //No interlacing.
	boost::iostreams::stream<png_chunk_output> ihdr_h(file, 0x49484452);
	ihdr_h.write(ihdr, sizeof(ihdr));
	ihdr_h.close();
	//Write the PLTE.
	if(has_palette) {
		std::vector<char> data;
		data.resize(3 * palette.size());
		for(size_t i = 0; i < palette.size(); i++) {
			data[3 * i + 0] = palette[i] >> 16;
			data[3 * i + 1] = palette[i] >> 8;
			data[3 * i + 2] = palette[i] >> 0;
		}
		boost::iostreams::stream<png_chunk_output> plte_h(file, 0x504C5445);
		plte_h.write(&data[0], data.size());
		plte_h.close();
	}
	//Write the tRNS.
	if(has_palette && has_alpha) {
		std::vector<char> data;
		data.resize(palette.size());
		for(size_t i = 0; i < palette.size(); i++)
			data[i] = palette[i] >> 24;
		boost::iostreams::stream<png_chunk_output> trns_h(file, 0x74524E53);
		trns_h.write(&data[0], data.size());
		trns_h.close();
	}
	//Write the IDAT
	boost::iostreams::filtering_ostream idat_h;
	boost::iostreams::zlib_params params;
	params.noheader = false;
	idat_h.push(boost::iostreams::zlib_compressor(params));
	idat_h.push(png_chunk_output(file, 0x49444154));
	std::vector<char> buf;
	buf.resize(1 + 4 * width);
	size_t bufstride = buffer_stride(width, has_palette, has_alpha, palette.size());
	for(size_t i = 0; i < height; i++) {
		buf[0] = 0;	//No filter.
		if(has_palette)
			switch(pbits) {
			case 1: write_row_pal1(&buf[1], &data[width * i], width); break;
			case 2: write_row_pal2(&buf[1], &data[width * i], width); break;
			case 4: write_row_pal4(&buf[1], &data[width * i], width); break;
			case 8: write_row_pal8(&buf[1], &data[width * i], width); break;
			case 16: write_row_pal16(&buf[1], &data[width * i], width); break;
			}
		else if(has_alpha)
			write_row_rgba(&buf[1], &data[width * i], width);
		else
			write_row_rgb(&buf[1], &data[width * i], width);
		idat_h.write(&buf[0], bufstride);
	}
	idat_h.pop();
	idat_h.pop();
	//Write the IEND and finish.
	boost::iostreams::stream<png_chunk_output> iend_h(file, 0x49454E44);
	iend_h.close();
	if(!file)
		throw std::runtime_error("Can't write target PNG file");
}
}
/*
int main(int argc, char** argv)
{
	png::decoder img;
	decode_png(argv[1], img);
	std::cout << "Size: " << img.width << "*" << img.height << std::endl;
	if(img.has_palette)
		std::cout << "Image is paletted, " << img.palette.size() << " colors." << std::endl;
	if(img.has_palette) {
		for(size_t i = 0; i < img.data.size(); i++) {
			if(i > 0 && i % img.width == 0)
				std::cout << std::endl;
			std::cout << hex::to(img.palette[img.data[i]])
				<< "<" << hex::to(img.data[i]) << "> ";
		}
	} else {
		for(size_t i = 0; i < img.data.size(); i++) {
			if(i > 0 && i % img.width == 0)
				std::cout << std::endl;
			std::cout << hex::to(img.data[i]) << " ";
		}
	}
	std::cout << std::endl;
}
//evaluate-lua b,p=gui.bitmap_load_png("/tmp/tbgn2c16.png"); on_paint = function() gui.bitmap_draw(0,0,b,p); end

*/
