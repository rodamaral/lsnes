#ifndef _library__controller_parse__hpp__included__
#define _library__controller_parse__hpp__included__

#include "assembler.hpp"

class port_controller_set;
class port_type;

namespace JSON
{
	class node;
}

struct port_controller_set* pcs_from_json(const JSON::node& root, const std::string& ptr);
std::vector<port_controller_set*> pcs_from_json_array(const JSON::node& root, const std::string& ptr);
std::string pcs_write_class(const struct port_controller_set& pset, unsigned& tmp_idx);
std::string pcs_write_trailer(const std::vector<port_controller_set*>& p);
std::string pcs_write_classes(const std::vector<port_controller_set*>& p, unsigned& tmp_idx);

struct port_type_generic : public port_type
{
	port_type_generic(const JSON::node& root, const std::string& ptr) throw(std::exception);
	~port_type_generic() throw();
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
	std::string port_iname(const JSON::node& root, const std::string& ptr);
	std::string port_hname(const JSON::node& root, const std::string& ptr);
	size_t port_size(const JSON::node& root, const std::string& ptr);
	static void _write(const port_type* _this, unsigned char* buffer, unsigned idx, unsigned ctrl, short x);
	static short _read(const port_type* _this, const unsigned char* buffer, unsigned idx, unsigned ctrl);
	static size_t _serialize(const port_type* _this, const unsigned char* buffer, char* textbuf);
	static size_t _deserialize(const port_type* _this, unsigned char* buffer, const char* textbuf);
	void make_dynamic_blocks();
	void make_routines(assembler::assembler& a, std::list<assembler::label>& labels);
};

#endif
