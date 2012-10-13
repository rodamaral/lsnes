#ifndef _inthread__hpp__included__
#define _inthread__hpp__included__

#include <list>
#include <cstdint>
#include <string>

void voicethread_task();
void voicethread_task_end();
void voice_frame_number(uint64_t newframe, double rate);

struct playback_stream_info
{
	uint64_t id;
	uint64_t base;
	uint64_t length;
};

bool voicesub_collection_loaded();
std::list<playback_stream_info> voicesub_get_stream_info();
void voicesub_play_stream(uint64_t id);
void voicesub_export_stream(uint64_t id, const std::string& filename, bool opus);
uint64_t voicesub_import_stream(uint64_t ts, const std::string& filename, bool opus);
void voicesub_delete_stream(uint64_t id);
void voicesub_export_superstream(const std::string& filename);
void voicesub_load_collection(const std::string& filename);
void voicesub_unload_collection();
void voicesub_alter_timebase(uint64_t id, uint64_t ts);
uint64_t voicesub_parse_timebase(const std::string& n);
double voicesub_ts_seconds(uint64_t ts);

#endif
