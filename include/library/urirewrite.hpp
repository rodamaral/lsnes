#ifndef _library__urirewrite__hpp__included__
#define _library__urirewrite__hpp__included__

#include <set>
#include <string>
#include <map>
#include "threads.hpp"

namespace urirewrite
{
class rewriter
{
public:
/**
 * Get set of known schemes.
 *
 * Returns: The known schemes.
 */
	std::set<std::string> get_schemes();
/**
 * Delete rewrite pattern.
 *
 * Parameter scheme: The scheme to delete.
 */
	void delete_rewrite(const std::string& scheme);
/**
 * Set rewrite pattern
 *
 * Parameter scheme: The scheme to rewrite.
 * Parameter pattern: The target pattern to rewrite to.
 */
	void set_rewrite(const std::string& scheme, const std::string& pattern);
/**
 * Get rewrite pattern
 *
 * Parameter scheme: The scheme to rewrite.
 * Returns: The current rewrite pattern.
 * Throws std::runtime_error: No pattern for scheme.
 */
	std::string get_rewrite(const std::string& scheme);
/**
 * Rewrite URI.
 *
 * Parameter uri: The URI to rewrite.
 * Returns: The rewritten URI.
 */
	std::string operator()(const std::string& uri);
/**
 * Save URI rewrite patterns to file.
 *
 * Parameter filename: The name of the file.
 */
	void save(const std::string& filename);
/**
 * Load URI rewrite pattern from file.
 *
 * Parameter filename: The name of the file.
 */
	void load(const std::string& filename);
private:
	threads::lock mlock;
	std::map<std::string, std::string> rewrites;
};
}

#endif
