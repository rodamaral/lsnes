#ifndef _library__memorysearch__hpp__included__
#define _library__memorysearch__hpp__included__

#include "memoryspace.hpp"
#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <stdexcept>

/**
 * Context for memory search. Each individual context is independent.
 */
class memory_search
{
public:
/**
 * Creates a new memory search context with all addresses.
 *
 * Parameter space: The memory space.
 */
	memory_search(memory_space& space) throw(std::bad_alloc);

/**
 * Reset the context so all addresses are candidates again.
 */
	void reset() throw(std::bad_alloc);

/**
 * This searches the memory space, leaving those addresses for which condition object returns true.
 *
 * Parameter obj The condition to search for.
 */
	template<class T> void search(const T& obj) throw();

/**
 * DQ a range of addresses (inclusive on both ends!).
 */
	void dq_range(uint64_t first, uint64_t last);

/**
 * Search for all bytes (update values)
 */
	void update() throw();

/**
 * Get number of memory addresses that are still candidates
 */
	uint64_t get_candidate_count() throw();

/**
 * Returns list of all candidates. This function isn't lazy, so be careful when calling with many candidates.
 */
	std::list<uint64_t> get_candidates() throw(std::bad_alloc);

	void byte_value(uint8_t value) throw();
	void byte_difference(uint8_t value) throw();
	void byte_slt() throw();
	void byte_sle() throw();
	void byte_seq() throw();
	void byte_sne() throw();
	void byte_sge() throw();
	void byte_sgt() throw();
	void byte_ult() throw();
	void byte_ule() throw();
	void byte_ueq() throw();
	void byte_une() throw();
	void byte_uge() throw();
	void byte_ugt() throw();
	void byte_seqlt() throw();
	void byte_seqle() throw();
	void byte_seqge() throw();
	void byte_seqgt() throw();

	void word_value(uint16_t value) throw();
	void word_difference(uint16_t value) throw();
	void word_slt() throw();
	void word_sle() throw();
	void word_seq() throw();
	void word_sne() throw();
	void word_sge() throw();
	void word_sgt() throw();
	void word_ult() throw();
	void word_ule() throw();
	void word_ueq() throw();
	void word_une() throw();
	void word_uge() throw();
	void word_ugt() throw();
	void word_seqlt() throw();
	void word_seqle() throw();
	void word_seqge() throw();
	void word_seqgt() throw();

	void dword_value(uint32_t value) throw();
	void dword_difference(uint32_t value) throw();
	void dword_slt() throw();
	void dword_sle() throw();
	void dword_seq() throw();
	void dword_sne() throw();
	void dword_sge() throw();
	void dword_sgt() throw();
	void dword_ult() throw();
	void dword_ule() throw();
	void dword_ueq() throw();
	void dword_une() throw();
	void dword_uge() throw();
	void dword_ugt() throw();
	void dword_seqlt() throw();
	void dword_seqle() throw();
	void dword_seqge() throw();
	void dword_seqgt() throw();

	void qword_value(uint64_t value) throw();
	void qword_difference(uint64_t value) throw();
	void qword_slt() throw();
	void qword_sle() throw();
	void qword_seq() throw();
	void qword_sne() throw();
	void qword_sge() throw();
	void qword_sgt() throw();
	void qword_ult() throw();
	void qword_ule() throw();
	void qword_ueq() throw();
	void qword_une() throw();
	void qword_uge() throw();
	void qword_ugt() throw();
	void qword_seqlt() throw();
	void qword_seqle() throw();
	void qword_seqge() throw();
	void qword_seqgt() throw();

	static bool searchable_region(memory_region* r)
	{
		return (r && !r->readonly && !r->special);
	}
private:
	memory_space& mspace;
	std::vector<uint8_t> previous_content;
	std::vector<uint64_t> still_in;
	uint64_t candidates;
};

#endif
