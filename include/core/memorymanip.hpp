#ifndef _memorymanip__hpp__included__
#define _memorymanip__hpp__included__

#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <stdexcept>

/**
 * \brief Information about region of memory
 *
 * This structure contains information about memory region.
 */
struct memory_region
{
/**
 * \brief Name of region.
 *
 * This is name of region, mainly for debugging and showing to the user.
 */
	std::string region_name;
/**
 * \brief Base address of region.
 */
	uint64_t baseaddr;
/**
 * \brief Size of region in bytes.
 */
	uint64_t size;
/**
 * \brief Last valid address in this region.
 */
	uint64_t lastaddr;
/**
 * \brief True for ROM, false for RAM.
 */
	bool readonly;
/**
 * \brief True for I/O space, false for fixed memory.
 */
	bool iospace;
/**
 * \brief Endianess of the region.
 *
 * If true, region uses host endian.
 * If false, region uses SNES (big) endian.
 */
	bool native_endian;
};

/**
 * \brief Refresh cart memory mappings
 *
 * This function rereads cartridge memory map. Call after loading a new cartridge.
 *
 * \throws std::bad_alloc Not enough memory.
 */
void refresh_cart_mappings() throw(std::bad_alloc);

/**
 * \brief Get listing of all regions
 *
 * This function returns a list of all known regions.
 *
 * \return All regions
 * \throws std::bad_alloc Not enough memory.
 */
std::vector<struct memory_region> get_regions() throw(std::bad_alloc);

/**
 * \brief Read byte from memory
 *
 * This function reads one byte from memory.
 *
 * \param addr The address to read.
 * \return The byte read.
 */
uint8_t memory_read_byte(uint64_t addr) throw();

/**
 * \brief Read word from memory
 *
 * This function reads two bytes from memory.
 *
 * \param addr The address to read.
 * \return The word read.
 */
uint16_t memory_read_word(uint64_t addr) throw();

/**
 * \brief Read dword from memory
 *
 * This function reads four bytes from memory.
 *
 * \param addr The address to read.
 * \return The dword read.
 */
uint32_t memory_read_dword(uint64_t addr) throw();

/**
 * \brief Read qword from memory
 *
 * This function reads eight bytes from memory.
 *
 * \param addr The address to read.
 * \return The qword read.
 */
uint64_t memory_read_qword(uint64_t addr) throw();

/**
 * \brief Write byte to memory
 *
 * This function writes one byte to memory.
 *
 * \param addr The address to write.
 * \param data The value to write.
 * \return true if the write succeeded.
 */
bool memory_write_byte(uint64_t addr, uint8_t data) throw();

/**
 * \brief Write word to memory
 *
 * This function writes two bytes to memory.
 *
 * \param addr The address to write.
 * \param data The value to write.
 * \return true if the write succeeded.
 */
bool memory_write_word(uint64_t addr, uint16_t data) throw();

/**
 * \brief Write dword to memory
 *
 * This function writes four bytes to memory.
 *
 * \param addr The address to write.
 * \param data The value to write.
 * \return true if the write succeeded.
 */
bool memory_write_dword(uint64_t addr, uint32_t data) throw();

/**
 * \brief Write qword to memory
 *
 * This function writes eight bytes to memory.
 *
 * \param addr The address to write.
 * \param data The value to write.
 * \return true if the write succeeded.
 */
bool memory_write_qword(uint64_t addr, uint64_t data) throw();

/**
 * \brief Memory search context
 *
 * Context for memory search. Each individual context is independent.
 */
class memorysearch
{
public:
/**
 * \brief Create new memory search context.
 *
 * Creates a new memory search context with all addresses.
 *
 * \throws std::bad_alloc Not enough memory.
 */
	memorysearch() throw(std::bad_alloc);

/**
 * \brief Reset the context
 *
 * Reset the context so all addresses are candidates again.
 *
 * \throws std::bad_alloc Not enough memory.
 */
	void reset() throw(std::bad_alloc);

/**
 * \brief Search for address satisfying criteria
 *
 * This searches the memory space, leaving those addresses for which condition object returns true.
 *
 * \param obj The condition to search for.
 */
	template<class T>
	void search(const T& obj) throw();

/**
 * \brief Search for byte with specified value
 * \param value The value to search for
 */
	void byte_value(uint8_t value) throw();
	void byte_difference(uint8_t value) throw();
/**
 * \brief Search for bytes that are signed less
 */
	void byte_slt() throw();
/**
 * \brief Search for bytes that are signed less or equal
 */
	void byte_sle() throw();
/**
 * \brief Search for bytes that are signed equal
 */
	void byte_seq() throw();
/**
 * \brief Search for bytes that are signed not equal
 */
	void byte_sne() throw();
/**
 * \brief Search for bytes that are signed greater or equal
 */
	void byte_sge() throw();
/**
 * \brief Search for bytes that are signed greater
 */
	void byte_sgt() throw();
/**
 * \brief Search for bytes that are unsigned less
 */
	void byte_ult() throw();
/**
 * \brief Search for bytes that are unsigned less or equal
 */
	void byte_ule() throw();
/**
 * \brief Search for bytes that are unsigned equal
 */
	void byte_ueq() throw();
/**
 * \brief Search for bytes that are unsigned not equal
 */
	void byte_une() throw();
/**
 * \brief Search for bytes that are unsigned greater or equal
 */
	void byte_uge() throw();
/**
 * \brief Search for bytes that are unsigned greater
 */
	void byte_ugt() throw();
	void byte_seqlt() throw();
	void byte_seqle() throw();
	void byte_seqge() throw();
	void byte_seqgt() throw();

/**
 * \brief Search for word with specified value
 * \param value The value to search for
 */
	void word_value(uint16_t value) throw();
	void word_difference(uint16_t value) throw();
/**
 * \brief Search for words that are signed less
 */
	void word_slt() throw();
/**
 * \brief Search for words that are signed less or equal
 */
	void word_sle() throw();
/**
 * \brief Search for words that are signed equal
 */
	void word_seq() throw();
/**
 * \brief Search for words that are signed not equal
 */
	void word_sne() throw();
/**
 * \brief Search for words that are signed greater or equal
 */
	void word_sge() throw();
/**
 * \brief Search for words that are signed greater
 */
	void word_sgt() throw();
/**
 * \brief Search for words that are unsigned less
 */
	void word_ult() throw();
/**
 * \brief Search for words that are unsigned less or equal
 */
	void word_ule() throw();
/**
 * \brief Search for words that are unsigned equal
 */
	void word_ueq() throw();
/**
 * \brief Search for words that are unsigned not equal
 */
	void word_une() throw();
/**
 * \brief Search for words that are unsigned greater or equal
 */
	void word_uge() throw();
/**
 * \brief Search for words that are unsigned greater
 */
	void word_ugt() throw();
	void word_seqlt() throw();
	void word_seqle() throw();
	void word_seqge() throw();
	void word_seqgt() throw();

/**
 * \brief Search for dword with specified value
 * \param value The value to search for
 */
	void dword_value(uint32_t value) throw();
	void dword_difference(uint32_t value) throw();
/**
 * \brief Search for dwords that are signed less
 */
	void dword_slt() throw();
/**
 * \brief Search for dwords that are signed less or equal
 */
	void dword_sle() throw();
/**
 * \brief Search for dwords that are signed equal
 */
	void dword_seq() throw();
/**
 * \brief Search for dwords that are signed not equal
 */
	void dword_sne() throw();
/**
 * \brief Search for dwords that are signed greater or equal
 */
	void dword_sge() throw();
/**
 * \brief Search for dwords that are signed greater
 */
	void dword_sgt() throw();
/**
 * \brief Search for dwords that are unsigned less
 */
	void dword_ult() throw();
/**
 * \brief Search for dwords that are unsigned less or equal
 */
	void dword_ule() throw();
/**
 * \brief Search for dwords that are unsigned equal
 */
	void dword_ueq() throw();
/**
 * \brief Search for dwords that are unsigned not equal
 */
	void dword_une() throw();
/**
 * \brief Search for dwords that are unsigned greater or equal
 */
	void dword_uge() throw();
/**
 * \brief Search for dwords that are unsigned greater
 */
	void dword_ugt() throw();
	void dword_seqlt() throw();
	void dword_seqle() throw();
	void dword_seqge() throw();
	void dword_seqgt() throw();

/**
 * \brief Search for qword with specified value
 * \param value The value to search for
 */
	void qword_value(uint64_t value) throw();
	void qword_difference(uint64_t value) throw();
/**
 * \brief Search for qwords that are signed less
 */
	void qword_slt() throw();
/**
 * \brief Search for qwords that are signed less or equal
 */
	void qword_sle() throw();
/**
 * \brief Search for qwords that are signed equal
 */
	void qword_seq() throw();
/**
 * \brief Search for qwords that are signed not equal
 */
	void qword_sne() throw();
/**
 * \brief Search for qwords that are signed greater or equal
 */
	void qword_sge() throw();
/**
 * \brief Search for qwords that are signed greater
 */
	void qword_sgt() throw();
/**
 * \brief Search for qwords that are unsigned less
 */
	void qword_ult() throw();
/**
 * \brief Search for qwords that are unsigned less or equal
 */
	void qword_ule() throw();
/**
 * \brief Search for qwords that are unsigned equal
 */
	void qword_ueq() throw();
/**
 * \brief Search for qwords that are unsigned not equal
 */
	void qword_une() throw();
/**
 * \brief Search for qwords that are unsigned greater or equal
 */
	void qword_uge() throw();
/**
 * \brief Search for qwords that are unsigned greater
 */
	void qword_ugt() throw();
	void qword_seqlt() throw();
	void qword_seqle() throw();
	void qword_seqge() throw();
	void qword_seqgt() throw();

/**
 * \brief DQ a range of addresses (inclusive on both ends!).
 */
	void dq_range(uint64_t first, uint64_t last);

/**
 * \brief Search for all bytes (update values)
 */
	void update() throw();

/**
 * \brief Get number of memory addresses that are still candidates
 *
 * This returns the number of memory addresses satisfying constraints so far.
 *
 * \return The number of candidates
 */
	uint64_t get_candidate_count() throw();

/**
 * \brief Get List of all candidate addresses
 *
 * Returns list of all candidates. This function isn't lazy, so be careful when calling with many candidates.
 *
 * \return Candidate address list
 * \throws std::bad_alloc Not enough memory.
 */
	std::list<uint64_t> get_candidates() throw(std::bad_alloc);
private:
	std::vector<uint8_t> previous_content;
	std::vector<uint64_t> still_in;
	uint64_t candidates;
};

#endif
