#ifndef _library__controller_data__hpp__included__
#define _library__controller_data__hpp__included__

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
#include <set>

/**
 * Memory to allocate for controller frame.
 */
#define MAXIMUM_CONTROLLER_FRAME_SIZE 128
/**
 * Maximum amount of data controller_frame::display() can write.
 */
#define MAX_DISPLAY_LENGTH 128
/**
 * Maximum amount of data controller_frame::serialize() can write.
 */
#define MAX_SERIALIZED_SIZE 256
/**
 * Size of controller page.
 */
#define CONTROLLER_PAGE_SIZE 65500
/**
 * Special return value for deserialize() indicating no input was taken.
 */
#define DESERIALIZE_SPECIAL_BLANK 0xFFFFFFFFUL

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

class port_type;

/**
 * Index triple.
 *
 * Note: The index 0 has to be mapped to triple (0, 0, 0).
 */
struct port_index_triple
{
/**
 * If true, the other parameters are valid. Otherwise this index doesn't correspond to anything valid, but still
 * exists. The reason for having invalid entries is to be backward-compatible.
 */
	bool valid;
/**
 * The port number.
 */
	unsigned port;
/**
 * The controller number.
 */
	unsigned controller;
/**
 * The control number.
 */
	unsigned control;
/**
 * Does neutral poll of this index count as non-lag frame (non-neutral polls always do)?
 */
	bool marks_nonlag;
};

/**
 * Controller index mappings
 */
struct port_index_map
{
/**
 * The poll indices.
 */
	std::vector<port_index_triple> indices;
/**
 * The logical controller mappings.
 */
	std::vector<std::pair<unsigned, unsigned>> logical_map;
/**
 * Legacy PCID mappings.
 */
	std::vector<std::pair<unsigned, unsigned>> pcid_map;
};

/**
 * Group of port types.
 */
class port_type_group
{
public:
/**
 * Construct a group.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	port_type_group() throw(std::bad_alloc);
/**
 * Destroy a group.
 */
	~port_type_group() throw();
/**
 * Get port type with specified name.
 *
 * Parameter name: The name of port type.
 * Returns: The port type structure.
 * Throws std::runtime_error: Specified port type unknown.
 */
	port_type& get_type(const std::string& name) const throw(std::runtime_error);
/**
 * Get set of all port types.
 *
 * Returns: The set of all port types.
 * Throws std::bad_alloc: Not enough memory.
 */
	std::set<port_type*> get_types() const throw(std::bad_alloc);
/**
 * Get default port type for specified port.
 *
 * Parameter port: The port.
 * Returns: The default port type.
 * Throws std::runtime_error: Bad port.
 */
	port_type& get_default_type(unsigned port) const throw(std::runtime_error);
/**
 * Add a port type.
 *
 * Parameter type: Name of the type.
 * Parameter port: The port type structure.
 * Throws std::bad_alloc: Not enough memory.
 */
	void register_type(const std::string& type, port_type& port);
/**
 * Remove a port type.
 *
 * Parameter type: Name of the type.
 * Throws std::bad_alloc: Not enough memory.
 */
	void unregister_type(const std::string& type);
/**
 * Set default type for port.
 *
 * Parameter port: The port.
 * Parameter ptype: The port type to make default.
 */
	void set_default(unsigned port, port_type& ptype);
private:
	std::map<std::string, port_type*> types;
	std::map<unsigned, port_type*> defaults;
};

class port_type_set;

/**
 * Type of controller.
 */
class port_type
{
public:
/**
 * Create a new port type.
 *
 * Parameter group: The group the type will belong to.
 * Parameter iname: Internal name of the port type.
 * Parameter hname: Human-readable name of the port type.
 * Parameter id: Identifier.
 * Parameter ssize: The storage size in bytes.
 * Throws std::bad_alloc: Not enough memory.
 */
	port_type(port_type_group& group, const std::string& iname, const std::string& hname, unsigned id,
		size_t ssize) throw(std::bad_alloc);
/**
 * Unregister a port type.
 */
	virtual ~port_type() throw();
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
 * Get number of used control indices on controller.
 *
 * Parameter controller: Number of controller.
 * Returns: Number of used control indices.
 */
	unsigned (*used_indices)(unsigned controller);
/**
 * Get name of controller type on controller.
 *
 * Parameter controller: Number of controller.
 * Returns: The name of controller.
 */
	const char* (*controller_name)(unsigned controller);
/**
 * Construct index map. Only called for port0.
 *
 * Parameter types: The port types to construct the map for.
 * Returns: The index map.
 */
	struct port_index_map (*construct_map)(std::vector<port_type*> types);
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
 * Id of the port.
 */
	unsigned pt_id;
/**
 * Group this port is in.
 */
	port_type_group& ingroup;
private:
	port_type(const port_type&);
	port_type& operator=(const port_type&);
};

/**
 * A set of port types.
 */
class port_type_set
{
public:
/**
 * Create empty port type set.
 */
	port_type_set() throw();
/**
 * Make a port type set with specified types. If called again with the same parameters, returns the same object.
 *
 * Parameter types: The types.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Illegal port types.
 */
	static port_type_set& make(std::vector<port_type*> types) throw(std::bad_alloc, std::runtime_error);
/**
 * Compare sets for equality.
 */
	bool operator==(const port_type_set& s) const throw() { return this == &s; }
/**
 * Compare sets for non-equality.
 */
	bool operator!=(const port_type_set& s) const throw() { return this != &s; }
/**
 * Get offset of specified port.
 *
 * Parameter port: The number of port.
 * Returns: The offset of port.
 * Throws std::runtime_error: Bad port number.
 */
	size_t port_offset(unsigned port) const throw(std::runtime_error)
	{
		if(port >= port_count)
			throw std::runtime_error("Invalid port index");
		return port_offsets[port];
	}
/**
 * Get type of specified port.
 *
 * Parameter port: The number of port.
 * Returns: The port type.
 * Throws std::runtime_error: Bad port number.
 */
	const class port_type& port_type(unsigned port) const throw(std::runtime_error)
	{
		if(port >= port_count)
			throw std::runtime_error("Invalid port index");
		return *(port_types[port]);
	}
/**
 * Get number of ports.
 *
 * Returns: The port count.
 */
	unsigned ports() const throw()
	{
		return port_count;
	}
/**
 * Get total size of controller data.
 *
 * Returns the size.
 */
	unsigned size() const throw()
	{
		return total_size;
	}
/**
 * Get total index count.
 */
	unsigned indices() const throw()
	{
		return _indices.size();
	}
/**
 * Look up the triplet for given control.
 *
 * Parameter index: The index to look up.
 * Returns: The triplet (may not be valid).
 * Throws std::runtime_error: Index out of range.
 */
	port_index_triple index_to_triple(unsigned index) const throw(std::runtime_error)
	{
		if(index >= _indices.size())
			throw std::runtime_error("Invalid index");
		return _indices[index];
	}
/**
 * Translate triplet into index.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter _index: The control index.
 * Returns: The index, or 0xFFFFFFFFUL if specified triple is not valid.
 */
	unsigned triple_to_index(unsigned port, unsigned controller, unsigned _index) const throw(std::runtime_error)
	{
		size_t place = port * port_multiplier + controller * controller_multiplier + _index;
		if(place >= indices_size)
			return 0xFFFFFFFFUL;
		unsigned pindex = indices_tab[place];
		if(pindex == 0xFFFFFFFFUL)
			return 0xFFFFFFFFUL;
		const struct port_index_triple& t = _indices[pindex];
		if(!t.valid || t.port != port || t.controller != controller || t.control != _index)
			return 0xFFFFFFFFUL;
		return indices_tab[place];
	}
/**
 * Return number of controllers connected.
 *
 * Returns: Number of controllers.
 */
	unsigned number_of_controllers() const throw()
	{
		return controllers.size();
	}
/**
 * Lookup physical controller index corresponding to logical one.
 *
 * Parameter lcid: Logical controller id.
 * Returns: Physical controller index (port, controller).
 * Throws std::runtime_error: No such controller.
 */
	std::pair<unsigned, unsigned> lcid_to_pcid(unsigned lcid) const throw(std::runtime_error)
	{
		if(lcid >= controllers.size())
			throw std::runtime_error("Bad logical controller");
		return controllers[lcid];
	}
/**
 * Return number of legacy PCIDs.
 */
	unsigned number_of_legacy_pcids() const throw()
	{
		return legacy_pcids.size();
	}
/**
 * Lookup (port,controller) pair corresponding to given legacy pcid.
 *
 * Parameter pcid: The legacy pcid.
 * Returns: The controller index.
 * Throws std::runtime_error: No such controller.
 */
	std::pair<unsigned, unsigned> legacy_pcid_to_pair(unsigned pcid) const throw(std::runtime_error)
	{
		if(pcid >= legacy_pcids.size())
			throw std::runtime_error("Bad legacy PCID");
		return legacy_pcids[pcid];
	}
/**
 * Get "marks nonlag even if neutral" flag for specified index.
 *
 * Parameter index: The index.
 * Returns: The flag.
 */
	bool marks_nonlag(unsigned index) const throw()
	{
		try {
			index_to_triple(index).marks_nonlag;
		} catch(...) {
			return false;
		}
	}
/**
 * Get "marks nonlag even if neutral" flag for specified triplet.
 *
 * Parameter port: The port
 * Parameter controller: The controller.
 * Parameter index: The index.
 * Returns: The flag.
 */
	bool marks_nonlag(unsigned port, unsigned controller, unsigned index) const throw()
	{
		try {
			index_to_triple(triple_to_index(port, controller, index)).marks_nonlag;
		} catch(...) {
			return false;
		}
	}
private:
	port_type_set(std::vector<class port_type*> types);
	size_t* port_offsets;
	class port_type** port_types;
	unsigned port_count;
	size_t total_size;
	std::vector<port_index_triple> _indices;
	std::vector<std::pair<unsigned, unsigned>> controllers;
	std::vector<std::pair<unsigned, unsigned>> legacy_pcids;

	size_t port_multiplier;
	size_t controller_multiplier;
	size_t indices_size;
	unsigned* indices_tab;
};

/**
 * Poll counter vector.
 */
class pollcounter_vector
{
public:
/**
 * Create new pollcounter vector filled with all zeroes and all DRDY bits clear.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	pollcounter_vector() throw(std::bad_alloc);
/**
 * Create new pollcounter vector suitably sized for given type set.
 *
 * Parameter p: The port types.
 * Throws std::bad_alloc: Not enough memory.
 */
	pollcounter_vector(const port_type_set& p) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~pollcounter_vector() throw();
/**
 * Copy the pollcounter_vector.
 */
	pollcounter_vector(const pollcounter_vector& v) throw(std::bad_alloc);
/**
 * Assign the pollcounter_vector.
 */
	pollcounter_vector& operator=(const pollcounter_vector& v) throw(std::bad_alloc);
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
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter ctrl: The control id.
 */
	void clear_DRDY(unsigned port, unsigned controller, unsigned ctrl) throw()
	{
		unsigned i = types->triple_to_index(port, controller, ctrl);
		if(i != 0xFFFFFFFFU)
			clear_DRDY(i);
	}
/**
 * Clear state of DRDY bit.
 *
 * Parameter idx: The control index.
 */
	void clear_DRDY(unsigned idx) throw();
/**
 * Get state of DRDY bit.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter ctrl: The control id.
 * Returns: The DRDY state.
 */
	bool get_DRDY(unsigned port, unsigned controller, unsigned ctrl) throw()
	{
		unsigned i = types->triple_to_index(port, controller, ctrl);
		if(i != 0xFFFFFFFFU)
			return get_DRDY(i);
		else
			return true;
	}
/**
 * Get state of DRDY bit.
 *
 * Parameter idx: The control index.
 * Returns: The DRDY state.
 */
	bool get_DRDY(unsigned idx) throw();
/**
 * Is any poll count nonzero or is system flag set?
 *
 * Returns: True if at least one poll count is nonzero or if system flag is set. False otherwise.
 */
	bool has_polled() throw();
/**
 * Read the actual poll count on specified control.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter ctrl: The control id.
 * Return: The poll count.
 */
	uint32_t get_polls(unsigned port, unsigned controller, unsigned ctrl) throw()
	{
		unsigned i = types->triple_to_index(port, controller, ctrl);
		if(i != 0xFFFFFFFFU)
			return get_polls(i);
		else
			return 0;
	}
/**
 * Read the actual poll count on specified control.
 *
 * Parameter idx: The control index.
 * Return: The poll count.
 */
	uint32_t get_polls(unsigned idx) throw();
/**
 * Increment poll count on specified control.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter ctrl: The control id.
 * Return: The poll count pre-increment.
 */
	uint32_t increment_polls(unsigned port, unsigned controller, unsigned ctrl) throw()
	{
		unsigned i = types->triple_to_index(port, controller, ctrl);
		if(i != 0xFFFFFFFFU)
			return increment_polls(i);
		else
			return 0;
	}
/**
 * Increment poll count on specified index.
 *
 * Parameter idx: The index.
 * Return: The poll count pre-increment.
 */
	uint32_t increment_polls(unsigned idx) throw();
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
	uint32_t* ctrs;
	const port_type_set* types;
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
 * Parameter p: Types of ports.
 */
	controller_frame(const port_type_set& p) throw(std::runtime_error);
/**
 * Create subframe of controls with specified controller types and specified memory.
 *
 * Parameter memory: The backing memory.
 * Parameter p: Types of ports.
 *
 * Throws std::runtime_error: NULL memory.
 */
	controller_frame(unsigned char* memory, const port_type_set& p) throw(std::runtime_error);
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
	const port_type& get_port_type(unsigned port) throw()
	{
		return types->port_type(port);
	}
/**
 * Get port count.
 */
	unsigned get_port_count() throw()
	{
		return types->ports();
	}
/**
 * Get index count.
 */
	unsigned get_index_count() throw()
	{
		return types->indices();
	}
/**
 * Set types of ports.
 *
 * Parameter ptype: New port types.
 * Throws std::runtime_error: Memory is mapped.
 */
	void set_types(const port_type_set& ptype) throw(std::runtime_error)
	{
		if(memory != backing)
			throw std::runtime_error("Can't change type of mapped controller_frame");
		types = &ptype;
	}
/**
 * Get blank dedicated frame of same port types.
 *
 * Return blank frame.
 */
	controller_frame blank_frame() throw()
	{
		return controller_frame(*types);
	}
/**
 * Check that types match.
 *
 * Parameter obj: Another object.
 * Returns: True if types match, false otherwise.
 */
	bool types_match(const controller_frame& obj) const throw()
	{
		return types == obj.types;
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
		if(types != another.types)
				throw std::runtime_error("controller_frame::operator^: Type mismatch");
		for(size_t i = 0; i < types->size(); i++)
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
 * Get size of frame.
 *
 * Returns: The number of bytes it takes to store frame of this type.
 */
	size_t size()
	{
		return types->size();
	}
/**
 * Set axis/button value.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter ctrl: The control id.
 * Parameter x: The new value.
 */
	void axis3(unsigned port, unsigned controller, unsigned ctrl, short x) throw()
	{
		if(port >= types->ports())
			return;
		types->port_type(port).write(backing + types->port_offset(port), controller, ctrl, x);
	}
/**
 * Set axis/button value.
 *
 * Parameter idx: Control index.
 * Parameter x: The new value.
 */
	void axis2(unsigned idx, short x) throw()
	{
		port_index_triple t = types->index_to_triple(idx);
		axis3(t.port, t.controller, t.control, x);
	}
/**
 * Get axis/button value.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter ctrl: The control id.
 * Return value: The axis value.
 */
	short axis3(unsigned port, unsigned controller, unsigned ctrl) throw()
	{
		if(port >= types->ports())
			return 0;
		return types->port_type(port).read(backing + types->port_offset(port), controller, ctrl);
	}

/**
 * Get axis/button value.
 *
 * Parameter idx: Index of control.
 * Return value: The axis value.
 */
	short axis2(unsigned idx) throw()
	{
		port_index_triple t = types->index_to_triple(idx);
		return axis3(t.port, t.controller, t.control);
	}
/**
 * Get controller display.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter buf: Buffer to write nul-terminated display to.
 */
	void display(unsigned port, unsigned controller, char* buf) throw()
	{
		if(port >= types->ports()) {
			*buf = '\0';
			return;
		}
		types->port_type(port).display(backing + types->port_offset(port), controller, buf);
	}
/**
 * Is device present?
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 * Returns: True if present, false if not.
 */
	bool is_present(unsigned port, unsigned controller) throw()
	{
		if(port >= types->ports())
			return false;
		return types->port_type(port).is_present(controller);
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
		for(size_t i = 0; i < types->ports(); i++) {
			size_t s;
			s = types->port_type(i).deserialize(backing + types->port_offset(i), buf + offset);
			if(s != DESERIALIZE_SPECIAL_BLANK) {
				offset += s;
				while(is_nonterminator(buf[offset]))
					offset++;
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
		for(size_t i = 0; i < types->ports(); i++)
			offset += types->port_type(i).serialize(backing + types->port_offset(i), buf + offset);
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
		return !memcmp(backing, obj.backing, types->size());
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
 * Parameter port: The port.
 * Parameter controller: The controller
 * Parameter lbid: Logical button id.
 * Returns: The physical button id, or -1 if no such button.
 */
	int button_id(unsigned port, unsigned controller, unsigned lbid)
	{
		if(port >= types->ports())
			return -1;
		return types->port_type(port).button_id(controller, lbid);
	}
/**
 * Does the specified controller have analog function.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 */
	bool is_analog(unsigned port, unsigned controller)
	{
		if(port >= types->ports())
			return false;
		return types->port_type(port).is_analog(controller);
	}
/**
 * Does the specified controller have mouse-type function.
 *
 * Parameter port: The port.
 * Parameter controller: The controller
 */
	bool is_mouse(unsigned port, unsigned controller)
	{
		if(port >= types->ports())
			return false;
		return types->port_type(port).is_mouse(controller);
	}
/**
 * Get the port type set.
 */
	const port_type_set& porttypes()
	{
		return *types;
	}
private:
	unsigned char memory[MAXIMUM_CONTROLLER_FRAME_SIZE];
	unsigned char* backing;
	const port_type_set* types;
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
	controller_frame_vector() throw();
/**
 * Construct new controller frame vector.
 *
 * Parameter p: The port types.
 */
	controller_frame_vector(const port_type_set& p) throw();
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
 * Parameter p: The port types.
 */
	void clear(const port_type_set& p) throw(std::runtime_error);
/**
 * Blank vector.
 */
	void clear() throw()
	{
		clear(*types);
	}
/**
 * Get number of subframes.
 */
	size_t size()
	{
		return frames;
	}
/**
 * Get the typeset.
 */
	const port_type_set& get_types()
	{
		return *types;
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
		return controller_frame(cache_page->content + pageoffset, *types);
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
		controller_frame c(*types);
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
	const port_type_set* types;
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
 * Get a dummy port type.
 *
 * Return value: Dummy port type.
 */
port_type& get_dummy_port_type() throw(std::bad_alloc);

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

/**
 * Generic port control index function.
 */
template<unsigned controllers, unsigned indices>
inline unsigned generic_used_indices(unsigned controller)
{
	if(controller >= controllers)
		return 0;
	return indices;
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
