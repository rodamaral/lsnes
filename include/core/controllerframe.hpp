#ifndef _controllerframe__hpp__included__
#define _controllerframe__hpp__included__

#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <set>

/**
 * For now, reserve 20 bytes, for:
 *
 * - 5 bytes for system.
 * - 6 bytes for port 1 (multitap). 
 * - 9 bytes for port 2 (justifiers).
 */
#define MAXIMUM_CONTROLLER_FRAME_SIZE 20

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
 * Special return value for deserialize() indicating no input was taken.
 */
#define DESERIALIZE_SPECIAL_BLANK 0xFFFFFFFFUL

/**
 * This enumeration gives the type of port.
 */
enum porttype_t
{
/**
 * No device
 */
	PT_NONE = 0,			//Nothing connected to port.
/**
 * Gamepad
 */
	PT_GAMEPAD = 1,
/**
 * Multitap (with 4 gamepads connected)
 */
	PT_MULTITAP = 2,
/**
 * Mouse
 */
	PT_MOUSE = 3,
/**
 * Superscope (only allowed for port 2).
 */
	PT_SUPERSCOPE = 4,
/**
 * Justifier (only allowed for port 2).
 */
	PT_JUSTIFIER = 5,
/**
 * 2 Justifiers (only allowed for port 2).
 */
	PT_JUSTIFIERS = 6,
/**
 * Number of controller types.
 */
	PT_LAST_CTYPE = 6,
/**
 * Invalid controller type.
 */
	PT_INVALID = PT_LAST_CTYPE + 1
};

/**
 * This enumeration gives the type of device.
 */
enum devicetype_t
{
/**
 * No device
 */
	DT_NONE = 0,
/**
 * Gamepad (note that multitap controllers are gamepads)
 */
	DT_GAMEPAD = 1,
/**
 * Mouse
 */
	DT_MOUSE = 3,
/**
 * Superscope
 */
	DT_SUPERSCOPE = 4,
/**
 * Justifier (note that justifiers is two of these).
 */
	DT_JUSTIFIER = 5
};

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
 * Parameter p: The port type.
 * Returns: Infor about port type.
 * Throws std::runtime_error: Invalid port type.
 */
	static const porttype_info& lookup(porttype_t p) throw(std::runtime_error);
/**
 * Look up information about port type.
 *
 * Parameter p: The port type string.
 * Returns: Infor about port type.
 * Throws std::runtime_error: Invalid port type.
 */
	static const porttype_info& lookup(const std::string& p) throw(std::runtime_error);
/**
 * Register port type.
 *
 * Parameter ptype: Type value for port type.
 * Parameter pname: The name of port type.
 * Parameter psize: The size of storage for this type.
 * Throws std::bad_alloc: Not enough memory.
 */
	porttype_info(porttype_t ptype, const std::string& pname, size_t psize) throw(std::bad_alloc);
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
	virtual void write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) const throw() = 0;
/**
 * Read controller data from compressed representation.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter idx: Index of controller.
 * Parameter ctrl: The control to query.
 * Returns: The value of control. Buttons return 0 or 1.
 */
	virtual short read(const unsigned char* buffer, unsigned idx, unsigned ctrl) const  throw() = 0;
/**
 * Format compressed controller data into input display.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter idx: Index of controller.
 * Parameter buf: The buffer to write NUL-terminated display string to. Assumed to be MAX_DISPLAY_LENGTH bytes in size.
 */
	virtual void display(const unsigned char* buffer, unsigned idx, char* buf) const  throw() = 0;
/**
 * Take compressed controller data and serialize it into textual representation.
 *
 * - The initial '|' is also written.
 *
 * Parameter buffer: The buffer storing compressed representation of controller state.
 * Parameter textbuf: The text buffer to write to.
 * Returns: Number of bytes written.
 */
	virtual size_t serialize(const unsigned char* buffer, char* textbuf) const  throw() = 0;
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
	virtual size_t deserialize(unsigned char* buffer, const char* textbuf) const  throw() = 0;
/**
 * Return device type for given index.
 *
 * Parameter idx: The index of controller.
 * Returns: The type of device.
 */
	virtual devicetype_t devicetype(unsigned idx) const  throw() = 0;
/**
 * Number of controllers connected to this port.
 */
	virtual unsigned controllers() const  throw() = 0;
/**
 * Internal type value for port.
 */
	virtual unsigned internal_type() const  throw() = 0;
/**
 * Return if type is legal for port.
 *
 * Parameter port: Number of port.
 * Returns: True if legal, false if not.
 */
	virtual bool legal(unsigned port) const  throw() = 0;
/**
 * Port type value.
 */
	porttype_t value;
/**
 * Number of bytes it takes to store this.
 */
	size_t storage_size;
/**
 * Name of port type.
 */
	std::string name;
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
 * Parameter pid: The physical controller id.
 * Parameter ctrl: The control id.
 */
	void clear_DRDY(unsigned pid, unsigned ctrl) throw();
/**
 * Get state of DRDY bit.
 *
 * Parameter pid: The physical controller id.
 * Parameter ctrl: The control id.
 * Returns: The DRDY state.
 */
	bool get_DRDY(unsigned pid, unsigned ctrl) throw();
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
 * Parameter pid: The physical controller id.
 * Parameter ctrl: The control id.
 * Return: The poll count.
 */
	uint32_t get_polls(unsigned pid, unsigned ctrl) throw();
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
 * Parameter pid: The physical controller id.
 * Parameter ctrl: The control id.
 * Return: The poll count pre-increment.
 */
	uint32_t increment_polls(unsigned pid, unsigned ctrl) throw();
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
	controller_frame(porttype_t p1, porttype_t p2) throw(std::runtime_error);
/**
 * Create subframe of controls with specified controller types and specified memory.
 *
 * Parameter memory: The backing memory.
 * Parameter p1: Type of port1.
 * Parameter p2: Type of port2.
 *
 * Throws std::runtime_error: Invalid port type or NULL memory.
 */
	controller_frame(unsigned char* memory, porttype_t p1 = PT_GAMEPAD, porttype_t p2 = PT_NONE)
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
	porttype_t get_port_type(unsigned port) throw()
	{
		return (port < MAX_PORTS) ? types[port] : PT_NONE;
	}
/**
 * Get blank dedicated frame of same port types.
 *
 * Return blank frame.
 */
	controller_frame blank_frame() throw()
	{
		return controller_frame(types[0], types[1]);
	}
/**
 * Set type of port. Input for that port is zeroized.
 *
 * Parameter port: Number of port.
 * Parameter type: The new type.
 * Throws std::runtime_error: Bad port type or non-dedicated memory.
 */
	void set_port_type(unsigned port, porttype_t ptype) throw(std::runtime_error);
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
 * Parameter pid: Physical controller id.
 * Parameter ctrl: The control id.
 * Parameter x: The new value.
 */
	void axis(unsigned pid, unsigned ctrl, short x) throw()
	{
		unsigned port = (pid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		pinfo[port]->write(backing + offsets[port], pid % MAX_CONTROLLERS_PER_PORT, ctrl, x);
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
 * Parameter pid: Physical controller id.
 * Parameter ctrl: The control id.
 * Return value: The axis value.
 */
	short axis(unsigned pid, unsigned ctrl) throw()
	{
		unsigned port = (pid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return pinfo[port]->read(backing + offsets[port], pid % MAX_CONTROLLERS_PER_PORT, ctrl);
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
 * Parameter pid: Physical controller id.
 * Parameter buf: Buffer to write nul-terminated display to.
 */
	void display(unsigned pid, char* buf) throw()
	{
		unsigned port = (pid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return pinfo[port]->display(backing + offsets[port], pid % MAX_CONTROLLERS_PER_PORT, buf);
	}
/**
 * Get device type.
 *
 * Parameter pid: Physical controller id.
 * Returns: Device type.
 */
	devicetype_t devicetype(unsigned pid) throw()
	{
		unsigned port = (pid / MAX_CONTROLLERS_PER_PORT) % MAX_PORTS;
		return pinfo[port]->devicetype(pid % MAX_CONTROLLERS_PER_PORT);
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
			size_t s = pinfo[i]->deserialize(backing + offsets[i], buf + offset);
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
		for(size_t i = 0; i < MAX_PORTS; i++) {
			offset += pinfo[i]->serialize(backing + offsets[i], buf + offset);
			buf[offset++] = (i < MAX_PORTS - 1) ? '|' : '\0';
		}
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
private:
	size_t totalsize;
	unsigned char memory[MAXIMUM_CONTROLLER_FRAME_SIZE];
	unsigned char* backing;
	porttype_t types[MAX_PORTS];
	size_t offsets[MAX_PORTS];
	const porttype_info* pinfo[MAX_PORTS];
	static size_t system_serialize(const unsigned char* buffer, char* textbuf);
	static size_t system_deserialize(unsigned char* buffer, const char* textbuf);
	void set_types(const porttype_t* tarr);
};

/**
 * Vector of controller frames.
 */
class controller_frame_vector
{
public:
/**
 * Construct new controller frame vector.
 *
 * Parameter p1: Type of port 1.
 * Parameter p2: Type of port 2.
 * Throws std::runtime_error: Illegal port types.
 */
	controller_frame_vector(enum porttype_t p1 = PT_GAMEPAD, enum porttype_t p2 = PT_NONE)
		throw(std::runtime_error);
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
	void clear(enum porttype_t p1, enum porttype_t p2) throw(std::runtime_error);
/**
 * Blank vector.
 */
	void clear() throw()
	{
		clear(types[0], types[1]);
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
		return controller_frame(cache_page->content + pageoffset, types[0], types[1]);
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
		controller_frame c(types[0], types[1]);
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
	porttype_t types[MAX_PORTS];
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

#endif
