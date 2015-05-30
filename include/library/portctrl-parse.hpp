#ifndef _library__portctrl_parse__hpp__included__
#define _library__portctrl_parse__hpp__included__

#include "assembler.hpp"

namespace JSON
{
	class node;
}

namespace portctrl
{
class controller_set;
class type;

struct controller_set* pcs_from_json(const JSON::node& root, const text& ptr);
std::vector<controller_set*> pcs_from_json_array(const JSON::node& root, const text& ptr);
text pcs_write_class(const struct controller_set& pset, unsigned& tmp_idx);
text pcs_write_trailer(const std::vector<controller_set*>& p);
text pcs_write_classes(const std::vector<controller_set*>& p, unsigned& tmp_idx);

struct type_generic : public type
{
	type_generic(const JSON::node& root, const text& ptr) throw(std::exception);
	~type_generic() throw();
	struct ser_instruction
	{
		int type;
		void* djumpvector;
		void* ejumpvector;
		size_t offset;
		unsigned char mask;
		char character;
	};
	struct idxinfo
	{
		int type;
		size_t offset;
		unsigned char mask;
		unsigned char imask;
		int controller;
		int index;
	};
private:
	std::vector<size_t> indexbase;
	std::vector<idxinfo> indexinfo;
	void* dyncode_block;
	mutable std::vector<ser_instruction> serialize_instructions;
	text port_iname(const JSON::node& root, const text& ptr);
	text port_hname(const JSON::node& root, const text& ptr);
	size_t port_size(const JSON::node& root, const text& ptr);
	static void _write(const type* _this, unsigned char* buffer, unsigned idx, unsigned ctrl, short x);
	static short _read(const type* _this, const unsigned char* buffer, unsigned idx, unsigned ctrl);
	static size_t _serialize(const type* _this, const unsigned char* buffer, char* textbuf);
	static size_t _deserialize(const type* _this, unsigned char* buffer, const char* textbuf);
	void make_dynamic_blocks();
	void make_routines(assembler::assembler& a, assembler::label_list& labels);
};
}

#endif
