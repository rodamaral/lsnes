#ifndef _interface__core__hpp__included__
#define _interface__core__hpp__included__

#include <string>
#include <map>

std::string emucore_get_version();
std::pair<uint32_t, uint32_t> emucore_get_video_rate(bool interlace = false);
std::pair<uint32_t, uint32_t> emucore_get_audio_rate();
void emucore_basic_init();

#endif
