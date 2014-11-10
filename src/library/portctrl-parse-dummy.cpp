#include "portctrl-parse-asmgen.hpp"
#include "assembler-intrinsics-dummy.hpp"

using namespace assembler_intrinsics;

namespace portctrl
{
namespace codegen
{
template<> void emit_serialize_prologue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_serialize_button(dummyarch& a, assembler::label_list& labels, int32_t offset, uint8_t mask,
	uint8_t ch)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_serialize_axis(dummyarch& a, assembler::label_list& labels, int32_t offset)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_serialize_pipe(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_serialize_epilogue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_prologue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_clear_storage(dummyarch& a, assembler::label_list& labels, int32_t size)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_button(dummyarch& a, assembler::label_list& labels, int32_t offset,
	uint8_t mask, assembler::label& next_pipe, assembler::label& end_deserialize)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_axis(dummyarch& a, assembler::label_list& labels, int32_t offset)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_skip_until_pipe(dummyarch& a, assembler::label_list& labels,
	assembler::label& next_pipe, assembler::label& deserialize_end)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_skip(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_special_blank(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_deserialize_epilogue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_read_prologue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_read_epilogue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_read_dispatch(dummyarch& a, assembler::label_list& labels,
	unsigned controllers, unsigned ilog2controls, assembler::label& end)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> assembler::label& emit_read_label(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_read_label_bad(dummyarch& a, assembler::label_list& labels, assembler::label& b)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_read_button(dummyarch& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset, uint8_t mask)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_read_axis(dummyarch& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_write_prologue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_write_epilogue(dummyarch& a, assembler::label_list& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_write_button(dummyarch& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset, uint8_t mask)
{
	throw std::runtime_error("ASM on this arch not supported");
}

template<> void emit_write_axis(dummyarch& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset)
{
	throw std::runtime_error("ASM on this arch not supported");
}
}
}
