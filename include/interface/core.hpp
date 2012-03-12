#ifndef _interface__core__hpp__included__
#define _interface__core__hpp__included__

#include <string>
#include <vector>
#include <map>

std::string emucore_get_version();
std::pair<uint32_t, uint32_t> emucore_get_video_rate(bool interlace = false);
std::pair<uint32_t, uint32_t> emucore_get_audio_rate();
void emucore_basic_init();

struct sram_slot_structure
{
	virtual std::string get_name() = 0;
	virtual void copy_to_core(const std::vector<char>& content) = 0;
	virtual void copy_from_core(std::vector<char>& content) = 0;
	virtual size_t get_size() = 0;		//0 if variable size.
};

size_t emucore_sram_slots();
struct sram_slot_structure* emucore_sram_slot(size_t index);
void emucore_refresh_cart();

#endif
