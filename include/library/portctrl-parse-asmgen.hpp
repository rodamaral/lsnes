#ifndef _library__portctrl_parse_asmgen__hpp__included__
#define _library__portctrl_parse_asmgen__hpp__included__

#include "assembler.hpp"

namespace portctrl
{
namespace codegen
{
//Emit function prologue for serialization function. This also Initializes the read position to start of
//serialization buffer.
//
//The serialization function takes three parameters. In order:
//	- Dummy pointer (to be ignored).
//	- Readonly controller state (pointer).
//	- serialization output buffer (pointer).
//
template<class T> void emit_serialize_prologue(T& a, assembler::label_list& labels);

//Emit code to serialize a button.
//
//The button is emitted to current write position as specified character if set, '.' if not set. Advances write
//position by one byte.
//
//Parameters:
//	- offset: The byte in controller state the button bit is in (0-based).
//	- mask: Bitmask for the button bit.
//	- ch: The character to use for writing pressed button.
//
template<class T> void emit_serialize_button(T& a, assembler::label_list& labels, int32_t offset, uint8_t mask,
	uint8_t ch);

//Emit code to serialize an axis.
//
//Call write_axis_value() in order to serialize the axis. Advance write position by indicated number of bytes.
//
//Parameters:
//	- offset: The low byte of axis in controller state (the high byte is at <offset>+1). This field is signed.
//
template<class T> void emit_serialize_axis(T& a, assembler::label_list& labels, int32_t offset);

//Emit code to write a pipe sign ('|').
//
//Emit a '|' to current write position. Advance write position by one byte.
//
template<class T> void emit_serialize_pipe(T& a, assembler::label_list& labels);

//Emit function epilogue for serialization function.
//
//Serialization function returns the number of bytes in serialized output.  The return type is size_t.
//
template<class T> void emit_serialize_epilogue(T& a, assembler::label_list& labels);

//Emit function prologue for deserialization function. This also initializes the read position to start of
//serialization buffer.
//
//The serialization function takes three parameters. In order:
//	- Dummy pointer (to be ignored).
//	- controller state to be modified (pointer).
//	- readonly serialization input buffer (pointer).
//
template<class T> void emit_deserialize_prologue(T& a, assembler::label_list& labels);

//Emit code to clear the controller state.
//
//Parameters:
//	- size: The number of bytes in controller state.
template<class T> void emit_deserialize_clear_storage(T& a, assembler::label_list& labels, int32_t size);

//Emit code to deserialize button.
//
//- If the current read position has '|', jump to <next_pipe> without advancing or modifying controller state.
//- If the current read position has '\r', \n' or '\0', jump to <end_deserialize> without advancing or modifying
//  controller state.
//- If the current read position has anything except '.' or ' ', set the corresponding bit in controller state.
//- If the first two cases do not apply, advance read position by one byte.
//
// Parameters:
//	- offset: The byte offset in controller state the button bit is in (0-based).
//	- mask: Bitmask for the button bit.
//	- next_pipe: Label to jump to when finding '|'.
//	- end_deserialize: Label to jump to when finding end of input.
//
template<class T> void emit_deserialize_button(T& a, assembler::label_list& labels, int32_t offset,
	uint8_t mask, assembler::label& next_pipe, assembler::label& end_deserialize);

//Emit code to deserialize axis.
//
//Call read_axis_value() in order to deserialize the value. Advance read position by indicated number of bytes.
//
// Parameters:
//	- offset: The low byte of axis in controller state (the high byte is at <offset>+1). This field is signed.
//
template<class T> void emit_deserialize_axis(T& a, assembler::label_list& labels, int32_t offset);

//Emit code to skip until next pipe ('|')
//
//While current read position does not contain '|', '\r', '\n' nor '\0', advance current read position.
//If '|' is encountered, jump to <next_pipe>. If '\r', '\n' or '\0' is encountered, jump to <deserialize_end>.
//
// Parameters:
//	- next_pipe: Address to jump to if '|' is found.
//	- deserialize_end: Address to jump to if '\r', '\n' or '\0' is found.
//
template<class T> void emit_deserialize_skip_until_pipe(T& a, assembler::label_list& labels,
	assembler::label& next_pipe, assembler::label& deserialize_end);

//Emit code to advance read position by one byte.
//
//Advance the read position by one byte. This is used to skip '|' bytes.
//
template<class T> void emit_deserialize_skip(T& a, assembler::label_list& labels);

//Emit code to arrange deserialization function to return DESERIALIZE_SPECIAL_BLANK instead of byte count.
//
//This changes the return value of deserialization function to DESERIALIZE_SPECIAL_BLANK.
//
template<class T> void emit_deserialize_special_blank(T& a, assembler::label_list& labels);

//Emit function epilogue for serialization function.
//
//Deserialization function returns the number of bytes in serialized input (or the special value
//DESERIALIZE_SPECIAL_BLANK). The return type is size_t.
//
template<class T> void emit_deserialize_epilogue(T& a, assembler::label_list& labels);

//Emit function prologue for Read function.
//
//The read function takes four parameters. In order:
//	- Dummy pointer (to be ignored).
//	- controller state to be queried.
//	- controller number (unsigned)
//	- control index number (unsigned)
//
//Initialze pending return value to 0.
//
template<class T> void emit_read_prologue(T& a, assembler::label_list& labels);

//Emit function epilogue for read function.
//
//The read function returns the pending return value (short):
//	- 0 => Invalid control, or released button.
//	- 1 => Pressed button.
//	- (other) => Axis value.
//
template<class T> void emit_read_epilogue(T& a, assembler::label_list& labels);

//Emit dispatch code for read/write function.
//
//Read dispatch table immediately after the code generated by this and jump to obtained address. The table layout
//is controls from low to high number, controllers from low to high number. Each controller has 2^<ilog2controls>
//entries.
//
// Parameters:
//	- controllers: Number of controllers.
//	- ilog2controls: Two's logarithm of number of controls per controller (rounded up).
//	- end: Label pointing to code causing read/write function to return.
//
template<class T> void emit_read_dispatch(T& a, assembler::label_list& labels,
	unsigned controllers, unsigned ilog2controls, assembler::label& end);

//Emit a jump pointer.
//
//Emits a jump pointer read by emit_read_dispatch() and returns reference to the pointer target.
//
// Return value:
//	- The newly created label to code this jumps to.
//
template<class T> assembler::label& emit_read_label(T& a, assembler::label_list& labels);

//Emit a specfied jump pointer.
//
//Emits a jump pointer read by emit_read_dispatch() pointing to specified label.
//
// Parameters:
//	- b: The code to jump to.
//
template<class T> void emit_read_label_bad(T& a, assembler::label_list& labels, assembler::label& b);

//Emit code to read button value.
//
//If the specified bit is set, set pending return value to 1. Then jump to <end>.
//
// Parameters:
//	- l: Label for the code fragment itself (this needs to be defined).
//	- end: Code to return from the read function.
//	- offset: Byte offset within controller state (0-based).
//	- mask: Bitmask for the button bit.
//
template<class T> void emit_read_button(T& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset, uint8_t mask);

//Emit code to read axis value.
//
//Set pending return value to value in specified location. Then jump to <end>.
//
// Parameters:
//	- l: Label for the code fragment itself (this needs to be defined).
//	- end: Code to return from the read function.
//	- offset: Low byte offset in controller state (high byte is at <offset>+1). Signed.
//
template<class T> void emit_read_axis(T& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset);

//Emit function prologue for Write function.
//
//The write function takes five parameters. In order:
//	- Dummy pointer (to be ignored).
//	- controller state to be queried.
//	- controller number (unsigned)
//	- control index number (unsigned)
//	- value to write (short).
//
template<class T> void emit_write_prologue(T& a, assembler::label_list& labels);

//Emit epilogue for write function.
//
//Write function does not return anything.
//
template<class T> void emit_write_epilogue(T& a, assembler::label_list& labels);

//Emit code to write button value.
//
//If the value to write is nonzero, set specified bit to 1, otherwise set it to 0. Then jump to <end>.
//
// Parameters:
//	- l: Label for the code fragment itself (this needs to be defined).
//	- end: Code to return from the write function.
//	- offset: Byte offset within controller state (0-based).
//	- mask: Bitmask for the button bit.
//
template<class T> void emit_write_button(T& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset, uint8_t mask);

//Emit code to write axis value.
//
//Write the value to write to specified location. Then jump to <end>.
//
// Parameters:
//	- l: Label for the code fragment itself (this needs to be defined).
//	- end: Code to return from the write function.
//	- offset: Low byte offset in controller state (high byte is at <offset>+1). Signed.
//
template<class T> void emit_write_axis(T& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset);
}
}

#endif
