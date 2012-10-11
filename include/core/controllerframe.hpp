#ifndef _controllerframe__hpp__included__
#define _controllerframe__hpp__included__

#include <cstring>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <list>
#include "library/controller-data.hpp"

/**
 * For now, reserve 23 bytes, for:
 *
 * - 5 bytes for system.
 * - 9 bytes for ports 1&2 (justifiers).
 */
#define MAXIMUM_CONTROLLER_FRAME_SIZE 23

/**
 * Maximum amount of data controller_frame::display() can write.
 */
#define MAX_DISPLAY_LENGTH 128
/**
 * Maximum amount of data controller_frame::serialize() can write.
 */
#define MAX_SERIALIZED_SIZE 256
/**
 * Maximum number of ports.
 */
#define MAX_PORTS 2
/**
 * Maximum number of controllers per one port.
 */
#define MAX_CONTROLLERS_PER_PORT 4
/**
 * Maximum numbers of controls per one controller.
 */
#define MAX_CONTROLS_PER_CONTROLLER 12
/**
 * Number of button controls.
 */
#define MAX_BUTTONS MAX_PORTS * MAX_CONTROLLERS_PER_PORT * MAX_CONTROLS_PER_CONTROLLER
/**
 * Size of controller page.
 */
#define CONTROLLER_PAGE_SIZE 65500
/**
 * Analog indices.
 */
#define MAX_ANALOG 3

/**
 * Poll counter vector.
 */
class pollcounter_vector
{
public:
/**
 * Create new pollcounter vector filled with all zeroes and all DRDY bits clear.
 */
	pollcounter_vector() throw();
/**
 * Zero all poll counters and clear all DRDY bits. System flag is cleared.
 */
	void clear() throw();
/**
 * Set all DRDY bits.
 */
	void set_all_DRDY() throw();
/**
 * Clear specified DRDY bit.
 *
 * Parameter pcid: The physical controller id.
 * Parameter ctrl: The control id.
 */
	void clear_DRDY(unsigned pcid, unsigned ctrl) throw();
/**
 * Get state of DRDY bit.
 *
 * Parameter pcid: The physical controller id.
 * Parameter ctrl: The control id.
 * Returns: The DRDY state.
 */
	bool get_DRDY(unsigned pcid, unsigned ctrl) throw();
/**
 * Get state of DRDY bit.
 *
 * Parameter idx: The control index.
 * Returns: The DRDY state.
 */
	bool get_DRDY(unsigned idx) throw()
	{
		return get_DRDY(idx / MAX_CONTROLS_PER_CONTROLLER, idx % MAX_CONTROLS_PER_CONTROLLER);
	}
/**
 * Is any poll count nonzero or is system flag set?
 *
 * Returns: True if at least one poll count is nonzero or if system flag is set. False otherwise.
 */
	bool has_polled() throw();
/**
 * Read the actual poll count on specified control.
 *
 * Parameter pcid: The physical controller id.
 * Parameter ctrl: The control id.
 * Return: The poll count.
 */
	uint32_t get_polls(unsigned pcid, unsigned ctrl) throw();
/**
 * Read the actual poll count on specified control.
 *
 * Parameter idx: The control index.
 * Return: The poll count.
 */
	uint32_t get_polls(unsigned idx) throw()
	{
		return get_polls(idx / MAX_CONTROLS_PER_CONTROLLER, idx % MAX_CONTROLS_PER_CONTROLLER);
	}
/**
 * Increment poll count on specified control.
 *
 * Parameter pcid: The physical controller id.
 * Parameter ctrl: The control id.
 * Return: The poll count pre-increment.
 */
	uint32_t increment_polls(unsigned pcid, unsigned ctrl) throw();
/**
 * Set the system flag.
 */
	void set_system() throw();
/**
 * Get the system flag.
 *
 * Returns: The state of system flag.
 */
	bool get_system() throw();
/**
 * Get highest poll counter value.
 *
 * - System flag counts as 1 poll.
 *
 * Returns: The maximum poll count (at least 1 if system flag is set).
 */
	uint32_t max_polls() throw();
/**
 * Save state to memory block.
 *
 * Parameter mem: The memory block to save to.
 * Throws std::bad_alloc: Not enough memory.
 */
	void save_state(std::vector<uint32_t>& mem) throw(std::bad_alloc);
/**
 * Load state from memory block.
 *
 * Parameter mem: The block from restore from.
 */
	void load_state(const std::vector<uint32_t>& mem) throw();
/**
 * Check if state can be loaded without errors.
 *
 * Returns: True if load is possible, false otherwise.
 */
	bool check(const std::vector<uint32_t>& mem) throw();
private:
	uint32_t ctrs[MAX_BUTTONS];
	bool system_flag;
};

/**
 * Single (sub)frame of controls.
 */
class controller_frame
{
public:
/**
 * Default constructor. Invalid port types, dedicated memory.
 */
	controller_frame() throw();
/**
 * Create subframe of controls with specified controller types and dedicated memory.
 *
 * Parameter p1: Type of port1.
 * Parameter p2: Type of port2.
 *
 * Throws std::runtime_error: Invalid port type.
 */
	controller_frame(port_type& p1, port_type& p2) throw(std::runtime_error);
/**
 * Create subframe of controls with specified controller types and specified memory.
 *
 * Parameter memory: The backing memory.
 * Parameter p1: Type of port1.
 * Parameter p2: Type of port2.
 *
 * Throws std::runtime_error: Invalid port type or NULL memory.
 */
	controller_frame(unsigned char* memory, port_type& p1, port_type& p2)
		throw(std::runtime_error);
/**
 * Copy construct a frame. The memory will be dedicated.
 *
 * Parameter obj: The object to copy.
 */
	controller_frame(const controller_frame& obj) throw();
/**
 * Assign a frame. The types must either match or memory must be dedicated.
 *
 * Parameter obj: The object to copy.
 * Returns: Reference to this.
 * Throws std::runtime_error: The types don't match and memory is not dedicated.
 */
	controller_frame& operator=(const controller_frame& obj) throw(std::runtime_error);
/**
 * Get type of port.
 *
 * Parameter port: Number of port.
 * Returns: The type of port.
 */
	port_type& get_port_type(unsigned port) throw()
	{
		//FIXME: Return proper types.
		return (port < MAX_PORTS) ? *types[port] : get_dummy_port_type();
	}
/**
 * Get blank dedicated frame of same port types.
 *
 * Return blank frame.
 */
	controller_frame blank_frame() throw()
	{
		return controller_frame(*types[0], *types[1]);
	}
/**
 * Set type of port. Input for that port is zeroized.
 *
 * Parameter port: Number of port.
 * Parameter type: The new type.
 * Throws std::runtime_error: Bad port type or non-dedicated memory.
 */
	void set_port_type(unsigned port, port_type& ptype) throw(std::runtime_error);
/**
 * Check that types match.
 *
 * Parameter obj: Another object.
 * Returns: True if types match, false otherwise.
 */
	bool types_match(const controller_frame& obj) const throw()
	{
		for(size_t i = 0; i < MAX_PORTS; i++)
			if(types[i] != obj.types[i])
				return false;
		return true;
	}
/**
 * Perform XOR between controller frames.
 *
 * Parameter another: The another object.
 * Returns: The XOR result (dedicated memory).
 * Throws std::runtime_error: Type mismatch.
 */
	controller_frame operator^(const controller_frame& another) throw(std::runtime_error)
	{
		controller_frame x(*this);
		for(size_t i = 0; i < MAX_PORTS; i++)
			if(types[i] != another.types[i])
				throw std::runtime_error("controller_frame::operator^: Type mismatch");
		for(size_t i = 0; i < totalsize; i++)
			x.backing[i] ^= another.backing[i];
		return x;
	}
/**
 * Set the sync flag.
 *
 * Parameter x: The value to set the sync flag to.
 */
	void sync(bool x) throw()
	{
		if(x)
			backing[0] |= 1;
		else
			backing[0] &= ~1;
	}
/**
 * Get the sync flag.
 *
 * Return value: Value of sync flag.
 */
	bool sync() throw()
	{
		return ((backing[0] & 1) != 0);
	}
/**
 * Quick get sync flag for buffer.
 */
	static bool sync(const unsigned char* mem) throw()
	{
		return ((mem[0] & 1) != 0);
	}
/**
 * Set the reset flag.
 *
 * Parameter x: The value to set the reset flag to.
 */
	void reset(bool x) throw()
	{
		if(x)
			backing[0] |= 2;
		else
			backing[0] &= ~2;
	}
/**
 * Get the reset flag.
 *
 * Return value: Value of resset flag.
 */
	bool reset() throw()
	{
		return ((backing[0] & 2) != 0);
	}
/**
 * Set the reset delay.
 *
 * Parameter x: The value to set reset delay to.
 */
	void delay(std::pair<short, short> x) throw()
	{
		backing[1] = static_cast<unsigned short>(x.first) >> 8;
		backing[2] = static_cast<unsigned short>(x.first);
		backing[3] = static_cast<unsigned short>(x.second) >> 8;
		backing[4] = static_cast<unsigned short>(x.second);
	}
/**
 * Get the reset delay.
 *
 * Return value: Value of reset delay.
 */
	std::pair<short, short> delay() throw()
	{
		short x, y;
		x = static_cast<short>(static_cast<unsigned short>(backing[1]) << 8);
		x |= static_cast<short>(static_cast<unsigned short>(backing[2]));
		y = static_cast<short>(static_cast<unsigned short>(backing[3]) << 8);
		y |= static_cast<short>(static_cast<unsigned short>(backing[4]));
		return std::make_pair(x, y);
	}
/**
 * Get size of frame.
 *
 * Returns: The number of bytes it takes to store frame of this type.
 */
	size_t size()
	{
		return totalsize;
	}
/**
 * Set axis/button value.
 *
 * Parameter pcid: Physical controller id.
 * Parameter ctrl: The control id.
 * Parameter x: The new value.
 */
	void axis(unsigned pcid, unsigned ctrl, short x) throw()
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		types[port]->write(backing + offsets[port], pcid % MAX_CONTROLLERS_PER_PORT, ctrl, x);
	}
/**
 * Set axis/button value.
 *
 * Parameter idx: Control index.
 * Parameter x: The new value.
 */
	void axis2(unsigned idx, short x) throw()
	{
		axis(idx / MAX_CONTROLS_PER_CONTROLLER, idx % MAX_CONTROLS_PER_CONTROLLER, x);
	}
/**
 * Get axis/button value.
 *
 * Parameter pcid: Physical controller id.
 * Parameter ctrl: The control id.
 * Return value: The axis value.
 */
	short axis(unsigned pcid, unsigned ctrl) throw()
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return types[port]->read(backing + offsets[port], pcid % MAX_CONTROLLERS_PER_PORT, ctrl);
	}

/**
 * Get axis/button value.
 *
 * Parameter idx: Index of control.
 * Return value: The axis value.
 */
	short axis2(unsigned idx) throw()
	{
		return axis(idx / MAX_CONTROLS_PER_CONTROLLER, idx % MAX_CONTROLS_PER_CONTROLLER);
	}
/**
 * Get controller display.
 *
 * Parameter pcid: Physical controller id.
 * Parameter buf: Buffer to write nul-terminated display to.
 */
	void display(unsigned pcid, char* buf) throw()
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return types[port]->display(backing + offsets[port], pcid % MAX_CONTROLLERS_PER_PORT, buf);
	}
/**
 * Is device present?
 *
 * Parameter pcid: Physical controller id.
 * Returns: True if present, false if not.
 */
	bool is_present(unsigned pcid) throw()
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return types[port]->is_present(pcid % MAX_CONTROLLERS_PER_PORT);
	}
/**
 * Deserialize frame from text format.
 *
 * Parameter buf: The buffer containing text representation. Terminated by NUL, CR or LF.
 * Throws std::runtime_error: Bad serialized representation.
 */
	void deserialize(const char* buf) throw(std::runtime_error)
	{
		size_t offset = 0;
		offset += system_deserialize(backing, buf);
		if(buf[offset] == '|')
			offset++;
		for(size_t i = 0; i < MAX_PORTS; i++) {
			size_t s = types[i]->deserialize(backing + offsets[i], buf + offset);
			if(s != DESERIALIZE_SPECIAL_BLANK) {
				offset += s;
				if(buf[offset] == '|')
					offset++;
			}
		}
	}
/**
 * Serialize frame to text format.
 *
 * Parameter buf: The buffer to write NUL-terminated text representation to.
 */
	void serialize(char* buf) throw()
	{
		size_t offset = 0;
		offset += system_serialize(backing, buf);
		for(size_t i = 0; i < MAX_PORTS; i++)
			offset += types[i]->serialize(backing + offsets[i], buf + offset);
		buf[offset++] = '\0';
	}
/**
 * Return copy with dedicated memory.
 *
 * Parameter sync: If set, the frame will have sync flag set, otherwise it will have sync flag clear.
 * Returns: Copy of this frame.
 */
	controller_frame copy(bool sync)
	{
		controller_frame c(*this);
		c.sync(sync);
		return c;
	}
/**
 * Compare two frames.
 *
 * Parameter obj: Another frame.
 * Returns: True if equal, false if not.
 */
	bool operator==(const controller_frame& obj) const throw()
	{
		if(!types_match(obj))
			return false;
		return !memcmp(backing, obj.backing, totalsize);
	}
/**
 * Compare two frames.
 *
 * Parameter obj: Another frame.
 * Returns: True if not equal, false if equal.
 */
	bool operator!=(const controller_frame& obj) const throw()
	{
		return !(*this == obj);
	}
/**
 * Get physical button ID for physical controller ID and logical button ID.
 *
 * Parameter pcid: Physical controller id.
 * Parameter lbid: Logical button id.
 * Returns: The physical button id, or -1 if no such button.
 */
	int button_id(unsigned pcid, unsigned lbid)
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return types[port]->button_id(pcid % MAX_CONTROLLERS_PER_PORT, lbid);
	}
/**
 * Does the specified controller have analog function.
 *
 * Parameter pcid: Physical controller id.
 */
	bool is_analog(unsigned pcid)
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return types[port]->is_analog(pcid % MAX_CONTROLLERS_PER_PORT);
	}
/**
 * Does the specified controller have mouse-type function.
 *
 * Parameter pcid: Physical controller id.
 */
	bool is_mouse(unsigned pcid)
	{
		unsigned port = (pcid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return types[port]->is_mouse(pcid % MAX_CONTROLLERS_PER_PORT);
	}
private:
	size_t totalsize;
	unsigned char memory[MAXIMUM_CONTROLLER_FRAME_SIZE];
	unsigned char* backing;
	port_type* types[MAX_PORTS];
	size_t offsets[MAX_PORTS];
	static size_t system_serialize(const unsigned char* buffer, char* textbuf);
	static size_t system_deserialize(unsigned char* buffer, const char* textbuf);
	void set_types(port_type** tarr);
};

/**
 * Vector of controller frames.
 */
class controller_frame_vector
{
public:
/**
 * Construct new controller frame vector.
 */
	controller_frame_vector() throw(std::runtime_error);
/**
 * Construct new controller frame vector.
 *
 * Parameter p1: Type of port 1.
 * Parameter p2: Type of port 2.
 * Throws std::runtime_error: Illegal port types.
 */
	controller_frame_vector(port_type& p1, port_type& p2) throw(std::runtime_error);
/**
 * Destroy controller frame vector
 */
	~controller_frame_vector() throw();
/**
 * Copy controller frame vector.
 *
 * Parameter obj: The object to copy.
 * Throws std::bad_alloc: Not enough memory.
 */
	controller_frame_vector(const controller_frame_vector& vector) throw(std::bad_alloc);
/**
 * Assign controller frame vector.
 *
 * Parameter obj: The object to copy.
 * Returns: Reference to this.
 * Throws std::bad_alloc: Not enough memory.
 */
	controller_frame_vector& operator=(const controller_frame_vector& vector) throw(std::bad_alloc);
/**
 * Blank vector and change the type of ports.
 *
 * Parameter p1: Type of port 1.
 * Parameter p2: Type of port 2.
 * Throws std::runtime_error: Illegal port types.
 */
	void clear(port_type& p1, port_type& p2) throw(std::runtime_error);
/**
 * Blank vector.
 */
	void clear() throw()
	{
		clear(*types[0], *types[1]);
	}
/**
 * Get number of subframes.
 */
	size_t size()
	{
		return frames;
	}
/**
 * Access specified subframe.
 *
 * Parameter x: The frame number.
 * Returns: The controller frame.
 * Throws std::runtime_error: Invalid frame index.
 */
	controller_frame operator[](size_t x)
	{
		size_t page = x / frames_per_page;
		size_t pageoffset = frame_size * (x % frames_per_page);
		if(x >= frames)
			throw std::runtime_error("controller_frame_vector::operator[]: Illegal index");
		if(page != cache_page_num) {
			cache_page = &pages[page];
			cache_page_num = page;
		}
		return controller_frame(cache_page->content + pageoffset, *types[0], *types[1]);
	}
/**
 * Append a subframe.
 *
 * Parameter frame: The frame to append.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Port type mismatch.
 */
	void append(controller_frame frame) throw(std::bad_alloc, std::runtime_error);
/**
 * Change length of vector.
 *
 * - Reducing length of vector will discard extra elements.
 * - Extending length of vector will add all-zero elements.
 *
 * Parameter newsize: New size of vector.
 * Throws std::bad_alloc: Not enough memory.
 */
	void resize(size_t newsize) throw(std::bad_alloc);
/**
 * Walk the indexes of sync subframes.
 *
 * - If frame is in range and there is at least one more sync subframe after it, the index of first sync subframe
 *   after given frame.
 * - If frame is in range, but there are no more sync subframes after it, the length of vector is returned.
 * - If frame is out of range, the given frame is returned.
 * 
 * Parameter frame: The frame number to start search from.
 * Returns: Index of next sync frame.
 */
	size_t walk_sync(size_t frame) throw()
	{
		return walk_helper(frame, true);
	}
/**
 * Get number of subframes in frame. The given subframe is assumed to be sync subframe.
 *
 * - The return value is the same as (walk_sync(frame) - frame).
 * 
 * Parameter frame: The frame number to start search from.
 * Returns: Number of subframes in this frame.
 */
	size_t subframe_count(size_t frame) throw()
	{
		return walk_helper(frame, false);
	}
/**
 * Count number of subframes in vector with sync flag set.
 *
 * Returns: The number of frames.
 */
	size_t count_frames() throw();
/**
 * Return blank controller frame with correct type and dedicated memory.
 *
 * Parameter sync: If set, the frame will have sync flag set, otherwise it will have sync flag clear.
 * Returns: Blank frame.
 */
	controller_frame blank_frame(bool sync)
	{
		controller_frame c(*types[0], *types[1]);
		c.sync(sync);
		return c;
	}
private:
	class page
	{
	public:
		page() { memset(content, 0, CONTROLLER_PAGE_SIZE); }
		unsigned char content[CONTROLLER_PAGE_SIZE];
	};
	size_t frames_per_page;
	size_t frame_size;
	size_t frames;
	port_type* types[MAX_PORTS];
	size_t cache_page_num;
	page* cache_page;
	std::map<size_t, page> pages;
	size_t walk_helper(size_t frame, bool sflag) throw();
	void clear_cache()
	{
		cache_page_num = 0;
		cache_page_num--;
		cache_page = NULL;
	}
};

/**
 * Controllers state.
 */
class controller_state
{
public:
/**
 * Constructor.
 */
	controller_state() throw();
/**
 * Convert lcid (Logical Controller ID) into pcid (Physical Controler ID).
 *
 * Parameter lcid: The logical controller ID.
 * Return: The physical controller ID, or -1 if no such controller exists.
 */
	int lcid_to_pcid(unsigned lcid) throw();
/**
 * Convert acid (Analog Controller ID) into pcid.
 *
 * Parameter acid: The analog controller ID.
 * Return: The physical controller ID, or -1 if no such controller exists.
 */
	int acid_to_pcid(unsigned acid) throw();
/**
 * Is given acid a mouse?
 *
 * Parameter acid: The analog controller ID.
 * Returns: True if given acid is mouse, false otherwise.
 */
	bool acid_is_mouse(unsigned acid) throw();
/**
 * Is given pcid present?
 *
 * Parameter pcid: The physical controller id.
 * Returns: True if present, false if not.
 */
	bool pcid_present(unsigned pcid) throw();
/**
 * Set type of port.
 *
 * Parameter port: The port to set.
 * Parameter ptype: The new type for port.
 * Parameter set_core: If true, set the core port type too, otherwise don't do that.
 * Throws std::runtime_error: Illegal port type.
 */
	void set_port(unsigned port, port_type& ptype, bool set_core) throw(std::runtime_error);
/**
 * Get status of current controls (with autohold/autofire factored in).
 *
 * Parameter framenum: Number of current frame (for evaluating autofire).
 * Returns: The current controls.
 */
	controller_frame get(uint64_t framenum) throw();
/**
 * Commit given controls (autohold/autofire is factored in).
 *
 * Parameter framenum: Number of current frame (for evaluating autofire).
 * Returns: The committed controls.
 */
	controller_frame commit(uint64_t framenum) throw();
/**
 * Commit given controls (autohold/autofire is ignored).
 *
 * Parameter controls: The controls to commit
 * Returns: The committed controls.
 */
	controller_frame commit(controller_frame controls) throw();
/**
 * Get status of committed controls.
 * Returns: The committed controls.
 */
	controller_frame get_committed() throw();
/**
 * Get blank frame.
 */
	controller_frame get_blank() throw();
/**
 * Send analog input to given acid.
 *
 * Parameter acid: The acid to send input to.
 * Parameter x: The x coordinate to send.
 * Parameter y: The x coordinate to send.
 */
	void analog(unsigned acid, int x, int y) throw();
/**
 * Manipulate the reset flag.
 *
 * Parameter delay: Delay for reset (-1 for no reset)
 */
	void reset(int32_t delay) throw();
/**
 * Manipulate autohold.
 *
 * Parameter pcid: The physical controller ID to manipulate.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter newstate: The new state for autohold.
 */
	void autohold(unsigned pcid, unsigned pbid, bool newstate) throw();
/**
 * Query autohold.
 *
 * Parameter pcid: The physical controller ID to query.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of autohold.
 */
	bool autohold(unsigned pcid, unsigned pbid) throw();
/**
 * Reset all frame holds.
 */
	void reset_framehold() throw();
/**
 * Manipulate hold for frame.
 *
 * Parameter pcid: The physical controller ID to manipulate.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter newstate: The new state for framehold.
 */
	void framehold(unsigned pcid, unsigned pbid, bool newstate) throw();
/**
 * Query hold for frame.
 *
 * Parameter pcid: The physical controller ID to query.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of framehold.
 */
	bool framehold(unsigned pcid, unsigned pbid) throw();
/**
 * Manipulate button.
 *
 * Parameter pcid: The physical controller ID to manipulate.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter newstate: The new state for button.
 */
	void button(unsigned pcid, unsigned pbid, bool newstate) throw();
/**
 * Query button.
 *
 * Parameter pcid: The physical controller ID to query.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of button.
 */
	bool button(unsigned pcid, unsigned pbid) throw();
/**
 * Set autofire pattern.
 *
 * Parameter pattern: The new pattern.
 * Throws std::bad_alloc: Not enough memory.
 */
	void autofire(std::vector<controller_frame> pattern) throw(std::bad_alloc);
/**
 * Get physical button ID for physical controller ID and logical button ID.
 *
 * Parameter pcid: Physical controller id.
 * Parameter lbid: Logical button id.
 * Returns: The physical button id, or -1 if no such button.
 */
	int button_id(unsigned pcid, unsigned lbid) throw();
/**
 * TODO: Document.
 */
	bool is_present(unsigned pcid) throw();
/**
 * TODO: Document.
 */
	bool is_analog(unsigned pcid) throw();
/**
 * TODO: Document.
 */
	bool is_mouse(unsigned pcid) throw();
private:
	port_type* porttypes[MAX_PORTS];
	int analog_indices[MAX_ANALOG];
	bool analog_mouse[MAX_ANALOG];
	controller_frame _input;
	controller_frame _autohold;
	controller_frame _framehold;
	controller_frame _committed;
	std::vector<controller_frame> _autofire;
};


#endif
