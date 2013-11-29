#ifndef _library__json__hpp__included__
#define _library__json__hpp__included__

#include <cstdint>
#include <string>
#include <list>
#include <vector>
#include <stdexcept>
#include <map>
#include "utf8.hpp"

namespace JSON
{
class node;

struct number_tag
{
	const static int id = 2;
	node operator()(double v) const;
	node operator()(uint64_t v) const;
	node operator()(int64_t v) const;
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

struct string_tag
{
	const static int id = 3;
	node operator()(const std::string& s) const;
	node operator()(const std::u32string& s) const;
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

struct boolean_tag
{
	const static int id = 1;
	node operator()(bool v);
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

struct array_tag
{
	const static int id = 4;
	node operator()() const;
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

struct object_tag {
	const static int id = 5;
	node operator()() const;
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

struct null_tag
{
	const static int id = 0;
	node operator()() const;
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

struct none_tag
{
	const static int id = -1;
	bool operator==(const int& n) const { return n == id; }
	bool operator!=(const int& n) const { return !(*this == n); }
	operator int() { return id; }
};

extern number_tag number;
extern string_tag string;
extern boolean_tag boolean;
extern array_tag array;
extern object_tag object;
extern null_tag null;
extern none_tag none;

node i(int64_t n);
node u(uint64_t n);
node f(double n);
node b(bool bl);
node s(const std::string& st);
node s(const std::u32string& st);
node n();

enum errorcode
{
	ERR_OK = 0,			//All OK.
	ERR_NOT_A_NUMBER,		//Not a number in operation expecting one.
	ERR_NOT_A_STRING,		//Not a string in operation expecting one.
	ERR_NOT_A_BOOLEAN,		//Not a boolean in operation expecting one.
	ERR_NOT_AN_ARRAY,		//Not an array in operation expecting one.
	ERR_NOT_AN_OBJECT,		//Not an object in operation expecting one.
	ERR_NOT_ARRAY_NOR_OBJECT,	//Not array nor object in operation expecting either.
	ERR_INDEX_INVALID,		//Non-existent index accessed.
	ERR_KEY_INVALID,		//Non-existent key accessed.
	ERR_INSTANCE_INVALID,		//Non-existent instance (of key) accessed.
	ERR_POINTER_TRAILING_ESCAPE,	//JSON pointer has trailing escape.
	ERR_POINTER_INVALID_ESCAPE,	//JSON pointer has invalid escape.
	ERR_BAD_HEX,			//Bad hexadecimal character.
	ERR_INVALID_SURROGATE,		//Invalid surrogate escape sequence.
	ERR_INVALID_ESCAPE,		//Invalid escape sequence.
	ERR_TRUNCATED_STRING,		//Truncated string.
	ERR_UNKNOWN_TYPE,		//Unknown value type.
	ERR_GARBAGE_AFTER_END,		//Garbage after end of JSON.
	ERR_TRUNCATED_JSON,		//JSON truncated.
	ERR_UNEXPECTED_COMMA,		//Unexpected ','.
	ERR_UNEXPECTED_COLON,		//Unexpected ':'.
	ERR_UNEXPECTED_RIGHT_BRACE,	//Unexpected '}'.
	ERR_UNEXPECTED_RIGHT_BRACKET,	//Unexpected ']'.
	ERR_INVALID_NUMBER,		//Bad number syntax.
	ERR_EXPECTED_STRING_KEY,	//Object key is not a string.
	ERR_EXPECTED_COLON,		//Expected ':' here.
	ERR_EXPECTED_COMMA,		//Expected ',' here.
	ERR_UNKNOWN_CHARACTER,		//Unknown token type.
	ERR_POINTER_BAD_APPEND,		//Bad JSON pointer append.
	ERR_POINTER_BAD_INDEX,		//Bad JSON pointer index.
	ERR_ITERATOR_END,		//Iterator already at end.
	ERR_ITERATOR_DELETED,		//Iterator points to deleted object.
	ERR_WRONG_OBJECT,		//Iterator points to wrong object.
	ERR_ILLEGAL_CHARACTER,		//Illegal character generated by escape.
	ERR_CONTROL_CHARACTER,		//Control character in string.
	ERR_UNKNOWN_SUBTYPE,		//Unknown value subtype.
	ERR_PATCH_BAD,			//Bad JSON patch.
	ERR_PATCH_TEST_FAILED,		//TEST operation in patch failed.
	ERR_PATCH_ILLEGAL_MOVE,		//MOVE operation in patch illegal.
};

enum parsestate
{
	PARSE_NOT_PARSING = 0,		//Not parsing.
	PARSE_ARRAY_AFTER_VALUE,	//After value in array
	PARSE_END_OF_DOCUMENT,		//At end of document
	PARSE_OBJECT_AFTER_VALUE,	//After value in object
	PARSE_OBJECT_COLON,		//Expecting for colon in object
	PARSE_OBJECT_NAME,		//Expecting name in object
	PARSE_STRING_BODY,		//Parsing string body
	PARSE_STRING_ESCAPE,		//Parsing escape in string body
	PARSE_VALUE_START,		//Parsing start of value
	PARSE_NUMBER,			//Parsing number.
};

extern const char* error_desc[];
extern const char* state_desc[];

struct error : public std::runtime_error
{
	error(errorcode _code) : runtime_error(error_desc[_code]), code(_code), state(PARSE_NOT_PARSING),
		position(std::string::npos) {}
	error(errorcode _code, parsestate _state, size_t pos) : runtime_error(error_desc[_code]), code(_code), 
		state(_state), position(pos) {}
	errorcode get_code() { return code; }
	parsestate get_state() { return state; }
	size_t get_position() { return position; }
	std::pair<size_t, size_t> get_position_lc(const std::string& doc) { return get_position_lc(doc, position); }
	std::pair<size_t, size_t> get_position_lc(const std::string& doc, size_t pos);
	const char* what() const throw();
	std::string extended_error(const std::string& doc);
private:
	errorcode code;
	parsestate state;
	size_t position;
	mutable char buffer[512];
};

class node;

/**
 * A JSON pointer
 */
class pointer
{
public:
	pointer();
	pointer(const std::string& ptr) throw(std::bad_alloc);
	pointer(const std::u32string& ptr) throw(std::bad_alloc);
	pointer pastend() const throw(std::bad_alloc) { return field(U"-"); }
	pointer& pastend_inplace() throw(std::bad_alloc) { return field_inplace(U"-"); }
	pointer index(uint64_t idx) const throw(std::bad_alloc);
	pointer& index_inplace(uint64_t idx) throw(std::bad_alloc);
	pointer field(const std::string& fld) const throw(std::bad_alloc) { return field(to_u32string(fld)); }
	pointer& field_inplace(const std::string& fld) throw(std::bad_alloc)
	{
		return field_inplace(to_u32string(fld));
	}
	pointer field(const std::u32string& fld) const throw(std::bad_alloc);
	pointer& field_inplace(const std::u32string& fld) throw(std::bad_alloc);
	pointer remove() const throw(std::bad_alloc);
	pointer& remove_inplace() throw(std::bad_alloc);
	std::string as_string8() const { return to_u8string(_pointer); }
	std::u32string as_string() const { return _pointer; }
	friend std::ostream& operator<<(std::ostream& s, const pointer& p);
	friend std::basic_ostream<char32_t>& operator<<(std::basic_ostream<char32_t>& s, const pointer& p);
private:
	friend class node;
	std::u32string _pointer;
};

/**
 * A JSON node.
 */
class node
{
public:
/**
 * Type of node.
 */
/**
 * Construct null object.
 */
	node() throw();
/**
 * Construct object of specified type.
 */
	node(null_tag) throw();
	node(boolean_tag, bool b) throw();
	node(string_tag, const std::u32string& str) throw(std::bad_alloc);
	node(string_tag, const std::string& str) throw(std::bad_alloc);
	node(number_tag, double n) throw();
	node(number_tag, int64_t n) throw();
	node(number_tag, uint64_t n) throw();
	node(array_tag) throw();
	node(object_tag) throw();
/**
 * Compare nodes.
 */
	bool operator==(const node& n) const;
	bool operator!=(const node& n) const { return !(*this == n); }
/**
 * Copy Constructor.
 */
	node(const node& _node) throw(std::bad_alloc);
/**
 * Construct object from description.
 */
	node(const std::string& doc) throw(std::bad_alloc, error);
/**
 * Serialize document.
 */
	std::string serialize() const throw(std::bad_alloc, error);
/**
 * Get type of node.
 */
	int type() const throw();
/**
 * Get type of node by pointer.
 */
	int type_of(const std::u32string& pointer) const throw(std::bad_alloc);
	int type_of(const std::string& pointer) const throw(std::bad_alloc)
	{
		return type_of(to_u32string(pointer));
	}
	int type_of(const pointer& ptr) const throw(std::bad_alloc)
	{
		return type_of(ptr._pointer);
	}
/**
 * Get type of node by pointer (indirect).
 */
	int type_of_indirect(const std::u32string& pointer) const throw(std::bad_alloc);
	int type_of_indirect(const std::string& pointer) const throw(std::bad_alloc)
	{
		return type_of_indirect(to_u32string(pointer));
	}
	int type_of_indirect(const pointer& ptr) const throw(std::bad_alloc)
	{
		return type_of_indirect(ptr._pointer);
	}
/**
 * Resolve an indirect pointer
 */
	std::u32string resolve_indirect(const std::u32string& pointer) const throw(std::bad_alloc);
	std::string resolve_indirect(const std::string& pointer) const throw(std::bad_alloc)
	{
		return to_u8string(resolve_indirect(to_u32string(pointer)));
	}
	pointer resolve_indirect(const pointer& ptr) const throw(std::bad_alloc)
	{
		return pointer(resolve_indirect(ptr._pointer));
	}
/**
 * Get double numeric value (NT_NUMBER).
 */
	double as_double() const throw(error);
/**
 * Get int64_t value (NT_NUMBER).
 */
	int64_t as_int() const throw(error);
/**
 * Get uint64_t value (NT_NUMBER).
 */
	uint64_t as_uint() const throw(error);
/**
 * Read the string as UTF-8 (NT_STRING).
 */
	const std::u32string& as_string() const throw(std::bad_alloc, error);
	std::string as_string8() const throw(std::bad_alloc, error) { return to_u8string(as_string()); }
/**
 * Get boolean value (NT_BOOLEAN).
 */
	bool as_bool() const throw(error);
/**
 * Read number of indices in array (NT_ARRAY).
 */
	size_t index_count() const throw(error);
/**
 * Read specified index from array (NT_ARRAY).
 */
	const node& index(size_t idx) const throw(error)
	{
		const node* n;
		auto e = index_soft(idx, n);
		if(e != ERR_OK) throw error(e);
		return *n;
	}
/**
 * Read number of indices in object key (NT_OBJECT).
 */
	size_t field_count(const std::u32string& key) const throw(error);
	size_t field_count(const std::string& key) const throw(std::bad_alloc, error)
	{
		return field_count(to_u32string(key));
	}
/**
 * Specified field exists (NT_OBJECT)
 */
	bool field_exists(const std::u32string& key) const throw(error);
	bool field_exists(const std::string& key) const throw(std::bad_alloc, error)
	{
		return field_exists(to_u32string(key));
	}
/**
 * Read specified key from object (NT_OBJECT).
 */
	const node& field(const std::u32string& key, size_t subindex = 0) const throw(error)
	{
		const node* n;
		auto e = field_soft(key, subindex, n);
		if(e != ERR_OK) throw error(e);
		return *n;
	}
	const node& field(const std::string& key, size_t subindex = 0) const throw(std::bad_alloc, error)
	{
		return field(to_u32string(key), subindex);
	}

/**
 * Apply JSON pointer (RFC 6901).
 */
	const node& follow(const std::u32string& pointer) const throw(std::bad_alloc, error)
	{
		const node* n;
		auto e = follow_soft(pointer, n);
		if(e != ERR_OK) throw error(e);
		return *n;
	}
	const node& follow(const std::string& pointer) const throw(std::bad_alloc, error)
	{
		return follow(to_u32string(pointer));
	}
	const node& follow(const pointer& ptr) const throw(std::bad_alloc, error)
	{
		return follow(ptr._pointer);
	}
/**
 * Apply JSON pointer (RFC 6901) following strings as indirect references.
 */
	const node& follow_indirect(const std::u32string& pointer) const throw(std::bad_alloc, error)
	{
		return follow(resolve_indirect(pointer));
	}
	const node& follow_indirect(const std::string& pointer) const throw(std::bad_alloc, error)
	{
		return follow_indirect(to_u32string(pointer));
	}
	const node& follow_indirect(const pointer& ptr) const throw(std::bad_alloc, error)
	{
		return follow_indirect(ptr._pointer);
	}
/**
 * Set value of node (any).
 */
	node& operator=(const node& node) throw(std::bad_alloc);
/**
 * Set value of node.
 */
	node& set(null_tag) throw();
	node& set(boolean_tag, bool number) throw();
	node& set(number_tag, double number) throw();
	node& set(number_tag, int64_t number) throw();
	node& set(number_tag, uint64_t number) throw();
	node& set(string_tag, const std::u32string& key) throw(std::bad_alloc);
	node& set(string_tag tag, const std::string& key) throw(std::bad_alloc)
	{
		return set(tag, to_u32string(key));
	}
/**
 * Read/Write specified index from array (NT_ARRAY).
 */
	node& index(size_t idx) throw(error)
	{
		node* n;
		auto e = index_soft(idx, n);
		if(e != ERR_OK) throw error(e);
		return *n;
	}
/**
 * Append new element to array (NT_ARRAY).
 */
	node& append(const node& node) throw(std::bad_alloc, error);
/**
 * Read/Write specified key from object (NT_OBJECT).
 */
	node& field(const std::u32string& key, size_t subindex = 0) throw(error)
	{
		node* n;
		auto e = field_soft(key, subindex, n);
		if(e != ERR_OK) throw error(e);
		return *n;
	}
	node& field(const std::string& key, size_t subindex = 0) throw(std::bad_alloc, error)
	{
		return field(to_u32string(key), subindex);
	}
/**
 * Insert new element to object (NT_OBJECT).
 */
	node& insert(const std::u32string& key, const node& node) throw(std::bad_alloc, error);
	node& insert(const std::string& key, const node& node) throw(std::bad_alloc, error)
	{
		return insert(to_u32string(key), node);
	}
/**
 * Apply JSON pointer (RFC 6901).
 */
	node& follow(const std::u32string& pointer) throw(std::bad_alloc, error)
	{
		node* n;
		auto e = follow_soft(pointer, n);
		if(e != ERR_OK) throw error(e);
		return *n;
	}
	node& follow(const std::string& pointer) throw(std::bad_alloc, error)
	{
		return follow(to_u32string(pointer));
	}
	node& follow(const pointer& ptr) throw(std::bad_alloc, error)
	{
		return follow(ptr._pointer);
	}
/**
 * Apply JSON pointer (RFC 6901) following strings as indirect references.
 */
	node& follow_indirect(const std::u32string& pointer) throw(std::bad_alloc, error)
	{
		return follow(resolve_indirect(pointer));
	}
	node& follow_indirect(const std::string& pointer) throw(std::bad_alloc, error)
	{
		return follow_indirect(to_u32string(pointer));
	}
	node& follow_indirect(const pointer& ptr) throw(std::bad_alloc, error)
	{
		return follow_indirect(ptr._pointer);
	}
/**
 * Return node specified by JSON pointer (RFC 6901). If the last component doesn't exist, it is created as NULL.
 */
	node& operator[](const std::u32string& pointer) throw(std::bad_alloc, error);
	node& operator[](const std::string& pointer) throw(std::bad_alloc, error)
	{
		return (*this)[to_u32string(pointer)];
	}
	node& operator[](const pointer& ptr) throw(std::bad_alloc, error)
	{
		return (*this)[ptr._pointer];
	}
/**
 * Create node at specified pointer and return it.
 */
	node& insert_node(const std::u32string& pointer, const node& nwn) throw(std::bad_alloc, error);
	node& insert_node(const std::string& pointer, const node& nwn) throw(std::bad_alloc, error)
	{
		return insert_node(to_u32string(pointer), nwn);
	}
	node& insert_node(const pointer& ptr, const node& nwn) throw(std::bad_alloc, error)
	{
		return insert_node(ptr._pointer, nwn);
	}
/**
 * Delete a node by pointer and return what was deleted.
 */
	node delete_node(const std::u32string& pointer) throw(std::bad_alloc, error);
	node delete_node(const std::string& pointer) throw(std::bad_alloc, error)
	{
		return delete_node(to_u32string(pointer));
	}
	node delete_node(const pointer& ptr) throw(std::bad_alloc, error)
	{
		return delete_node(ptr._pointer);
	}
/**
 * Synonym for follow().
 */
	const node& operator[](const std::u32string& pointer) const throw(std::bad_alloc, error)
	{
		return follow(pointer);
	}
	const node& operator[](const std::string& pointer) const throw(std::bad_alloc, error)
	{
		return follow(pointer);
	}
	const node& operator[](const pointer& ptr) const throw(std::bad_alloc, error)
	{
		return follow(ptr._pointer);
	}
/**
 * Delete an array index. The rest are shifted.
 */
	void erase_index(size_t idx) throw(error);
/**
 * Delete an array field. The rest are shifted.
 */
	void erase_field(const std::u32string& fld, size_t idx = 0) throw(error);
	void erase_field(const std::string& fld, size_t idx = 0) throw(std::bad_alloc, error)
	{
		erase_field(to_u32string(fld), idx);
	}
/**
 * Delete an entiere array field.
 */
	void erase_field_all(const std::u32string& fld) throw(error);
	void erase_field_all(const std::string& fld) throw(std::bad_alloc, error)
	{
		erase_field_all(to_u32string(fld));
	}
/**
 * Apply a JSON patch.
 */
	node patch(const node& patch) const throw(std::bad_alloc, error);
/**
 * Clear entiere array or object.
 */
	void clear() throw(error);
/**
 * Iterator.
 */
	class iterator
	{
	public:
		typedef std::forward_iterator_tag iterator_category;
		typedef node value_type;
		typedef int difference_type;
		typedef node& reference;
		typedef node* pointer;
		iterator() throw();
		iterator(node& n) throw(error);
		std::u32string key() throw(std::bad_alloc, error);
		std::string key8() throw(std::bad_alloc, error) { return to_u8string(key()); }
		size_t index() throw(error);
		node& operator*() throw(error);
		node* operator->() throw(error);
		iterator operator++(int) throw(error);
		iterator& operator++() throw(error);
		bool operator==(const iterator& i) throw();
		bool operator!=(const iterator& i) throw();
	private:
		friend class node;
		node* n;
		size_t idx;
		std::u32string _key;
	};
/**
 * Constant iterator.
 */
	class const_iterator
	{
	public:
		typedef std::forward_iterator_tag iterator_category;
		typedef node value_type;
		typedef int difference_type;
		typedef const node& reference;
		typedef const node* pointer;
		const_iterator() throw();
		const_iterator(const node& n) throw(error);
		std::u32string key() throw(std::bad_alloc, error);
		std::string key8() throw(std::bad_alloc, error) { return to_u8string(key()); }
		size_t index() throw(error);
		const node& operator*() throw(error);
		const node* operator->() throw(error);
		const_iterator operator++(int) throw(error);
		const_iterator& operator++() throw(error);
		bool operator==(const const_iterator& i) throw();
		bool operator!=(const const_iterator& i) throw();
	private:
		const node* n;
		size_t idx;
		std::u32string _key;
	};
/**
 * Iterators
 */
	const_iterator begin() const throw(error) { return const_iterator(*this); }
	const_iterator end() const throw() { return const_iterator(); }
	iterator begin() throw(error) { return iterator(*this); }
	iterator end() throw() { return iterator(); }
/**
 * Delete item pointed by iterator. The rest are shifted and new iterator is returned.
 */
	iterator erase(iterator itr) throw(error);
private:
	class number_holder
	{
	public:
		number_holder() { sub = 0; n.n0 = 0; }
		number_holder(const std::string& expr, size_t& ptr, size_t len);
		template<typename T> T to() const
		{
			switch(sub) {
			case 0: return n.n0;
			case 1: return n.n1;
			case 2: return n.n2;
			}
			return 0;
		}
		template<typename T> void from(T val);
		void write(std::ostream& s) const;
		bool operator==(const number_holder& h) const;
	private:
		template<typename T> bool cmp(const T& num) const;
		unsigned sub;
		union {
			double n0;
			uint64_t n1;
			int64_t n2;
		} n;
	};
	friend class iterator;
	friend class const_iterator;
	void fixup_nodes(const node& _node);
	void ctor(const std::string& doc, size_t& ptr, size_t len) throw(std::bad_alloc, error);
	node(const std::string& doc, size_t& ptr, size_t len) throw(std::bad_alloc, error);
	template<typename T> void set_helper(T v)
	{
		std::u32string tmp;
		vtype = number;
		_number.from<T>(v);
		std::swap(_string, tmp);
		xarray.clear();
		xobject.clear();
	}
	template<typename T> T get_number_helper() const
	{
		if(vtype != number)
			throw error(ERR_NOT_A_NUMBER);
		return _number.to<T>();
	}

	int vtype;
	number_holder _number;
	bool _boolean;
	std::u32string _string;
	std::list<node> xarray;
	std::vector<node*> xarray_index;
	std::map<std::u32string, std::list<node>> xobject;
	errorcode follow_soft(const std::u32string& pointer, const node*& out) const throw(std::bad_alloc);
	errorcode follow_soft(const std::u32string& pointer, node*& out) throw(std::bad_alloc);
	errorcode field_soft(const std::u32string& key, size_t subindex, node*& out) throw();
	errorcode field_soft(const std::u32string& key, size_t subindex, const node*& out) const throw();
	errorcode index_soft(size_t index, node*& out) throw();
	errorcode index_soft(size_t index, const node*& out) const throw();
};
}

bool operator==(const int& n, const JSON::number_tag& v);
bool operator==(const int& n, const JSON::string_tag& v);
bool operator==(const int& n, const JSON::boolean_tag& v);
bool operator==(const int& n, const JSON::array_tag& v);
bool operator==(const int& n, const JSON::object_tag& v);
bool operator==(const int& n, const JSON::null_tag& v);
bool operator!=(const int& n, const JSON::number_tag& v);
bool operator!=(const int& n, const JSON::string_tag& v);
bool operator!=(const int& n, const JSON::boolean_tag& v);
bool operator!=(const int& n, const JSON::array_tag& v);
bool operator!=(const int& n, const JSON::object_tag& v);
bool operator!=(const int& n, const JSON::null_tag& v);


#endif