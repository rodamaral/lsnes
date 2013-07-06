#ifndef _advdumper__hpp__included__
#define _advdumper__hpp__included__

#include <string>
#include <set>
#include <stdexcept>

#include "library/framebuffer.hpp"

class adv_dumper
{
public:
/**
 * Detail flags.
 */
	static unsigned target_type_mask;
	static unsigned target_type_file;
	static unsigned target_type_prefix;
	static unsigned target_type_special;
/**
 * Register a dumper.
 *
 * Parameter id: The ID of dumper.
 * Throws std::bad_alloc: Not enough memory.
 */
	adv_dumper(const std::string& id) throw(std::bad_alloc);
/**
 * Unregister a dumper.
 */
	~adv_dumper();
/**
 * Get ID of dumper.
 *
 * Returns: The id.
 */
	const std::string& id() throw();
/**
 * Get set of all dumpers.
 *
 * Returns: The set.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::set<adv_dumper*> get_dumper_set() throw(std::bad_alloc);
/**
 * List all valid submodes.
 *
 * Returns: List of all valid submodes. Empty list means this dumper has no submodes.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::set<std::string> list_submodes() throw(std::bad_alloc) = 0;
/**
 * Get mode details
 *
 * parameter mode: The submode.
 * Returns: Mode details flags
 */
	virtual unsigned mode_details(const std::string& mode) throw() = 0;
/**
 * Get mode extensions. Only called if mode details specifies that output is a single file.
 *
 * parameter mode: The submode.
 * Returns: Mode extension
 */
	virtual std::string mode_extension(const std::string& mode) throw() = 0;
/**
 * Get human-readable name for this dumper.
 *
 * Returns: The name.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::string name() throw(std::bad_alloc) = 0;
/**
 * Get human-readable name for submode.
 *
 * Parameter mode: The submode.
 * Returns: The name.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::string modename(const std::string& mode) throw(std::bad_alloc) = 0;
/**
 * Is this dumper busy dumping?
 *
 * Return: True if busy, false if not.
 */
	virtual bool busy() = 0;
/**
 * Start dump.
 *
 * parameter mode: The mode to dump using.
 * parameter targetname: The target filename or prefix.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Can't start dump.
 */
	virtual void start(const std::string& mode, const std::string& targetname) throw(std::bad_alloc,
		std::runtime_error) = 0;
/**
 * End current dump.
 */
	virtual void end() throw() = 0;
private:
	std::string d_id;
};

/**
 * Render Lua HUD on video.
 *
 * Parameter target: The target screen to render on.
 * Parameter source: The source screen to read.
 * Parameter hscl: The horizontal scale factor.
 * Parameter vscl: The vertical scale factor.
 * Parameter roffset: Red offset.
 * Parameter goffset: Green offset.
 * Parameter boffset: Blue offset.
 * Parameter lgap: Left gap.
 * Parameter tgap: Top gap.
 * Parameter rgap: Right gap
 * Parameter bgap: Bottom gap.
 * Parameter fn: Function to call between running lua hooks and actually rendering.
 */
template<bool X> void render_video_hud(struct framebuffer<X>& target, struct framebuffer_raw& source, uint32_t hscl,
	uint32_t vscl, uint32_t roffset, uint32_t goffset, uint32_t boffset, uint32_t lgap, uint32_t tgap,
	uint32_t rgap, uint32_t bgap, void(*fn)());

#endif
