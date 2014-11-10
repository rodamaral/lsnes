#include "core/moviefile-binary.hpp"
#include "core/moviefile.hpp"
#include "library/serialization.hpp"

#include <fcntl.h>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
#include <windows.h>
//FUCK YOU. SERIOUSLY.
#define EXTRA_OPENFLAGS O_BINARY
#else
#define EXTRA_OPENFLAGS 0
#endif

namespace
{
	void emerg_write_bytes(int handle, const uint8_t* d, size_t dsize)
	{
		while(dsize > 0) {
			ssize_t r = write(handle, d, dsize);
			if(r > 0) {
				d += r;
				dsize -= r;
			}
		}
	}
	void emerg_write_number(int handle, uint64_t num)
	{
		uint8_t data[10];
		size_t len = 0;
		do {
			bool cont = (num > 127);
			data[len++] = (cont ? 0x80 : 0x00) | (num & 0x7F);
			num >>= 7;
		} while(num);
		emerg_write_bytes(handle, data, len);
	}
	size_t number_size(uint64_t num)
	{
		unsigned len = 0;
		do {
			num >>= 7;
			len++;
		} while(num);
		return len;
	}
	void emerg_write_number32(int handle, uint32_t num)
	{
		char buf[4];
		serialization::u32b(buf, num);
		emerg_write_bytes(handle, (const uint8_t*)buf, 4);
	}
	void emerg_write_member(int handle, uint32_t tag, uint64_t size)
	{
		emerg_write_number32(handle, 0xaddb2d86);
		emerg_write_number32(handle, tag);
		emerg_write_number(handle, size);
	}
	void emerg_write_blob_implicit(int handle, const std::vector<char>& v)
	{
		emerg_write_bytes(handle, (const uint8_t*)&v[0], v.size());
	}
	void emerg_write_byte(int handle, uint8_t byte)
	{
		emerg_write_bytes(handle, &byte, 1);
	}
	size_t string_size(const std::string& str)
	{
		return number_size(str.length()) + str.length();
	}
	void emerg_write_string_implicit(int handle, const std::string& str)
	{
		for(size_t i = 0; i < str.length(); i++)
			emerg_write_byte(handle, str[i]);
	}
	void emerg_write_string(int handle, const std::string& str)
	{
		emerg_write_number(handle, str.length());
		emerg_write_string_implicit(handle, str);
	}
	void emerg_write_movie(int handle, const portctrl::frame_vector& v, uint32_t tag)
	{
		uint64_t stride = v.get_stride();
		uint64_t pageframes = v.get_frames_per_page();
		uint64_t vsize = v.size();
		emerg_write_member(handle, tag, vsize * stride);
		size_t pagenum = 0;
		while(vsize > 0) {
			uint64_t count = (vsize > pageframes) ? pageframes : vsize;
			size_t bytes = count * stride;
			const unsigned char* content = v.get_page_buffer(pagenum++);
			emerg_write_bytes(handle, content, bytes);
			vsize -= count;
		}
	}
	uint64_t append_number(char* ptr, uint64_t n)
	{
		unsigned digits = 0;
		uint64_t n2 = n;
		do {
			digits++;
			n2 /= 10;
		} while(n2);
		for(unsigned i = digits; i > 0; i--) {
			ptr[i - 1] = (n % 10) + '0';
			n /= 10;
		}
		ptr[digits] = 0;
		return digits;
	}
	template<typename T>
	uint64_t map_index(const std::map<std::string, T>& b, const std::string& n)
	{
		uint64_t idx = 0;
		for(auto& i : b) {
			if(i.first == n)
				return idx;
			idx++;
		}
		return 0xFFFFFFFFFFFFFFFFULL;
	}
}

void emerg_save_movie(const moviefile& mv, rrdata_set& rrd)
{
	//Whee, assume state of the emulator is totally busted.
	if(!mv.gametype)
		return;  //No valid movie. Trying to save would segfault.
	char header[] = {'l', 's', 'm', 'v', '\x1a'};
	int fd;
	char filename_buf[512];
	int number = 1;
name_again:
	filename_buf[0] = 0;
	strcpy(filename_buf + strlen(filename_buf), "crashsave-");
	append_number(filename_buf + strlen(filename_buf), time(NULL));
	strcpy(filename_buf + strlen(filename_buf), "-");
	append_number(filename_buf + strlen(filename_buf), number++);
	strcpy(filename_buf + strlen(filename_buf), ".lsmv");
	fd = open(filename_buf, O_WRONLY | O_CREAT | O_EXCL | EXTRA_OPENFLAGS, 0666);
	if(fd < 0 && errno == EEXIST) goto name_again;
	if(fd < 0) return;  //Can't open.
	//Headers.
	emerg_write_bytes(fd, (const uint8_t*)header, sizeof(header));
	emerg_write_string(fd, mv.gametype->get_name());
	for(auto& i : mv.settings) {
		emerg_write_byte(fd, 1);
		emerg_write_string(fd, i.first);
		emerg_write_string(fd, i.second);
	}
	emerg_write_byte(fd, 0);
	//The actual movie.
	for(auto& i : mv.branches) {
		emerg_write_member(fd, TAG_BRANCH_NAME, i.first.length());
		emerg_write_string_implicit(fd, i.first);
		emerg_write_movie(fd, i.second, (&i.second == mv.input) ? TAG_MOVIE : TAG_BRANCH);
	}
	//Movie starting time.
	emerg_write_member(fd, TAG_MOVIE_TIME, number_size(mv.movie_rtc_second) +
		number_size(mv.movie_rtc_subsecond));
	emerg_write_number(fd, mv.movie_rtc_second);
	emerg_write_number(fd, mv.movie_rtc_subsecond);
	//Project id.
	emerg_write_member(fd, TAG_PROJECT_ID, mv.projectid.length());
	emerg_write_string_implicit(fd, mv.projectid);
	//starting SRAM.
	for(auto& i : mv.movie_sram) {
		emerg_write_member(fd, TAG_MOVIE_SRAM, string_size(i.first) + i.second.size());
		emerg_write_string(fd, i.first);
		emerg_write_blob_implicit(fd, i.second);
	}
	//Anchor save.
	emerg_write_member(fd, TAG_ANCHOR_SAVE, mv.anchor_savestate.size());
	emerg_write_blob_implicit(fd, mv.anchor_savestate);
	//RRDATA.
	emerg_write_member(fd, TAG_RRDATA, rrd.size_emerg());
	rrdata_set::esave_state estate;
	while(true) {
		char buf[4096];
		size_t w = rrd.write_emerg(estate, buf, sizeof(buf));
		if(!w) break;
		emerg_write_bytes(fd, (const uint8_t*)buf, w);
	}
	//Core version.
	emerg_write_member(fd, TAG_CORE_VERSION, mv.coreversion.length());
	emerg_write_string_implicit(fd, mv.coreversion);
	//ROM slots data.
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		if(mv.romimg_sha256[i].length()) {
			emerg_write_member(fd, TAG_ROMHASH, mv.romimg_sha256[i].length() + 1);
			emerg_write_byte(fd, 2 * i);
			emerg_write_string_implicit(fd, mv.romimg_sha256[i]);
		}
		if(mv.romxml_sha256[i].length()) {
			emerg_write_member(fd, TAG_ROMHASH, mv.romxml_sha256[i].length() + 1);
			emerg_write_byte(fd, 2 * i + 1);
			emerg_write_string_implicit(fd, mv.romxml_sha256[i]);
		}
		if(mv.namehint[i].length()) {
			emerg_write_member(fd, TAG_ROMHINT, mv.namehint[i].length() + 1);
			emerg_write_byte(fd, i);
			emerg_write_string_implicit(fd, mv.namehint[i]);
		}
	}
	//Game name.
	emerg_write_member(fd, TAG_GAMENAME, mv.gamename.size());
	emerg_write_string_implicit(fd, mv.gamename);
	//Subtitles.
	for(auto& i : mv.subtitles) {
		emerg_write_member(fd, TAG_SUBTITLE, number_size(i.first.get_frame()) +
			number_size(i.first.get_length()) + i.second.length());
		emerg_write_number(fd, i.first.get_frame());
		emerg_write_number(fd, i.first.get_length());
		emerg_write_string_implicit(fd, i.second);
	}
	//Authors.
	for(auto& i : mv.authors) {
		emerg_write_member(fd, TAG_AUTHOR, string_size(i.first) + i.second.size());
		emerg_write_string(fd, i.first);
		emerg_write_string_implicit(fd, i.second);

	}
	//RAM contents.
	for(auto& i : mv.ramcontent) {
		emerg_write_member(fd, TAG_RAMCONTENT, string_size(i.first) + i.second.size());
		emerg_write_string(fd, i.first);
		emerg_write_blob_implicit(fd, i.second);
	}
	close(fd);
}
