#pragma once

void write_val8(char* out, const uint8_t& var)
{
	*out = var;
}

void write_val32(char* out, const unsigned& var)
{
	out[0] = var >> 24;
	out[1] = var >> 16;
	out[2] = var >> 8;
	out[3] = var;
}

void write_val16a(char* out, const uint16_t* var, size_t elems)
{
	for(size_t i = 0; i < elems; i++) {
		out[2 * i + 0] = var[i] >> 8;
		out[2 * i + 1] = var[i];
	}
}

void read_val32(const char* in, unsigned& var)
{
	var = 0;
	var |= ((unsigned)(unsigned char)in[0] << 24);
	var |= ((unsigned)(unsigned char)in[1] << 16);
	var |= ((unsigned)(unsigned char)in[2] << 8);
	var |= ((unsigned)(unsigned char)in[3]);
}

void read_val8(const char* in, uint8_t& var)
{
	var = in[0];
}

void read_val16a(const char* in, uint16_t* var, size_t elems)
{
	for(size_t i = 0; i < elems; i++) {
		var[i] = 0;
		var[i] |= ((unsigned)(unsigned char)in[0] << 8);
		var[i] |= ((unsigned)(unsigned char)in[1]);
	}
}

