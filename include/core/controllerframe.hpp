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
#define MAX_CONTROLS_PER_CONTROLLER 16
/**
 * Number of button controls.
 */
#define MAX_BUTTONS MAX_PORTS * MAX_CONTROLLERS_PER_PORT * MAX_CONTROLS_PER_CONTROLLER
/**
 * Size of controller page.
 */
#define CONTROLLER_PAGE_SIZE 65500
/**
 * Special return value for deserialize() indicating no input was taken.
 */
#define DESERIALIZE_SPECIAL_BLANK 0xFFFFFFFFUL
/**
 * Analog indices.
 */
#define MAX_ANALOG 3

/**
 * Is not field terminator.
 *
 * Parameter ch: The character.
 * Returns: True if character is not terminator, false if character is terminator.
 */
inline bool is_nonterminator(char ch) throw()
{
	return (ch != '|' && ch != '\r' && ch != '\n' && ch != '\0');
}

/**
 * Read button value.
 *
 * Parameter buf: Buffer to read from.
 * Parameter idx: Index to buffer. Updated.
 * Returns: The read value.
 */
inline bool read_button_value(const char* buf, size_t& idx) throw()
{
	char ch = buf[idx];
	if(is_nonterminator(ch))
		idx++;
	return (ch != '|' && ch != '\r' && ch != '\n' && ch != '\0' && ch != '.' && ch != ' ' && ch != '\t');
}

/**
 * Read axis value.
 *
 * Parameter buf: Buffer to read from.
 * Parameter idx: Index to buffer. Updated.
 * Returns: The read value.
 */
short read_axis_value(const char* buf, size_t& idx) throw();

/**
 * Skip whitespace.
 *
 * Parameter buf: Buffer to read from.
 * Parameter idx: Index to buffer. Updated.
 */
inline void skip_field_whitespace(const char* buf, size_t& idx) throw()
{
	while(buf[idx] == ' ' || buf[idx] == '\t')
		idx++;
}

/**
 * Skip rest of the field.
 *
 * Parameter buf: Buffer to read from.
 * Parameter idx: Index to buffer. Updated.
 * Parameter include_pipe: If true, also skip the '|'.
 */
inline void skip_rest_of_field(const char* buf, size_t& idx, bool include_pipe) throw()
{
	while(is_nonterminator(buf[idx]))
		idx++;
	if(include_pipe && buf[idx] == '|')
		idx++;
}

/**
 * Serialize short.
 */
inline void serialize_short(unsigned char* buf, short val)
{
	buf[0] = static_cast<unsigned short>(val) >> 8;
	buf[1] = static_cast<unsigned short>(val);
}

/**
 * Serialize short.
 */
inline short unserialize_short(const unsigned char* buf)
{
	return static_cast<short>((static_cast<unsigned short>(buf[0]) << 8) | static_cast<unsigned short>(buf[1]));
}

/**
 * Information about port type.
 */
struct porttype_info
{
/**
 * Look up information about port type.
 *
 * Parameter p: The port type string.
 * Returns: Infor about port type.
 * Throws std::runtime_error: Invalid port type.
 */
	static porttype_info& lookup(const std::string& p) throw(std::runtime_error);
/**
 * Get set of all available port types.
 */
	static std::list<porttype_info*> get_all();
/**
 * Get some default port type.
 */
	static porttype_info& default_type();
/**
 * Get port default type.
 */
	static porttype_info& port_default(unsigned port);
/**
 * Register port type.
 *
 * Parameter pname: The name of port type.
 * Parameter hname: Human-readable name of the port type.
 * Parameter psize: The size of storage for this type.
 * Throws std::bad_alloc: Not enough memory.
 */
	porttype_info(const std::string& pname, const std::string& hname, unsigned _pt_id, size_t psize)
		throw(std::bad_alloc);
/**
 * Unregister port type.
 */
	~porttype_info() throw();
/**
 * Writes controller data into compressed representation.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter idx: Index of controller.
 * Parameter ctrl: The control to manipulate.
 * Parameter x: New value for control. Only zero/nonzero matters for buttons.
 */
	void (*write)(unsigned char* buffer, unsigned idx, unsigned ctrl, short x);
/**
 * Read controller data from compressed representation.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter idx: Index of controller.
 * Parameter ctrl: The control to query.
 * Returns: The value of control. Buttons return 0 or 1.
 */
	short (*read)(const unsigned char* buffer, unsigned idx, unsigned ctrl);
/**
 * Format compressed controller data into input display.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter idx: Index of controller.
 * Parameter buf: The buffer to write NUL-terminated display string to. Assumed to be MAX_DISPLAY_LENGTH bytes in size.
 */
	void (*display)(const unsigned char* buffer, unsigned idx, char* buf);
/**
 * Take compressed controller data and serialize it into textual representation.
 *
 * - The initial '|' is also written.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter textbuf: The text buffer to write to.
 * Returns: Number of bytes written.
 */
	size_t (*serialize)(const unsigned char* buffer, char* textbuf);
/**
 * Unserialize textual representation into compressed controller state.
 *
 * - Only stops reading on '|', NUL, CR or LF in the final read field. That byte is not read.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter textbuf: The text buffer to read.
 * Returns: Number of bytes read.
 * Throws std::runtime_error: Bad serialization.
 */
	size_t (*deserialize)(unsigned char* buffer, const char* textbuf);
/**
 * Get device flags for given index.
 *
 * Parameter idx: The index of controller.
 * Returns: The device flags.
 *	Bit 0: Present.
 *	Bit 1: Has absolute analog axes 0 and 1.
 *	Bit 2: Has relative analog axes 0 and 1.
 */
	unsigned (*deviceflags)(unsigned idx);
/**
 * Is the device legal for port?
 *
 * Parameter port: Port to query.
 * Returns: Nonzero if legal, zero if illegal.
 */
	int (*legal)(unsigned port);
/**
 * Number of controllers connected to this port.
 */
	unsigned controllers;
/**
 * Translate controller and logical button id pair into physical button id.
 *
 * Parameter controller: The number of controller.
 * Parameter lbid: Logigal button ID.
 * Returns: The physical button ID, or -1 if no such button exists.
 */
	int (*button_id)(unsigned controller, unsigned lbid);
/**
 * Set this controller as core controller.
 *
 * Parameter port: Port to set to.
 */
	void (*set_core_controller)(unsigned port);
/**
 * Does the controller exist?
 *
 * Parameter controller: Controller number.
 */
	bool is_present(unsigned controller) const throw();
/**
 * Does the controller have analog function?
 *
 * Parameter controller: Controller number.
 */
	bool is_analog(unsigned controller) const throw();
/**
 * Does the controller have mouse-type function?
 *
 * Parameter controller: Controller number.
 */
	bool is_mouse(unsigned controller) const throw();
/**
 * Human-readable name.
 */
	std::string hname;
/**
 * Number of bytes it takes to store this.
 */
	size_t storage_size;
/**
 * Name of port type.
 */
	std::string name;
/**
 * Name of controller.
 */
	std::string ctrlname;
/**
 * Id of the port.
 */
	unsigned pt_id;
/**
 * Symbols of buttons.
 */
	const char* button_symbols;
private:
	porttype_info(const porttype_info&);
	porttype_info& operator=(const porttype_info&);
};

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
	controller_frame(porttype_info& p1, porttype_info& p2) throw(std::runtime_error);
/**
 * Create subframe of controls with specified controller types and specified memory.
 *
 * Parameter memory: The backing memory.
 * Parameter p1: Type of port1.
 * Parameter p2: Type of port2.
 *
 * Throws std::runtime_error: Invalid port type or NULL memory.
 */
	controller_frame(unsigned char* memory, porttype_info& p1, porttype_info& p2)
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
	porttype_info& get_port_type(unsigned port) throw()
	{
		return (port < MAX_PORTS) ? *types[port] : porttype_info::default_type();
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
	void set_port_type(unsigned port, porttype_info& ptype) throw(std::runtime_error);
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
	porttype_info* types[MAX_PORTS];
	size_t offsets[MAX_PORTS];
	static size_t system_serialize(const unsigned char* buffer, char* textbuf);
	static size_t system_deserialize(unsigned char* buffer, const char* textbuf);
	void set_types(porttype_info** tarr);
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
	controller_frame_vector(porttype_info& p1, porttype_info& p2) throw(std::runtime_error);
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
	void clear(porttype_info& p1, porttype_info& p2) throw(std::runtime_error);
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
/**
 * Return number of pages in movie.
 */
	size_t get_page_count() { return pages.size(); }
/**
 * Return the stride.
 */
	size_t get_stride() { return frame_size; }
/**
 * Return number of frames per page.
 */
	size_t get_frames_per_page() { return frames_per_page; }
/**
 * Get content of given page.
 */
	unsigned char* get_page_buffer(size_t page) { return pages[page].content; }
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
	porttype_info* types[MAX_PORTS];
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
	void set_port(unsigned port, porttype_info& ptype, bool set_core) throw(std::runtime_error);
/**
 * Get status of current controls (with autohold/autofire factored in).
 *
 * Parameter framenum: Number of current frame (for evaluating autofire).
 * Returns: The current controls.
 */
	controller_frame get(uint64_t framenum) throw();
/**
 * Commit given controls (autohold/autofire is ignored).
 *
 * Parameter controls: The controls to commit
 */
	void commit(controller_frame controls) throw();
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
	porttype_info* porttypes[MAX_PORTS];
	int analog_indices[MAX_ANALOG];
	bool analog_mouse[MAX_ANALOG];
	controller_frame _input;
	controller_frame _autohold;
	controller_frame _framehold;
	controller_frame _committed;
	std::vector<controller_frame> _autofire;
};

/**
 * Generic port write function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline void generic_port_write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) throw()
{
	if(idx >= controllers)
		return;
	if(ctrl < analog_axis) {
		buffer[2 * idx * analog_axis + 2 * ctrl] = (x >> 8);
		buffer[2 * idx * analog_axis + 2 * ctrl + 1] = x;
	} else if(ctrl < analog_axis + buttons) {
		size_t bit = 16 * controllers * analog_axis + idx * buttons + ctrl - analog_axis;
		if(x)
			buffer[bit / 8] |= (1 << (bit % 8));
		else
			buffer[bit / 8] &= ~(1 << (bit % 8));
	}
}

/**
 * Generic port read function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline short generic_port_read(const unsigned char* buffer, unsigned idx, unsigned ctrl) throw()
{
	if(idx >= controllers)
		return 0;
	if(ctrl < analog_axis) {
		uint16_t a = buffer[2 * idx * analog_axis + 2 * ctrl];
		uint16_t b = buffer[2 * idx * analog_axis + 2 * ctrl + 1];
		return static_cast<short>(256 * a + b);
	} else if(ctrl < analog_axis + buttons) {
		size_t bit = 16 * controllers * analog_axis + idx * buttons + ctrl - analog_axis;
		return ((buffer[bit / 8] & (1 << (bit % 8))) != 0);
	} else
		return 0;
}

extern const char* button_symbols;

/**
 * Generic port display function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons, unsigned sidx>
inline void generic_port_display(const unsigned char* buffer, unsigned idx, char* buf) throw()
{
	if(idx > controllers) {
		buf[0] = '\0';
		return;
	}
	size_t ptr = 0;
	for(unsigned i = 0; i < analog_axis; i++) {
		uint16_t a = buffer[2 * idx * analog_axis + 2 * i];
		uint16_t b = buffer[2 * idx * analog_axis + 2 * i + 1];
		ptr += sprintf(buf + ptr, "%i ", static_cast<short>(256 * a + b));
	}
	for(unsigned i = 0; i < buttons; i++) {
		size_t bit = 16 * controllers * analog_axis + idx * buttons + i;
		buf[ptr++] = ((buffer[bit / 8] & (1 << (bit % 8))) != 0) ? button_symbols[i + sidx] : '-';
	}
	buf[ptr] = '\0';
}

/**
 * Generic port serialization function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons, unsigned sidx>
inline size_t generic_port_serialize(const unsigned char* buffer, char* textbuf) throw()
{
	size_t ptr = 0;
	for(unsigned j = 0; j < controllers; j++) {
		textbuf[ptr++] = '|';
		for(unsigned i = 0; i < buttons; i++) {
			size_t bit = 16 * controllers * analog_axis + j * buttons + i;
			textbuf[ptr++] = ((buffer[bit / 8] & (1 << (bit % 8))) != 0) ? button_symbols[i + sidx] : '.';
		}
		for(unsigned i = 0; i < analog_axis; i++) {
			uint16_t a = buffer[2 * j * analog_axis + 2 * i];
			uint16_t b = buffer[2 * j * analog_axis + 2 * i + 1];
			ptr += sprintf(textbuf + ptr, " %i", static_cast<short>(256 * a + b));
		}
	}
	return ptr;
}

/**
 * Generic port size function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline size_t generic_port_size()
{
	return 2 * controllers * analog_axis + (controllers * buttons + 7) / 8;
}

/**
 * Generic port deserialization function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline size_t generic_port_deserialize(unsigned char* buffer, const char* textbuf) throw()
{
	if(!controllers)
		return DESERIALIZE_SPECIAL_BLANK;
	memset(buffer, 0, generic_port_size<controllers, analog_axis, buttons>());
	size_t ptr = 0;
	for(unsigned j = 0; j < controllers; j++) {
		for(unsigned i = 0; i < buttons; i++) {
			size_t bit = 16 * controllers * analog_axis + j * buttons + i;
			if(read_button_value(textbuf, ptr))
				buffer[bit / 8] |= (1 << (bit % 8));
		}
		for(unsigned i = 0; i < analog_axis; i++) {
			short v = read_axis_value(textbuf, ptr);
			buffer[2 * j * analog_axis + 2 * i] = v >> 8;
			buffer[2 * j * analog_axis + 2 * i + 1] = v;
		}
		skip_rest_of_field(textbuf, ptr, j + 1 < controllers);
	}
	return ptr;
}

template<unsigned mask>
inline int generic_port_legal(unsigned port) throw()
{
	if(port >= CHAR_BIT * sizeof(unsigned))
		port = CHAR_BIT * sizeof(unsigned) - 1;
	return ((mask >> port) & 1);
}

/**
 * Generic port type function.
 */
template<unsigned controllers, unsigned flags>
inline unsigned generic_port_deviceflags(unsigned idx) throw()
{
	return (idx < controllers) ? flags : 0;
}

#endif
