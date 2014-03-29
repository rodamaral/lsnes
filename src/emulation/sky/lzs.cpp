#include "lzs.hpp"
#include <cassert>
#include <climits>
#include <vector>

namespace sky
{
	data_error::data_error(const char* errmsg) : std::runtime_error(errmsg) {}

	input_stream::~input_stream()
	{
	}

	inline void reload(input_stream& in, unsigned char& byte, unsigned char& bits)
	{
		assert(bits <= 8);
		if(bits == 0) {
			int r = in.read("Unexpected end of compressed data");
			byte = r;
			bits = 8;
		}
		assert(bits <= 8);
	}

	inline bool read1(input_stream& in, unsigned char& byte, unsigned char& bits)
	{
		assert(bits <= 8);
		reload(in, byte, bits);
		bool ret = ((byte & 0x80) != 0);
		byte <<= 1;
		bits--;
		assert(bits <= 8);
		return ret;
	}

	inline size_t read_n(input_stream& in, unsigned char& byte, unsigned char& bits, unsigned count)
	{
		assert(bits <= 8);
		if(count == 0)
			return 0;
		if(count <= bits) {
			//Satisfiable immediately.
			size_t ret = byte >> (8 - count);
			bits -= count;
			byte <<= count;
			return ret;
		}
		size_t readc = 0;
		size_t ret = 0;
		//Read the first incomplete byte if any.
		if(bits > 0) {
			ret = byte >> (8 - bits);
			readc = bits;
			bits = 0;
		}
		//Read complete bytes.
		while(readc + 8 <= count) {
			int r = in.read("Unexpected end of compressed data");
			ret = (ret << 8) | (r & 0xFF);
			readc += 8;
		}
		size_t tailbits = count - readc;
		if(tailbits > 0) {
			//Read the tail bits.
			reload(in, byte, bits);
			ret = (ret << tailbits) | (byte >> (8 - tailbits));
			byte <<= tailbits;
			bits -= tailbits;
		}
		return ret;
	}

	void lzs_decompress(input_stream& in, output_stream& out, size_t size)
	{
		unsigned char cbits = in.read("Incomplete compression header");
		unsigned char sbits = in.read("Incomplete compression header");
		unsigned char lbits = in.read("Incomplete compression header");
		size_t maxbits = sizeof(size_t) * CHAR_BIT;
		if((sbits >= maxbits || lbits >= maxbits) || (sbits == maxbits - 1 && lbits == maxbits - 1))
			throw data_error("Can't handle the backward offset space");
		if(cbits >= maxbits)
			throw data_error("Can't handle the copy length space");

		size_t maxoffset = 2 + (size_t(1) << sbits) + (size_t(1) << lbits);
		size_t shortbias = 2;
		size_t longbias = 2 + (size_t(1) << sbits);
		maxoffset = (size < maxoffset) ? size : maxoffset;
		std::vector<unsigned char> tmp;
		tmp.resize(maxoffset);

		uint8_t pending_byte = 0;
		uint8_t pending_bits = 0;
		size_t output = 0;
		size_t copy_remaining = 0;
		size_t copy_backward = 0;
		size_t cyclic_pointer = 0;
		while(output < size) {
			assert(pending_bits <= 8);
			if(copy_remaining) {
				size_t cindex;
				if(copy_backward <= cyclic_pointer)
					cindex = cyclic_pointer - copy_backward;
				else
					cindex = maxoffset + cyclic_pointer - copy_backward;
				copy_remaining--;
				out.put(tmp[cyclic_pointer++] = tmp[cindex]);
				if(cyclic_pointer == maxoffset)
					cyclic_pointer = 0;
				output++;
				continue;
			}
			if(read1(in, pending_byte, pending_bits)) {
				if(read1(in, pending_byte, pending_bits)) {
					//Literial insert.
					out.put(tmp[cyclic_pointer++] = static_cast<unsigned char>(read_n(in,
						pending_byte, pending_bits, 8)));
					if(cyclic_pointer == maxoffset)
						cyclic_pointer = 0;
					output++;
				} else {
					//Long copy.
					copy_backward = longbias + read_n(in, pending_byte, pending_bits, lbits);
					copy_remaining = 2 + read_n(in, pending_byte, pending_bits, cbits);
					if(copy_backward > output)
					throw data_error("Backward copy offset too large");
				}
			} else {
				//Short copy.
				copy_backward = shortbias + read_n(in, pending_byte, pending_bits, sbits);
				copy_remaining = 2 + read_n(in, pending_byte, pending_bits, cbits);
				if(copy_backward > output)
					throw data_error("Backward copy offset too large");
			}
		}
	}
}
