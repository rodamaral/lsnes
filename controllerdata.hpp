#ifndef _controllerdata__hpp__included__
#define _controllerdata__hpp__included__

#include "fieldsplit.hpp"
#include <vector>
#include <stdexcept>

/**
 * \brief What version to write as control version?
 */
#define WRITE_CONTROL_VERSION 0
/**
 * \brief System control: Frame sync flag
 */
#define CONTROL_FRAME_SYNC 0
/**
 * \brief System control: System reset button
 */
#define CONTROL_SYSTEM_RESET 1
/**
 * \brief High part of cycle count for system reset (multiplier 10000).
 */
#define CONTROL_SYSTEM_RESET_CYCLES_HI 2
/**
 * \brief Low part of cycle count for system reset (multiplier 1).
 */
#define CONTROL_SYSTEM_RESET_CYCLES_LO 3
/**
 * \brief Number of system controls.
 */
#define MAX_SYSTEM_CONTROLS 4
/**
 * \brief SNES has 2 controller ports.
 */
#define MAX_PORTS 2
/**
 * \brief Multitap can connect 4 controllers to a single port.
 */
#define MAX_CONTROLLERS_PER_PORT 4
/**
 * \brief Ordinary gamepad has 12 buttons/axis total (more than anything else supported).
 */
#define CONTROLLER_CONTROLS 12
/**
 * \brief The total number of controls (currently 100).
 */
#define TOTAL_CONTROLS (MAX_SYSTEM_CONTROLS + MAX_PORTS * CONTROLLER_CONTROLS * MAX_CONTROLLERS_PER_PORT)

struct controls_t;


/**
 * \brief Subfield information
 *
 * Information about single subfield in control field. Subfields can be either single buttons or single axes.
 */
struct control_subfield
{
/**
 * \brief Index used for subfield in control field
 *
 * This is the index this subfield is accessed as in operator[] method of class controlfield.
 */
	unsigned index;

/**
 * \brief Type of subfield
 *
 * This type gives the type of the subfield, if it is a single button or single axis.
 */
	enum control_subfield_type {
/**
 * \brief The subfield is a button.
 */
		BUTTON,

/**
 * \brief The subfield is an axis.
 */
		AXIS
	};

/**
 * \brief Type of subfield
 *
 * This gives the type of the subfield.
 */
	enum control_subfield_type type;

/**
 * \brief Make subfield structure
 *
 * Make subfield structure having specified index and type.
 *
 * \param _index The index to give to the subfield.
 * \param _type The type to to give to the subfield.
 */
	control_subfield(unsigned _index, enum control_subfield_type _type) throw();
};

/**
 * \brief Field parser
 *
 * This class handles parsing fields consisting of subfields.
 */
class controlfield
{
public:
/**
 * \brief Create new field parser
 *
 * This constructor creates new field parser parsing field consisting of specified subfields.
 *
 * \param _subfields The subfields forming the field to parse.
 * \throws std::bad_alloc Not enough memory to complete the operation.
 */
	controlfield(std::vector<control_subfield> _subfields) throw(std::bad_alloc);

/**
 * \brief Update values of subfields
 *
 * Read in string representation of a field and update values of all subfields.
 *
 * \param str The string representation
 * \throws std::bad_alloc Not enough memory to complete the operation.
 */
	void set_field(const std::string& str) throw(std::bad_alloc);

/**
 * \brief Read data in specified subfield.
 *
 * This method takes in a subfield index and gives the current data for said subfield.
 *
 * \param index The subfield index to read (0-based indexing).
 * \throws std::logic_error Trying to access subfield index out of range
 * \return The current value for that subfield
 * \note If subfield index is in range but not mapped to any subfield, then 0 is returned.
 */
	short operator[](unsigned index) throw(std::logic_error);

/**
 * \brief Get number of valid subfield indices
 *
 * Obtain number of valid subfield indices. The indices are [0, N), where N is the return value of this method.
 *
 * \return The amount of accessable subfield indices.
 */
	unsigned indices() throw();
private:
	std::vector<control_subfield> subfields;
	std::vector<short> values;
};

/**
 * \brief System control field parser
 *
 * Subtype of controlfield (field parser) with fixed subfields of system field.
 */
class controlfield_system : public controlfield
{
public:
/**
 * \brief Create new field parser
 *
 * This creates new parser for system fields.
 *
 * \param version The version to parse, currently must be 0.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Unknown version.
 */
	controlfield_system(unsigned version) throw(std::bad_alloc, std::runtime_error);
};

/**
 * \brief Gamepad control field parser
 *
 * Subtype of controlfield (field parser) with fixed subfields of gamepad controller (12 buttons).
 */
class controlfield_gamepad : public controlfield
{
public:
/**
 * \brief Create new field parser
 *
 * This creates new parser for gamepad fields.
 *
 * \throws std::bad_alloc Not enough memory.
 */
	controlfield_gamepad() throw(std::bad_alloc);
};

/**
 * \brief Mouse/Justifier control field parser
 *
 * Subtype of controlfield (field parser) with fixed subfields of mouse or justfier controller (2 axes, 2 buttons).
 */
class controlfield_mousejustifier : public controlfield
{
public:
/**
 * \brief Create new field parser
 *
 * This creates new parser for mouse or justifier fields.
 *
 * \throws std::bad_alloc Not enough memory.
 */
	controlfield_mousejustifier() throw(std::bad_alloc);
};

/**
 * \brief Superscope control field parser
 *
 * Subtype of controlfield (field parser) with fixed subfields of superscope controller (2 axes, 4 buttons).
 */
class controlfield_superscope : public controlfield
{
public:
/**
 * \brief Create new field parser
 *
 * This creates new parser for superscope fields.
 *
 * \throws std::bad_alloc Not enough memory.
 */
	controlfield_superscope() throw(std::bad_alloc);
};

/**
 * \brief Decoders
 */
class cdecode
{
public:
/**
 * \brief Port decoder type
 *
 * This is type of functions that perform decoding of port fields.
 *
 * \param port The number of port to decode.
 * \param line Field splitter, positioned so the first field of that port is read first.
 * \param controls Buffer to place the read controls to.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Bad input.
 * \note This method needs to read all fields associated with the port.
 */
	typedef void (*fn_t)(unsigned port, fieldsplitter& line, short* controls);

/**
 * \brief System field decoder
 *
 * This is a decoder for the system field.
 *
 * \param line The line to decode from. Assumed to be positioned on the system field.
 * \param controls The buffer for controls read.
 * \param version The version of control structure to read.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Invalid field or invalid version.
 * \note This can't be put into decoder_type as parameters are bit diffrent.
 */
	static void system(fieldsplitter& line, short* controls, unsigned version) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type none.
 *
 * This is a port decoder for port type none.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Can't happen.
 * \throws std::runtime_error Can't happen.
 */
	static void none(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type gamepad.
 *
 * This is a port decoder for port type gamepad.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Invalid controller data.
 */
	static void gamepad(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type multitap.
 *
 * This is a port decoder for port type multitap.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Invalid controller data.
 */
	static void multitap(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type mouse.
 *
 * This is a port decoder for port type mouse.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Invalid controller data.
 */
	static void mouse(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type superscope.
 *
 * This is a port decoder for port type superscope.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Invalid controller data.
 */
	static void superscope(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type justifier.
 *
 * This is a port decoder for port type justifier.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Invalid controller data.
 */
	static void justifier(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Port decoder for type justifiers.
 *
 * This is a port decoder for port type justifiers.
 *
 * \param port The port number
 * \param line The line to decode. Assumed to be positioned on start of port data.
 * \param controls Buffer to decode the controls to.
 * \throws std::bad_alloc Out of memory.
 * \throws std::runtime_error Invalid controller data.
 */
	static void justifiers(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc,
		std::runtime_error);
};

/**
 * \brief Decoders
 */
class cencode
{
public:
/**
 * \brief Port encoder type
 *
 * This is the type of functions that perform encoding of port fields.
 *
 * \param port To number of port to encode.
 * \param controls Buffer to read the controls from.
 * \throws std::bad_alloc Not enough memory.
 */
	typedef std::string (*fn_t)(unsigned port, const short* controls);

/**
 * \brief Encoder for system field
 *
 * This is encoder for the system field.
 *
 * \param controls The controls to encode.
 * \throws std::bad_alloc Out of memory.
 * \note This can't be put into encoder_type as parameters are bit diffrent.
 */
	static std::string system(const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type none.
 *
 * This is a port encoder for port type none.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string none(unsigned port, const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type gamepad.
 *
 * This is a port encoder for port type gamepad.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string gamepad(unsigned port, const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type multitap.
 *
 * This is a port encoder for port type multitap.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string multitap(unsigned port, const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type mouse.
 *
 * This is a port encoder for port type mouse.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string mouse(unsigned port, const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type superscope.
 *
 * This is a port encoder for port type superscope.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string superscope(unsigned port, const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type justifier.
 *
 * This is a port encoder for port type justifier.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string justifier(unsigned port, const short* controls) throw(std::bad_alloc);

/**
 * \brief Port encoder for type justifiers.
 *
 * This is a port encoder for port type justifiers.
 *
 * \param port The port number
 * \param controls The controls to encode.
 * \return The encoded fields.
 * \throws std::bad_alloc Out of memory.
 */
	static std::string justifiers(unsigned port, const short* controls) throw(std::bad_alloc);
};

/**
 * \brief Controls for single (sub)frame
 *
 * This structure holds controls for single (sub)frame or instant of time.
 */
struct controls_t
{
/**
 * \brief Create new controls structure
 *
 * Creates new controls structure. All buttons are released and all axes are 0 (neutral).
 *
 * \param sync If true, write 1 (pressed) to frame sync subfield, else write 0 (released). Default false.
 */
	controls_t(bool sync = false) throw();

/**
 * \brief Read in a line using specified decoders
 *
 * This constructor takes in a line of input, port decoders and system field version and decodes the controls.
 *
 * \param line The line to decode.
 * \param decoders The decoders for each port.
 * \param version Version for the system field.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Invalid input line.
 */
	controls_t(const std::string& line, const std::vector<cdecode::fn_t>& decoders, unsigned version)
		throw(std::bad_alloc, std::runtime_error);
/**
 * \brief Encode line
 *
 * This method takes in port encoders and encodes the controls.
 *
 * \param encoders The encoders for each port.
 * \throws std::bad_alloc Not enough memory.
 */
	std::string tostring(const std::vector<cencode::fn_t>& encoders) const throw(std::bad_alloc);

/**
 * \brief Read control value by (port, controller, control) tuple.
 *
 * This method takes in controller (port, controller, control) tuple and returns reference to the value of that
 * control.
 *
 * \param port The port number
 * \param controller The controller number within that port.
 * \param control The control number within that controller.
 * \return Reference to control value.
 * \throws std::logic_error port, controller or control is invalid.
 */
	const short& operator()(unsigned port, unsigned controller, unsigned control) const throw(std::logic_error);

/**
 * \brief Read control value by system index.
 *
 * This method takes in system index and returns reference to the value of that control.
 *
 * \param control The system index of control.
 * \return Reference to control value.
 * \throws std::logic_error Invalid control index.
 */
	const short& operator()(unsigned control) const throw(std::logic_error);

/**
 * \brief Read control value by (port, controller, control) tuple.
 *
 * This method takes in controller (port, controller, control) tuple and returns reference to the value of that
 * control.
 *
 * \param port The port number
 * \param controller The controller number within that port.
 * \param control The control number within that controller.
 * \return Reference to control value.
 * \throws std::logic_error port, controller or control is invalid.
 */
	short& operator()(unsigned port, unsigned controller, unsigned control) throw(std::logic_error);

/**
 * \brief Read control value by system index.
 *
 * This method takes in system index and returns reference to the value of that control.
 *
 * \param control The system index of control.
 * \return Reference to control value.
 * \throws std::logic_error Invalid control index.
 */
	short& operator()(unsigned control) throw(std::logic_error);

	controls_t operator^(controls_t other) throw();

/**
 * \brief Raw controller data
 *
 * This field contains the raw controller data. Avoid manipulating directly.
 */
	short controls[TOTAL_CONTROLS];

/**
 * \brief Equality
 */
	bool operator==(const controls_t& c) const throw();
};

/**
 * \brief Type of port
 *
 * This enumeration gives the type of port.
 */
enum porttype_t
{
/**
 * \brief No device
 */
	PT_NONE = 0,			//Nothing connected to port.
/**
 * \brief Gamepad
 */
	PT_GAMEPAD = 1,
/**
 * \brief Multitap (with 4 gamepads connected)
 */
	PT_MULTITAP = 2,
/**
 * \brief Mouse
 */
	PT_MOUSE = 3,
/**
 * \brief Superscope (only allowed for port 2).
 */
	PT_SUPERSCOPE = 4,
/**
 * \brief Justifier (only allowed for port 2).
 */
	PT_JUSTIFIER = 5,
/**
 * \brief 2 Justifiers (only allowed for port 2).
 */
	PT_JUSTIFIERS = 6,
/**
 * \brief Number of controller types.
 */
	PT_LAST_CTYPE = 6,
/**
 * \brief Invalid controller type.
 */
	PT_INVALID = 7
};

/**
 * \brief Type of port
 *
 * This enumeration gives the type of device.
 */
enum devicetype_t
{
/**
 * \brief No device
 */
	DT_NONE = 0,
/**
 * \brief Gamepad
 */
	DT_GAMEPAD = 1,
/**
 * \brief Mouse
 */
	DT_MOUSE = 3,
/**
 * \brief Superscope
 */
	DT_SUPERSCOPE = 4,
/**
 * \brief Justifier.
 */
	DT_JUSTIFIER = 5
};

/**
 * \brief Information about port type.
 */
struct port_type
{
	const char* name;
	cdecode::fn_t decoder;
	cencode::fn_t encoder;
	porttype_t ptype;
	unsigned devices;
	devicetype_t dtype;
	bool valid_port1;
	static const port_type& lookup(const std::string& name, bool port2 = true) throw(std::bad_alloc,
		std::runtime_error);
};


/**
 * \brief Information about port types.
 */
extern port_type port_types[];

/**
 * \brief Translate controller index to system index.
 *
 * This method takes in controller (port, controller, control) tuple and returns the system index corresponding to
 * that control.
 *
 * \param port The port number
 * \param controller The controller number within that port.
 * \param control The control number within that controller.
 * \return System control index.
 * \throws std::logic_error port, controller or control is invalid.
 */
unsigned ccindex2(unsigned port, unsigned controller, unsigned control) throw(std::logic_error);

#endif
