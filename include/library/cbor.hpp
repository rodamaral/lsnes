#ifndef _library__cbor__hpp__included__
#define _library__cbor__hpp__included__

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace CBOR
{
enum tag_type
{
	T_UNSIGNED,
	T_NEGATIVE,
	T_OCTETS,
	T_STRING,
	T_ARRAY,
	T_MAP,
	T_TAG,
	T_FLOAT,
	T_SIMPLE,
	T_BOOLEAN,
	T_NULL,
	T_UNDEFINED
};

class item
{
public:
	struct _unsigned_tag
	{
		item operator()(uint64_t val) { return item(*this, val); }
	};
	struct _negative_tag
	{
		item operator()(uint64_t val) { return item(*this, val); }
	};
	struct _octets_tag
	{
		item operator()(const std::vector<uint8_t>& val) { return item(*this, val); }
	};
	struct _string_tag
	{
		item operator()(const std::string& val) { return item(*this, val); }
	};
	struct _array_tag
	{
		item operator()() { return item(*this); }
	};
	struct _map_tag
	{
		item operator()() { return item(*this); }
	};
	struct _tag_tag
	{
		item operator()(uint64_t tag, const item& inner) { return item(*this, tag, inner); }
	};
	struct _simple_tag
	{
		item operator()(uint8_t val) { return item(*this, val); }
	};
	struct _boolean_tag
	{
		item operator()(bool val) { return item(*this, val); }
	};
	struct _null_tag
	{
		item operator()() { return item(*this); }
	};
	struct _undefined_tag
	{
		item operator()() { return item(*this); }
	};
	struct _float_tag
	{
		item operator()(double val) { return item(*this, val); }
	};
/**
 * Constructor.
 *
 * The constructed value is default UNSINGED(0).
 */
	item() throw();
/**
 * Construct new UNSIGNED value.
 *
 * Parameter _tag: Dummy.
 * Parameter _value: The value.
 */
	item(_unsigned_tag _tag, uint64_t _value) throw();
/**
 * Construct new NEGATIVE value.
 *
 * Parameter _tag: Dummy.
 * Parameter _value: The value.
 */
	item(_negative_tag _tag, uint64_t _value) throw();
/**
 * Construct new OCTETS value.
 *
 * Parameter _value: The value.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(_octets_tag _tag, const std::vector<uint8_t>& _value) throw(std::bad_alloc);
/**
 * Construct new STRING value.
 *
 * Parameter _value: The value.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::out_of_range: The value is not valid UTF-8.
 */
	item(_string_tag _tag, const std::string& _value) throw(std::bad_alloc, std::out_of_range);
/**
 * Construct new empty ARRAY value.
 *
 * Parameter _tag: Dummy.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(_array_tag _tag) throw(std::bad_alloc);
/**
 * Construct new empty MAP value.
 *
 * Parameter _tag: Dummy.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(_map_tag _tag) throw(std::bad_alloc);
/**
 * Construct new FLOAT value.
 *
 * Parameter _value: The value.
 */
	item(_float_tag _tag, double _value) throw();
/**
 * Construct new BOOLEAN value.
 *
 * Parameter _value: The value.
 */
	item(_boolean_tag _tag, bool _value) throw();
/**
 * Construct new empty SIMPLE value.
 *
 * Parameter _tag: Dummy.
 * Parameter _value: The value.
 * Throws std::out_of_range: SIMPLE values 23-31 are not allowed.
 * 
 */
	item(_simple_tag _tag, uint8_t _value) throw(std::out_of_range);
/**
 * Construct new NULL value.
 *
 * Parameter _tag: Dummy.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(_null_tag _tag) throw();
/**
 * Construct new UNDEFINED value.
 *
 * Parameter _tag: Dummy.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(_undefined_tag _tag) throw();
/**
 * Construct new TAG value.
 *
 * Parameter _tag: Dummy.
 * Parameter _value: The tag value.
 * Parameter _inner: The inner value.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(_tag_tag _tag, uint64_t _value, const item& _inner) throw(std::bad_alloc);
/**
 * Create item from CBOR.
 *
 * Parameter buffer: The buffer.
 * Parameter buffersize: The size of buffer in bytes.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Bad CBOR.
 */
	item(const char* buffer, size_t buffersize) throw(std::bad_alloc, std::runtime_error);
/**
 * Copy constructor.
 *
 * Parameter tocopy: The element to copy.
 * Throws std::bad_alloc: Not enough memory.
 */
	item(const item& tocopy) throw(std::bad_alloc);
/**
 * Assignment operator.
 *
 * Parameter tocopy: The element to copy.
 * Returns: Reference to this.
 * Throws std::bad_alloc: Not enough memory.
 */
	item& operator=(const item& tocopy) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~item();
/**
 * Serialize item as CBOR.
 *
 * Returns: The serialization.
 * Throws std::bad_alloc: Not enough memory.
 */
	std::vector<char> serialize() const throw(std::bad_alloc);
/**
 * Get type of item.
 *
 * Returns: The type of item.
 */
	tag_type get_type() const throw() { return tag; }
/**
 * Get size of CBOR serialization.
 *
 * Returns: The size in bytes.
 */
	uint64_t get_size() const throw();
/**
 * Get value of UNSIGNED.
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not UNSIGNED.
 */
	uint64_t get_unsigned() const throw(std::domain_error);
/**
 * Set value as UNSIGNED.
 *
 * Parameter _value: The value.
 */
	void set_unsigned(uint64_t _value) throw();
/**
 * Get value of NEGATIVE.
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not NEGATIVE.
 */
	uint64_t get_negative() const throw(std::domain_error);
/**
 * Set value as NEGATIVE.
 *
 * Parameter _value: The value.
 */
	void set_negative(uint64_t _value) throw();
/**
 * Get value of OCTETS.
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not OCTETS.
 */
	const std::vector<uint8_t>& get_octets() const throw(std::domain_error);
/**
 * Set value as OCTETS.
 *
 * Parameter _value: The value.
 * Throws std::bad_alloc: Not enough memory.
 */
	void set_octets(const std::vector<uint8_t>& _value) throw(std::bad_alloc);
/**
 * Set value as OCTETS.
 *
 * Parameter _value: The value.
 * Parameter _valuelen: The length of value.
 * Throws std::bad_alloc: Not enough memory.
 */
	void set_octets(const uint8_t* _value, size_t _valuelen) throw(std::bad_alloc);
/**
 * Get value of STRING.
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not STRING.
 */
	const std::string& get_string() const throw(std::domain_error);
/**
 * Set value as STRING.
 *
 * Parameter _value: The value.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::out_of_range: The value is not valid UTF-8.
 */
	void set_string(const std::string& _value) throw(std::bad_alloc, std::out_of_range);
/**
 * Set value to empty array.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	void set_array() throw(std::bad_alloc);
/**
 * Get array length.
 *
 * Returns: The number of elements in array.
 * Throws std::domain_error: The CBOR item is not ARRAY.
 */
	size_t get_array_size() const throw(std::domain_error);
/**
 * Set array length.
 *
 * Parameter newsize: New size for array.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::domain_error: The CBOR item is not ARRAY.
 */
	void set_array_size(size_t newsize) throw(std::bad_alloc, std::domain_error);
/**
 * Read array element.
 *
 * Parameter index: The element index.
 * Returns: The array element.
 * Throws std::domain_error: The CBOR item is not ARRAY.
 * Throws std::out_of_range: Invalid array index.
 */
	const item& operator[](size_t index) const throw(std::domain_error, std::out_of_range);
/**
 * Read/Write array element.
 *
 * Parameter index: The element index.
 * Returns: The array element.
 * Throws std::domain_error: The CBOR item is not ARRAY.
 * Throws std::out_of_range: Invalid array index.
 */
	item& operator[](size_t index) throw(std::domain_error, std::out_of_range);
/**
 * Set value to empty map.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	void set_map() throw(std::bad_alloc);
/**
 * Get map size.
 *
 * Returns: The number of elements in map.
 * Throws std::domain_error: The CBOR item is not MAP.
 */
	size_t get_map_size() const throw(std::domain_error);
/**
 * Read map element.
 *
 * Parameter index: The element key.
 * Returns: The map element.
 * Throws std::domain_error: The CBOR item is not MAP.
 * Throws std::out_of_range: Invalid map key.
 */
	const item& get_map_lookup(const item& index) const throw(std::domain_error, std::out_of_range);
/**
 * Read map element.
 *
 * Parameter index: The element key.
 * Returns: The map element.
 * Throws std::domain_error: The CBOR item is not MAP.
 * Throws std::out_of_range: Invalid map key.
 */
	const item& operator[](const item& index) const throw(std::domain_error, std::out_of_range)
	{
		return get_map_lookup(index);
	}
/**
 * Read/Write map element. If key does not exist, it is created.
 *
 * Parameter index: The element key.
 * Returns: The map element.
 * Throws std::domain_error: The CBOR item is not MAP.
 * Throws std::bad_alloc: Not enough memory.
 */
	item& operator[](const item& index) throw(std::bad_alloc, std::domain_error);
/**
 * Get iterator into internal map (begin).
 *
 * Returns: The iterator.
 * Throws std::domain_error: The CBOR item is not MAP.
 */
	std::map<item, item>::iterator get_map_begin() throw(std::domain_error);
/**
 * Get iterator into internal map (end).
 *
 * Returns: The iterator.
 * Throws std::domain_error: The CBOR item is not MAP.
 */
	std::map<item, item>::iterator get_map_end() throw(std::domain_error);
/**
 * Get iterator into internal map (begin).
 *
 * Returns: The iterator.
 * Throws std::domain_error: The CBOR item is not MAP.
 */
	std::map<item, item>::const_iterator get_map_begin() const throw(std::domain_error);
/**
 * Get iterator into internal map (end).
 *
 * Returns: The iterator.
 * Throws std::domain_error: The CBOR item is not MAP.
 */
	std::map<item, item>::const_iterator get_map_end() const throw(std::domain_error);
/**
 * Get tag value.
 *
 * Returns: The tag value.
 * Throws std::domain_error: The CBOR item is not TAG.
 */
	uint64_t get_tag_number() const throw(std::domain_error);
/**
 * Set tag value.
 *
 * Parameter _value: The new tag value.
 * Throws std::domain_error: The CBOR item is not TAG.
 */
	void set_tag_number(uint64_t _value) throw(std::domain_error);
/**
 * Get tag inner.
 *
 * Returns: The tag inner item.
 * Throws std::domain_error: The CBOR item is not TAG.
 */
	const item& get_tag_inner() const throw(std::domain_error);
/**
 * Get tag inner.
 *
 * Returns: The tag inner item.
 * Throws std::domain_error: The CBOR item is not TAG.
 */
	item& get_tag_inner() throw(std::domain_error);
/**
 * Tag item.
 *
 * The current item is moved to be inner item of tag.
 *
 * Parameter _value: The tag value.
 * Throws std::bad_alloc: Not enough memory.
 */
	void tag_item(uint64_t _value) throw(std::bad_alloc);
/**
 * Detag item.
 *
 * The inner item is moved to be this item.
 * Throws std::domain_error: The item is not TAG.
 */
	void detag_item() throw(std::domain_error);
/**
 * Get value of FLOAT.
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not FLOAT.
 */
	double get_float() const throw(std::domain_error);
/**
 * Set value as FLOAT.
 *
 * Parameter _value: The value.
 */
	void set_float(double _value) throw() { _set_float(*id(&_value)); }
/**
 * Get value of SIMPLE.
 *
 * Types BOOLEAN, NULL and UNDEFINED convert into appropriate values (20-23).
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not SIMPLE, BOOLEAN, NULL nor UNDEFINED.
 */
	uint8_t get_simple() const throw(std::domain_error);
/**
 * Set value as SIMPLE.
 *
 * Simple values 20-23 autoconvert to appropriate types.
 *
 * Parameter _value: The value.
 * Throws std::out_of_range: _value is 23-31 (those are not valid):
 */
	void set_simple(uint8_t _value) throw(std::out_of_range);
/**
 * Get value of BOOLEAN.
 *
 * Returns: The value.
 * Throws std::domain_error: The CBOR item is not BOOLEAN.
 */
	bool get_boolean() const throw(std::domain_error);
/**
 * Set value as BOOLEAN.
 *
 * Parameter _value: The value.
 */
	void set_boolean(bool _value) throw();
/**
 * Set value to NULL.
 */
	void set_null() throw();
/**
 * Set value to UNDEFINED.
 */
	void set_undefined() throw();
/**
 * Compare two items.
 *
 * Parameter a: The another item.
 * Returns: True if this is less than a, false otherwise.
 */
	bool operator<(const item& a) const throw() { return cmp(*this, a) < 0; }
/**
 * Compare two items.
 *
 * Parameter a: The another item.
 * Returns: True if this is less/equal to a, false otherwise.
 */
	bool operator<=(const item& a) const throw() { return cmp(*this, a) <= 0; }
/**
 * Compare two items.
 *
 * Parameter a: The another item.
 * Returns: True if this is equal to a, false otherwise.
 */
	bool operator==(const item& a) const throw() { return cmp(*this, a) == 0; }
/**
 * Compare two items.
 *
 * Parameter a: The another item.
 * Returns: True if this is not equal to a, false otherwise.
 */
	bool operator!=(const item& a) const throw() { return cmp(*this, a) != 0; }
/**
 * Compare two items.
 *
 * Parameter a: The another item.
 * Returns: True if this is greater/equal to a, false otherwise.
 */
	bool operator>=(const item& a) const throw() { return cmp(*this, a) >= 0; }
/**
 * Compare two items.
 *
 * Parameter a: The another item.
 * Returns: True if this is greater/equal to a, false otherwise.
 */
	bool operator>(const item& a) const throw() { return cmp(*this, a) > 0; }
/**
 * Swap two CBOR documents.
 *
 * Parameter a: The another document.
 */
	void swap(item& a) throw();
/**
 * Set cleanup callback.
 *
 * Pa
 */
private:
	typedef std::vector<uint8_t> octets_t;
	typedef std::string string_t;
	typedef std::map<size_t, item> array_t;
	typedef std::map<item, item> map_t;
	typedef std::pair<uint64_t, item> tag_t;

	std::function<void(item*)>* cleanup_cb;
	tag_type tag;
	union {
		uint64_t integer;
		void* pointer;
		array_t* _debug_array;
	} value;
	void release_inner() throw();
	void replace_inner(tag_type _tag, uint64_t _integer) throw();
	void replace_inner(tag_type _tag, void* _pointer) throw();
	void _set_float(uint64_t _value) throw();
	void ctor(const char* b, size_t& p, size_t s);
	static int cmp(const item& a, const item& b) throw();
	static int cmp_nosize(const item& a, const item& b) throw();
	void _serialize(char*& out) const throw();
	void check_array() const throw(std::domain_error);
	void check_boolean() const throw(std::domain_error);
	void check_float() const throw(std::domain_error);
	void check_map() const throw(std::domain_error);
	void check_negative() const throw(std::domain_error);
	void check_octets() const throw(std::domain_error);
	void check_string() const throw(std::domain_error);
	void check_tag() const throw(std::domain_error);
	void check_unsigned() const throw(std::domain_error);
	uint8_t get_tagbyte() const throw();
	uint64_t get_arg() const throw();
	const uint64_t* id(const double* v) const { return reinterpret_cast<const uint64_t*>(v); }
	const double* idinv(const uint64_t* v) const { return reinterpret_cast<const double*>(v); }
	template<typename T> T& inner_as() throw() { return *reinterpret_cast<T*>(value.pointer); }
	template<typename T> const T& inner_as() const throw() { return *reinterpret_cast<const T*>(value.pointer); }
	template<typename T> void* inner_copy(const void* pointer)
	{
		return new T(*reinterpret_cast<const T*>(pointer));
	}
};

extern item::_unsigned_tag integer;
extern item::_negative_tag negative;
extern item::_octets_tag octets;
extern item::_string_tag string;
extern item::_array_tag array;
extern item::_map_tag map;
extern item::_tag_tag tag;
extern item::_simple_tag simple;
extern item::_boolean_tag boolean;
extern item::_null_tag null;
extern item::_undefined_tag undefined;
extern item::_float_tag floating;

extern thread_local std::function<void(item*)> dtor_cb;
}

#endif