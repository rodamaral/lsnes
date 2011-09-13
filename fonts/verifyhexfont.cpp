#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>

extern uint32_t fontdata_size;
extern uint32_t fontdata[];

uint32_t mdir_use = 0;
uint32_t mdir_nuse = 0;
uint32_t sdir_gkey = 0;
uint32_t sdir_bkey = 0;
uint32_t ftype0 = 0;
uint32_t ftype1 = 0;
std::set<uint32_t> seen;

//This is Jenkin's MIX function.
uint32_t keyhash(uint32_t key, uint32_t item, uint32_t mod)
{
	uint32_t a = key;
	uint32_t b = 0;
	uint32_t c = item;
	a=a-b;	a=a-c;	a=a^(c >> 13);
	b=b-c;	b=b-a;	b=b^(a << 8); 
	c=c-a;	c=c-b;	c=c^(b >> 13);
	a=a-b;	a=a-c;	a=a^(c >> 12);
	b=b-c;	b=b-a;	b=b^(a << 16);
	c=c-a;	c=c-b;	c=c^(b >> 5);
	a=a-b;	a=a-c;	a=a^(c >> 3);
	b=b-c;	b=b-a;	b=b^(a << 10);
	c=c-a;	c=c-b;	c=c^(b >> 15);
	return c % mod;
}

void verify_font(uint32_t cp, uint32_t offset)
{
	if(offset >= fontdata_size) {
		std::cerr << "Font verify error: Bad offset for codepoint " << cp << "." << std::endl;
	}
	if(seen.count(offset))
		return;
	if(fontdata[offset] > 1) {
		std::cerr << "Font verify error: Bad glyph type for codepoint " << cp << "." << std::endl;
		exit(1);
	}
	if(fontdata[offset] == 0)
		ftype0++;
	else if(fontdata[offset] == 1)
		ftype1++;
	seen.insert(offset);
}

void verify_subdirectory(uint32_t offset, uint32_t mseed, uint32_t msize, uint32_t snum)
{
	if(offset + 2 > fontdata_size) {
		std::cerr << "Font verify error: Subdirectory offset out of range: " << offset << "." << std::endl;
		exit(1);
	}
	uint32_t sseed = fontdata[offset++];
	uint32_t ssize = fontdata[offset++];
	if(offset + 2 * ssize > fontdata_size) {
		std::cerr << "Font verify error: Subdirectory offset out of range: " << offset - 2 << "." << std::endl;
		exit(1);
	}
	if(ssize)
		mdir_use++;
	else
		mdir_nuse++;
	for(uint32_t i = 0; i < ssize; i++) {
		uint32_t cp = fontdata[offset++];
		uint32_t foffset = fontdata[offset++];
		if(keyhash(mseed, cp, msize) == snum && keyhash(sseed, cp, ssize) == i) {
			verify_font(cp, foffset);
			sdir_gkey++;
		} else if(foffset != 0) {
			std::cerr << "Font verify error: Bad codepoint with nonzero offset: " << i << " of "
				<< snum << "." << std::endl;
			exit(1);
		} else
			sdir_bkey++;
	}
}

void verify_main_directory(uint32_t offset)
{
	if(offset + 2 > fontdata_size) {
		std::cerr << "Font verify error: Main directory offset out of range: " << offset << "." << std::endl;
		exit(1);
	}
	uint32_t mseed = fontdata[offset++];
	uint32_t msize = fontdata[offset++];
	if(offset + msize > fontdata_size) {
		std::cerr << "Font verify error: Main directory offset out of range: " << offset - 2 << "."
			<< std::endl;
		exit(1);
	}
	for(uint32_t i = 0; i < msize; i++)
		verify_subdirectory(fontdata[offset++], mseed, msize, i);
}

int main()
{
	if(fontdata_size == 0) {
		std::cerr << "Font verify error: Empty fontdata." << std::endl;
		exit(1);
	}
	verify_main_directory(fontdata[0]);
	std::cerr << "Font verify successful!" << std::endl;
	std::cerr << "Main directory entries: " << (mdir_use + mdir_nuse) << " (" << mdir_use << " used + "
		<< mdir_nuse << " not_used)." << std::endl;
	std::cerr << "Subdirectory entries:  " << (sdir_gkey + sdir_bkey) << " (" << sdir_gkey << " good + "
		<< sdir_bkey << " bad)." << std::endl;
	std::cerr << "Distinct glyphs:       " << (ftype0 + ftype1) << " (" << ftype0 << " narrow + "
		<< ftype1 << " wide)." << std::endl;
	return 0;
}