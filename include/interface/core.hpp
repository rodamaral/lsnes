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
	virtual ~sram_slot_structure();
	virtual std::string get_name() = 0;
	virtual void copy_to_core(const std::vector<char>& content) = 0;
	virtual void copy_from_core(std::vector<char>& content) = 0;
	virtual size_t get_size() = 0;		//0 if variable size.
};

struct vma_structure
{
	enum endian
	{
		E_LITTLE = -1,
		E_HOST = 0,
		E_BIG = 1
	};

	vma_structure(const std::string& _name, uint64_t _base, uint64_t _size, endian _rendian, bool _readonly);
	virtual ~vma_structure();
	std::string get_name() { return name; }
	uint64_t get_base() { return base; }
	uint64_t get_size() { return size; }
	bool is_readonly() { return readonly; }
	endian get_endian() { return rendian; }
	virtual void copy_from_core(uint64_t start, char* buffer, uint64_t size) = 0;
	virtual void copy_to_core(uint64_t start, const char* buffer, uint64_t size) = 0;
protected:
	std::string name;
	uint64_t base;
	uint64_t size;
	bool readonly;
	endian rendian;
};

size_t emucore_sram_slots();
struct sram_slot_structure* emucore_sram_slot(size_t index);
size_t emucore_vma_slots();
struct vma_structure* emucore_vma_slot(size_t index);
void emucore_refresh_cart();
std::vector<char> emucore_serialize();
void emucore_unserialize(const std::vector<char>& data);

#endif
