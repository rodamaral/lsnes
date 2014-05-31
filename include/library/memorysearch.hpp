#ifndef _library__memorysearch__hpp__included__
#define _library__memorysearch__hpp__included__

#include "memoryspace.hpp"

#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <stdexcept>

class memory_space;

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
/**
 * Is specified address a candidate?
 */
	bool is_candidate(uint64_t addr) throw();
/**
 * Next candidate in VMA.
 */
	uint64_t cycle_candidate_vma(uint64_t addr, bool next) throw();

	template<typename T> void s_value(T value) throw();
	template<typename T> void s_difference(T value) throw();
	template<typename T> void s_lt() throw();
	template<typename T> void s_le() throw();
	template<typename T> void s_eq() throw();
	template<typename T> void s_ne() throw();
	template<typename T> void s_ge() throw();
	template<typename T> void s_gt() throw();
	template<typename T> void s_seqlt() throw();
	template<typename T> void s_seqle() throw();
	template<typename T> void s_seqge() throw();
	template<typename T> void s_seqgt() throw();
	template<typename T> T v_read(uint64_t addr) throw();
	template<typename T> T v_readold(uint64_t addr) throw();
	template<typename T> void v_write(uint64_t addr, T val) throw();

	static bool searchable_region(memory_region* r)
	{
		return (r && !r->readonly && !r->special);
	}
/**
 * Savestate type.
 */
	enum savestate_type
	{
		ST_PREVMEM,
		ST_SET,
		ST_ALL
	};
/**
 * Save state.
 */
	void savestate(std::vector<char>& buffer, enum savestate_type type) const;
/**
 * Load state.
 */
	void loadstate(const std::vector<char>& buffer);
private:
	memory_space& mspace;
	std::vector<uint8_t> previous_content;
	std::vector<uint64_t> still_in;
	uint64_t candidates;
};

#endif
