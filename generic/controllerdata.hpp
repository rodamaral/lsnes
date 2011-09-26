#ifndef _controllerdata__hpp__included__
#define _controllerdata__hpp__included__

#include <vector>
#include <stdexcept>

#define ENCODE_SPECIAL_NO_OUTPUT 0xFFFFFFFFU

/**
 * What version to write as control version?
 */
#define WRITE_CONTROL_VERSION 0
/**
 * System control: Frame sync flag
 */
#define CONTROL_FRAME_SYNC 0
/**
 * System control: System reset button
 */
#define CONTROL_SYSTEM_RESET 1
/**
 * High part of cycle count for system reset (multiplier 10000).
 */
#define CONTROL_SYSTEM_RESET_CYCLES_HI 2
/**
 * Low part of cycle count for system reset (multiplier 1).
 */
#define CONTROL_SYSTEM_RESET_CYCLES_LO 3
/**
 * Number of system controls.
 */
#define MAX_SYSTEM_CONTROLS 4
/**
 * SNES has 2 controller ports.
 */
#define MAX_PORTS 2
/**
 * Multitap can connect 4 controllers to a single port.
 */
#define MAX_CONTROLLERS_PER_PORT 4
/**
 * Ordinary gamepad has 12 buttons/axis total (more than anything else supported).
 */
#define CONTROLLER_CONTROLS 12
/**
 * The total number of controls (currently 100).
 */
#define TOTAL_CONTROLS (MAX_SYSTEM_CONTROLS + MAX_PORTS * CONTROLLER_CONTROLS * MAX_CONTROLLERS_PER_PORT)

struct controls_t;

/**
 * Decoders
 */
class cdecode
{
public:
/**
 * This is type of functions that perform decoding of port fields.
 *
 * parameter port: The number of port to decode.
 * parameter line: The line to decode from.
 * parameter pos: Position on the line to start from.
 * parameter controls: Buffer to place the read controls to.
 * returns: End of fields (end of string or on '|') or start of next field (otherwise).
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Bad input.
 */
	typedef size_t (*fn_t)(unsigned port, const std::string& line, size_t pos, short* controls);

/**
 * This is a decoder for the system field. Note that this is not compatible with fn_t as parameters are diffrent.
 *
 * parameter port: The number of port to decode.
 * parameter line: The line to decode from.
 * parameter pos: Position on the line to start from.
 * parameter controls: Buffer to place the read controls to.
 * parameter version: The version of control structure to read.
 * returns: End of fields or start of next field.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Bad input.
 */
	static size_t system(const std::string& line, size_t pos, short* controls, unsigned version)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type none (see fn_t).
 */
	static size_t none(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type gamepad (see fn_t).
 */
	static size_t gamepad(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type multitap (see fn_t).
 */
	static size_t multitap(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type mouse (see fn_t).
 */
	static size_t mouse(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type superscope (see fn_t).
 */
	static size_t superscope(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type justifier (see fn_t).
 */
	static size_t justifier(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);

/**
 * This is a port decoder for port type justifiers (see fn_t).
 */
	static size_t justifiers(unsigned port, const std::string& line, size_t pos, short* controls)
		throw(std::bad_alloc, std::runtime_error);
};

/**
 * Encoders
 */
class cencode
{
public:
/**
 * This is the type of functions that perform encoding of port fields.
 *
 * parameter port: To number of port to encode.
 * parameter buffer: Buffer to store the encoded data to.
 * parameter pos: Position to start writing to.
 * parameter controls: Buffer to read the controls from.
 * returns: Position after written data, or ENCODE_SPECIAL_NO_OUTPUT if one wants no output, suppressing even the
 *	field terminator.
 * throws std::bad_alloc: Not enough memory.
 */
	typedef size_t (*fn_t)(unsigned port, char* buffer, size_t pos, const short* controls);

/**
 * This is encoder for the system field. Note that the parameters are bit diffrent and this can't be put into fn_t.
 *
 * parameter buffer: Buffer to store the encoded data to.
 * parameter pos: Position to start writing to.
 * parameter controls: Buffer to read the controls from.
 * returns: Position after written data, or ENCODE_SPECIAL_NO_OUTPUT if one wants no output, suppressing even the
 *	field terminator.
 * throws std::bad_alloc: Not enough memory.
 */
	static size_t system(char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type none. See fn_t.
 */
	static size_t none(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type gamepad. See fn_t.
 */
	static size_t gamepad(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type multitap. See fn_t.
 */
	static size_t multitap(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type mouse. See fn_t.
 */
	static size_t mouse(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type superscope. See fn_t.
 */
	static size_t superscope(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type justifier. See fn_t.
 */
	static size_t justifier(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);

/**
 * This is a port encoder for port type justifiers. See fn_t.
 */
	static size_t justifiers(unsigned port, char* buffer, size_t pos, const short* controls) throw(std::bad_alloc);
};

/**
 * This structure holds controls for single (sub)frame or instant of time.
 */
struct controls_t
{
/**
 * Creates new controls structure. All buttons are released and all axes are 0 (neutral).
 *
 * parameter sync: If true, write 1 (pressed) to frame sync subfield, else write 0 (released). Default false.
 */
	explicit controls_t(bool sync = false) throw();

/**
 * This constructor takes in a line of input, port decoders and system field version and decodes the controls.
 *
 * parameter line: The line to decode.
 * parameter decoders: The decoders for each port.
 * parameter version: Version for the system field.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid input line.
 */
	controls_t(const std::string& line, const std::vector<cdecode::fn_t>& decoders, unsigned version)
		throw(std::bad_alloc, std::runtime_error);
/**
 * This method takes in port encoders and encodes the controls.
 *
 * parameter encoders: The encoders for each port.
 * returns: The encoded controls.
 * throws std::bad_alloc: Not enough memory.
 */
	std::string tostring(const std::vector<cencode::fn_t>& encoders) const throw(std::bad_alloc);

/**
 * This method takes in controller (port, controller, control) tuple and returns reference to the value of that
 * control.
 *
 * parameter port: The port number
 * parameter controller: The controller number within that port.
 * parameter control: The control number within that controller.
 * returns: Reference to control value.
 * throws std::logic_error: port, controller or control is invalid.
 */
	const short& operator()(unsigned port, unsigned controller, unsigned control) const throw(std::logic_error);

/**
 * This method takes in control index and returns reference to the value of that control.
 *
 * parameter control: The control index number.
 * returns: Reference to control value.
 * throws std::logic_error: control index is invalid.
 */
	const short& operator()(unsigned control) const throw(std::logic_error);

/**
 * This method takes in controller (port, controller, control) tuple and returns reference to the value of that
 * control.
 *
 * parameter port: The port number
 * parameter controller: The controller number within that port.
 * parameter control: The control number within that controller.
 * returns: Reference to control value.
 * throws std::logic_error: port, controller or control is invalid.
 */
	short& operator()(unsigned port, unsigned controller, unsigned control) throw(std::logic_error);

/**
 * This method takes in control index and returns reference to the value of that control.
 *
 * parameter control: The control index number.
 * returns: Reference to control value.
 * throws std::logic_error: control index is invalid.
 */
	short& operator()(unsigned control) throw(std::logic_error);

/**
 * Perform XOR per-control.
 *
 * parameter other: The othe field to XOR with.
 * returns: The XOR result.
 */
	controls_t operator^(controls_t other) throw();

/**
 * This field contains the raw controller data. Avoid manipulating directly.
 */
	short controls[TOTAL_CONTROLS];

/**
 * Equality
 */
	bool operator==(const controls_t& c) const throw();
};

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
 * Information about port type.
 */
struct port_type
{
/**
 * Name of type.
 */
	const char* name;
/**
 * Decoder function.
 */
	cdecode::fn_t decoder;
/**
 * Encoder function.
 */
	cencode::fn_t encoder;
/**
 *  Port type value.
 */
	porttype_t ptype;
/**
 *  Number of devices.
 */
	unsigned devices;
/**
 *  Type of each connected device.
 */
	devicetype_t dtype;
/**
 * True if valid for port1&2, false if valid for only for port 2.
 */
	bool valid_port1;
/**
 * BSNES controller type ID.
 */
	unsigned bsnes_type;
/**
 * Lookup port type by name.
 *
 * parameter name: Name of the port type to look up.
 * parameter port2: True if controller is for port 2, false if for port 1.
 * returns: The port type structure
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid port type.
 */
	static const port_type& lookup(const std::string& name, bool port2 = true) throw(std::bad_alloc,
		std::runtime_error);
};


/**
 * Information about port types, index by port type value (porttype_t).
 */
extern port_type port_types[];

/**
 * This method takes in controller (port, controller, control) tuple and returns the system index corresponding to
 * that control.
 *
 * parameter port: The port number
 * parameter controller: The controller number within that port.
 * parameter control: The control number within that controller.
 * returns: The control index.
 * throws std::logic_error: port, controller or control is invalid.
 */
unsigned ccindex2(unsigned port, unsigned controller, unsigned control) throw(std::logic_error);

#endif
