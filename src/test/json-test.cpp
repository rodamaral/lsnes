#include "json.hpp"
#include "string.hpp"

struct test_x
{
	const char* title;
	bool (*dotest)();
	JSON::errorcode expect;
};

test_x tests[] = {
	{"Default constructor", []() {
		JSON::node x;
		return (x.type() == JSON::null);
	}},{"Explicit null constructor", []() {
		JSON::node x(JSON::null);
		return (x.type() == JSON::null);
	}},{"Explicit boolean constructor #1", []() {
		JSON::node x(JSON::boolean, false);
		return (x.type() == JSON::boolean);
	}},{"Explicit boolean constructor #2", []() {
		JSON::node x(JSON::boolean, true);
		return (x.type() == JSON::boolean);
	}},{"Explicit boolean constructor #3", []() {
		JSON::node x(JSON::boolean, false);
		return !x.as_bool();
	}},{"Explicit boolean constructor #4", []() {
		JSON::node x(JSON::boolean, true);
		return x.as_bool();
	}},{"Explicit string constructor #1", []() {
		JSON::node x(JSON::string, U"foo");
		return (x.type() == JSON::string);
	}},{"Explicit string constructor #2", []() {
		JSON::node x(JSON::string, U"bar");
		return x.as_string() == U"bar";
	}},{"Numeric types #1", []() {
		JSON::node x(JSON::number, (double)0.0);
		return (x.type() == JSON::number);
	}},{"Numeric types #2", []() {
		JSON::node x(JSON::number, (uint64_t)0);
		return (x.type() == JSON::number);
	}},{"Numeric types #3", []() {
		JSON::node x(JSON::number, (int64_t)0);
		return (x.type() == JSON::number);
	}},{"Numeric conversions #1", []() {
		JSON::node x(JSON::number, (double)123.45);
		return x.as_double() == 123.45;
	}},{"Numeric conversions #2", []() {
		JSON::node x(JSON::number, (uint64_t)9223372036854775809ULL);
		return x.as_uint() == 9223372036854775809ULL;
	}},{"Numeric conversions #3", []() {
		JSON::node x(JSON::number, (int64_t)-36028797018963969LL);
		return x.as_int() == -36028797018963969LL;
	}},{"Numeric conversions #4", []() {
		JSON::node x(JSON::number, (double)123.0);
		return x.as_uint() == 123;
	}},{"Numeric conversions #5", []() {
		JSON::node x(JSON::number, (uint64_t)12345);
		return x.as_int() == 12345;
	}},{"Numeric conversions #6", []() {
		JSON::node x(JSON::number, (int64_t)-456);
		return x.as_double() == -456;
	}},{"Numeric conversions #7", []() {
		JSON::node x(JSON::number, (double)-1234.0);
		return x.as_int() == -1234;
	}},{"Numeric conversions #8", []() {
		JSON::node x(JSON::number, (uint64_t)12345);
		return x.as_double() == 12345.0;
	}},{"Numeric conversions #9", []() {
		JSON::node x(JSON::number, (int64_t)456);
		return x.as_uint() == 456;
	}},{"Explicit array constructor", []() {
		JSON::node x(JSON::array);
		return (x.type() == JSON::array);
	}},{"Explicit object constructor", []() {
		JSON::node x(JSON::object);
		return (x.type() == JSON::object);
	}},{"Explicit set null", []() {
		JSON::node x(JSON::object);
		x.set(JSON::null);
		return (x.type() == JSON::null);
	}},{"Explicit set boolean #1", []() {
		JSON::node x(JSON::object);
		x.set(JSON::boolean, false);
		return (x.type() == JSON::boolean);
	}},{"Explicit set boolean #2", []() {
		JSON::node x(JSON::object);
		x.set(JSON::boolean, true);
		return (x.type() == JSON::boolean);
	}},{"Explicit set boolean #3", []() {
		JSON::node x(JSON::object);
		x.set(JSON::boolean, false);
		return !x.as_bool();
	}},{"Explicit set boolean #4", []() {
		JSON::node x(JSON::object);
		x.set(JSON::boolean, true);
		return x.as_bool();
	}},{"Explicit set number #1", []() {
		JSON::node x(JSON::object);
		x.set(JSON::number, (double)123.45);
		return (x.type() == JSON::number);
	}},{"Explicit set number #2", []() {
		JSON::node x(JSON::object);
		x.set(JSON::number, (uint64_t)9223372036854775809ULL);
		return (x.type() == JSON::number);
	}},{"Explicit set number #3", []() {
		JSON::node x(JSON::object);
		x.set(JSON::number, (int64_t)-123);
		return (x.type() == JSON::number);
	}},{"Explicit set number #4", []() {
		JSON::node x(JSON::object);
		x.set(JSON::number, (double)123.45);
		return (x.as_double() == 123.45);
	}},{"Explicit set number #5", []() {
		JSON::node x(JSON::object);
		x.set(JSON::number, (uint64_t)9223372036854775809ULL);
		return (x.as_uint() == 9223372036854775809ULL);
	}},{"Explicit set number #6", []() {
		JSON::node x(JSON::object);
		x.set(JSON::number, (int64_t)-36028797018963969LL);
		return (x.as_int() == -36028797018963969LL);
	}},{"Explicit set string #1", []() {
		JSON::node x(JSON::object);
		x.set(JSON::string, U"qux");
		return (x.type() == JSON::string);
	}},{"Explicit set string #2", []() {
		JSON::node x(JSON::object);
		x.set(JSON::string, U"zot");
		return (x.as_string() == U"zot");
	}},{"Non-string as string", []() {
		JSON::node x(JSON::object);
		x.as_string();
		return false;
	}, JSON::ERR_NOT_A_STRING},{"Non-boolean as boolean", []() {
		JSON::node x(JSON::object);
		x.as_bool();
		return false;
	}, JSON::ERR_NOT_A_BOOLEAN},{"Non-number as number #1", []() {
		JSON::node x(JSON::object);
		x.as_double();
		return false;
	}, JSON::ERR_NOT_A_NUMBER},{"Non-number as number #2", []() {
		JSON::node x(JSON::object);
		x.as_int();
		return false;
	}, JSON::ERR_NOT_A_NUMBER},{"Non-number as number #3", []() {
		JSON::node x(JSON::object);
		x.as_uint();
		return false;
	}, JSON::ERR_NOT_A_NUMBER},{"Index count of non-array", []() {
		JSON::node x(JSON::object);
		x.index_count();
		return false;
	}, JSON::ERR_NOT_AN_ARRAY},{"Index of non-array", []() {
		JSON::node x(JSON::object);
		x.index(0);
		return false;
	}, JSON::ERR_NOT_AN_ARRAY},{"Index of non-array (const)", []() {
		JSON::node x(JSON::object);
		const JSON::node& y = x;
		y.index(0);
		return false;
	}, JSON::ERR_NOT_AN_ARRAY},{"Index count of array #1", []() {
		JSON::node x("[]");
		return (x.index_count() == 0);
	}},{"Index count of array #2", []() {
		JSON::node x("[null]");
		return (x.index_count() == 1);
	}},{"Index count of array #3", []() {
		JSON::node x("[null,false,true]");
		return (x.index_count() == 3);
	}},{"Index count of array #4", []() {
		JSON::node x("[null,[false,true]]");
		return (x.index_count() == 2);
	}},{"Indexing array #1", []() {
		JSON::node x("[null,false,true]");
		return (x.index(0).type() == JSON::null);
	}},{"Indexing array #2", []() {
		JSON::node x("[null,false,true]");
		return !x.index(1).as_bool();
	}},{"Indexing array #3", []() {
		JSON::node x("[null,false,true]");
		return x.index(2).as_bool();
	}},{"Indexing array out-of-bounds #1", []() {
		JSON::node x("[null,false,true]");
		x.index(3);
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Indexing array out-of-bounds #2", []() {
		JSON::node x("[null,false,true]");
		x.index(12466);
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Indexing array out-of-bounds #3", []() {
		JSON::node x("[]");
		x.index(0);
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Indexing array #1 (const)", []() {
		JSON::node x("[null,false,true]");
		const JSON::node& y = x;
		return (y.index(0).type() == JSON::null);
	}},{"Indexing array #2 (const)", []() {
		JSON::node x("[null,false,true]");
		const JSON::node& y = x;
		return !y.index(1).as_bool();
	}},{"Indexing array #3 (const)", []() {
		JSON::node x("[null,false,true]");
		const JSON::node& y = x;
		return y.index(2).as_bool();
	}},{"Indexing array out-of-bounds #1 (const)", []() {
		JSON::node x("[null,false,true]");
		const JSON::node& y = x;
		y.index(3);
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Indexing array out-of-bounds #2 (const)", []() {
		JSON::node x("[null,false,true]");
		const JSON::node& y = x;
		y.index(12466);
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Indexing array out-of-bounds #3 (const)", []() {
		JSON::node x("[]");
		const JSON::node& y = x;
		y.index(0);
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Field count of non-object", []() {
		JSON::node x("[null,false,true]");
		x.field_count(U"foo");
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"Field exists of non-object", []() {
		JSON::node x("[null,false,true]");
		x.field_exists(U"foo");
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"Field count of found object", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		return (x.field_count(U"foo") == 1);
	}},{"Field count of non-found object #1", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		return (x.field_count(U"bar") == 0);
	}},{"Field count of non-found object #2", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		return (x.field_count(U"qux") == 0);
	}},{"Field exists of found object", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		return x.field_exists(U"foo");
	}},{"Field exists of non-found object #1", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		return !x.field_exists(U"bar");
	}},{"Field exists of non-found object #2", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		return !x.field_exists(U"qux");
	}},{"Field count of duplicate object #1", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		return (x.field_count(U"foo") == 2);
	}},{"Field count of duplicate object #2", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		x.insert(U"foo", JSON::node(JSON::boolean, true));
		return (x.field_count(U"foo") == 3);
	}},{"Field exists of duplicate object #1", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		return x.field_exists(U"foo");
	}},{"Field exists of duplicate object #2", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		x.insert(U"foo", JSON::node(JSON::boolean, true));
		return x.field_exists(U"foo");
	}},{"Field access of non-object", []() {
		JSON::node x("[null,false,true]");
		x.field(U"foo");
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"Field access #1", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		return (x.field(U"foo").type() == JSON::null);
	}},{"Field access #2", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		return !x.field(U"bar").as_bool();
	}},{"Field access #3", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		return (x.field(U"foo").type() == JSON::null);
	}},{"Field access #4", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		return !x.field(U"foo", 1).as_bool();
	}},{"Field access to invalid key", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		x.field(U"baz");
		return false;
	}, JSON::ERR_KEY_INVALID},{"Field access to invalid index #1", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		x.field(U"bar", 1);
		return false;
	}, JSON::ERR_INSTANCE_INVALID},{"Field access to invalid index #2", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		x.field(U"foo", 2);
		return false;
	}, JSON::ERR_INSTANCE_INVALID},{"Field access of non-object (const)", []() {
		JSON::node x("[null,false,true]");
		const JSON::node& y = x;
		y.field(U"foo");
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"Field access #1 (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		return (y.field(U"foo").type() == JSON::null);
	}},{"Field access #2 (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		return !y.field(U"bar").as_bool();
	}},{"Field access #3 (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		return (y.field(U"foo").type() == JSON::null);
	}},{"Field access #4 (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		return !y.field(U"foo", 1).as_bool();
	}},{"Field access to invalid key (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		y.field(U"baz");
		return false;
	}, JSON::ERR_KEY_INVALID},{"Field access to invalid index #1 (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		y.field(U"bar", 1);
		return false;
	}, JSON::ERR_INSTANCE_INVALID},{"Field access to invalid index #2 (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::null));
		x.insert(U"foo", JSON::node(JSON::boolean, false));
		const JSON::node& y = x;
		y.field(U"foo", 2);
		return false;
	}, JSON::ERR_INSTANCE_INVALID},{"Append of non-array", []() {
		JSON::node x(JSON::object);
		x.append(JSON::node(JSON::null));
		return false;
	}, JSON::ERR_NOT_AN_ARRAY},{"Insert of non-object", []() {
		JSON::node x("[null,false,true]");
		x.insert(U"foo", JSON::node(JSON::boolean, true));
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"Follow #1", []() {
		JSON::node x("[null,[false,true]]");
		return (x.follow(U"0").type() == JSON::null);
	}},{"Follow #2", []() {
		JSON::node x("[null,[false,true]]");
		return (x.follow(U"1").type() == JSON::array);
	}},{"Follow #3", []() {
		JSON::node x("[null,[false,true]]");
		return !x.follow(U"1/0").as_bool();
	}},{"Follow #4", []() {
		JSON::node x("[null,[false,true]]");
		return x.follow(U"1/1").as_bool();
	}},{"Follow invalid pointer #1", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"2");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #2", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"0/1");
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Follow invalid pointer #3", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"1/2");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #4", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"foo");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #5", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"1/foo");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #6", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"0/foo");
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Follow invalid pointer #7", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"2/0");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #8", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"2/foo");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #9", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"foo/0");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #10", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"1~10");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #11", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"~2");
		return false;
	}, JSON::ERR_POINTER_INVALID_ESCAPE},{"Follow invalid pointer #12", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"1~");
		return false;
	}, JSON::ERR_POINTER_TRAILING_ESCAPE},{"Follow invalid pointer #13", []() {
		JSON::node x("[null,[false,true]]");
		x.follow(U"-");
		return false;
	}, JSON::ERR_POINTER_BAD_APPEND},{"Follow #1 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		return (y.follow(U"0").type() == JSON::null);
	}},{"Follow #2 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		return (y.follow(U"1").type() == JSON::array);
	}},{"Follow #3 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		return !y.follow(U"1/0").as_bool();
	}},{"Follow #4 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		return y.follow(U"1/1").as_bool();
	}},{"Follow invalid pointer #1 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"2");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #2 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"0/1");
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Follow invalid pointer #3 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"1/2");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #4 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"foo");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #5 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"1/foo");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #6 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"0/foo");
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Follow invalid pointer #7 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"2/0");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #8 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"2/foo");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Follow invalid pointer #9 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"foo/0");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #10 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"1~10");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"Follow invalid pointer #11 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"~2");
		return false;
	}, JSON::ERR_POINTER_INVALID_ESCAPE},{"Follow invalid pointer #12 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"1~");
		return false;
	}, JSON::ERR_POINTER_TRAILING_ESCAPE},{"Follow invalid pointer #13 (const)", []() {
		JSON::node x("[null,[false,true]]");
		const JSON::node& y = x;
		y.follow(U"-");
		return false;
	}, JSON::ERR_POINTER_BAD_APPEND},{"Follow ~", []() {
		JSON::node x(JSON::object);
		x.insert(U"~", JSON::node(JSON::boolean, true));
		return x.follow(U"~0").as_bool();
	}},{"Follow ~ (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"~", JSON::node(JSON::boolean, true));
		const JSON::node& y = x;
		return y.follow(U"~0").as_bool();
	}},{"Follow nonexistent", []() {
		JSON::node x(JSON::object);
		x.insert(U"~x", JSON::node(JSON::boolean, true));
		x.follow(U"~0").as_bool();
		return false;
	}, JSON::ERR_KEY_INVALID},{"Follow nonexistent (const)", []() {
		JSON::node x(JSON::object);
		x.insert(U"~x", JSON::node(JSON::boolean, true));
		const JSON::node& y = x;
		y.follow(U"~0").as_bool();
		return false;
	}, JSON::ERR_KEY_INVALID},{"Empty string token", []() {
		JSON::node x("\"\"");
		return (x.as_string() == U"");
	}},{"Simple string token", []() {
		JSON::node x("\"hello\"");
		std::cout << utf8::to8(x.as_string()) << "..." << std::flush;
		return (x.as_string() == U"hello");
	}},{"Simple number token #1", []() {
		JSON::node x("123");
		return (x.as_int() == 123);
	}},{"Simple number token #2", []() {
		JSON::node x("-123");
		return (x.as_int() == -123);
	}},{"Simple number token #3", []() {
		JSON::node x("-123.45e2");
		return (x.as_int() == -12345);
	}},{"Simple number token #4", []() {
		JSON::node x("-123e2");
		return (x.as_int() == -12300);
	}},{"Bad number token #1", []() {
		JSON::node x("123f3");
		return false;
	}, JSON::ERR_GARBAGE_AFTER_END},{"Bad number token #2", []() {
		JSON::node x("123e3e3");
		return false;
	}, JSON::ERR_GARBAGE_AFTER_END},{"Bad number token #3", []() {
		JSON::node x("123e3.3");
		return false;
	}, JSON::ERR_GARBAGE_AFTER_END},{"Bad number token #4", []() {
		JSON::node x("123.3.3");
		return false;
	}, JSON::ERR_GARBAGE_AFTER_END},{"Bad number token #5", []() {
		JSON::node x("-");
		return false;
	}, JSON::ERR_INVALID_NUMBER},{"Bad number token #6", []() {
		JSON::node x("-0.");
		return false;
	}, JSON::ERR_INVALID_NUMBER},{"Bad number token #7", []() {
		JSON::node x("-e+6");
		return false;
	}, JSON::ERR_INVALID_NUMBER},{"Bad number token #8", []() {
		JSON::node x("-0.5e+");
		return false;
	}, JSON::ERR_INVALID_NUMBER},{"Bad number token #9", []() {
		JSON::node x("-0.5e-");
		return false;
	}, JSON::ERR_INVALID_NUMBER},{"Bad number token #10", []() {
		JSON::node x("-01");
		return false;
	}, JSON::ERR_GARBAGE_AFTER_END},{"Hex string escapes", []() {
		JSON::node x("\"\\u0123\\u4567\\u89AB\\uCDEF\\uabcd\\uef00\"");
		return (x.as_string() == U"\U00000123\U00004567\U000089ab\U0000cdef\U0000abcd\U0000ef00");
	}},{"Surrogate escape #1", []() {
		JSON::node x("\"\\uD800\\uDC00\"");
		return (x.as_string() == U"\U00010000");
	}},{"Surrogate escape #2", []() {
		JSON::node x("\"\\uD800\\uDC01\"");
		return (x.as_string() == U"\U00010001");
	}},{"Surrogate escape #3", []() {
		JSON::node x("\"\\uD801\\uDC02\"");
		return (x.as_string() == U"\U00010402");
	}},{"Bad hex character", []() {
		JSON::node x("\"\\u01g5\"");
		return false;
	}, JSON::ERR_BAD_HEX},{"Bad surrogate escape #1", []() {
		JSON::node x("\"\\uD800\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #2", []() {
		JSON::node x("\"\\uD800\\n\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #3", []() {
		JSON::node x("\"\\uD800\\u0000\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #4", []() {
		JSON::node x("\"\\uDC00\\uD800\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #5", []() {
		JSON::node x("\"\\uDC00\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #6", []() {
		JSON::node x("\"\\uDC00\\uDC00\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #7", []() {
		JSON::node x("\"\\uD800Hi\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad surrogate escape #8", []() {
		JSON::node x("\"\\uDC800\"");
		return false;
	}, JSON::ERR_INVALID_SURROGATE},{"Bad string expression #1", []() {
		JSON::node x("\"Hi");
		return false;
	}, JSON::ERR_TRUNCATED_STRING},{"Bad string expression #2", []() {
		JSON::node x("\"\\n");
		return false;
	}, JSON::ERR_TRUNCATED_STRING},{"Bad escape", []() {
		JSON::node x("\"\\e\"");
		return false;
	}, JSON::ERR_INVALID_ESCAPE},{"Escapes", []() {
		JSON::node x("\"\\\"\\\\\\/\\b\\f\\r\\n\\t\"");
		return (x.as_string() == U"\"\\/\b\f\r\n\t");
	}},{"Whitespace", []() {
		JSON::node x(" [\t123,\n456,\r789]");
		return (x.follow(U"2").as_int() == 789);
	}},{"Trailing whitespace", []() {
		JSON::node x("123 ");
		return (x.as_int() == 123);
	}},{"Bad string character", []() {
		JSON::node x("\"\n\"]");
		return false;
	}, JSON::ERR_CONTROL_CHARACTER},{"Incomplete array #1", []() {
		JSON::node x("[");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Incomplete array #2", []() {
		JSON::node x("[123");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Incomplete array #3", []() {
		JSON::node x("[123,");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Incomplete array #4", []() {
		JSON::node x("[123,456");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Incomplete object", []() {
		JSON::node x("{");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #1", []() {
		JSON::node x("{\"hi\"");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #2", []() {
		JSON::node x("{\"hi\"}");
		return false;
	}, JSON::ERR_EXPECTED_COLON},{"Bad object #3", []() {
		JSON::node x("{\"hi\":");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #4", []() {
		JSON::node x("{\"hi\"}");
		return false;
	}, JSON::ERR_EXPECTED_COLON},{"Bad object #5", []() {
		JSON::node x("{\"hi\":123");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #6", []() {
		JSON::node x("{\"hi\":123,");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #7", []() {
		JSON::node x("{\"hi\":123,}");
		return false;
	}, JSON::ERR_EXPECTED_STRING_KEY},{"Bad object #8", []() {
		JSON::node x("{\"hi\":123,\"fu\"");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #9", []() {
		JSON::node x("{\"hi\":123,\"fu\"}");
		return false;
	}, JSON::ERR_EXPECTED_COLON},{"Bad object #10", []() {
		JSON::node x("{\"hi\":123,\"fu\":");
		return false;
	}, JSON::ERR_TRUNCATED_JSON},{"Bad object #11", []() {
		JSON::node x("{\"hi\":123,\"fu\":}");
		return false;
	}, JSON::ERR_UNEXPECTED_RIGHT_BRACE},{"Bad object #12", []() {
		JSON::node x("{\"hi\",123}");
		return false;
	}, JSON::ERR_EXPECTED_COLON},{"Bad object #13", []() {
		JSON::node x("{\"hi\"123}");
		return false;
	}, JSON::ERR_EXPECTED_COLON},{"Bad object #14", []() {
		JSON::node x("{\"hi\":123:\"fu\":null}");
		return false;
	}, JSON::ERR_EXPECTED_COMMA},{"Bad array", []() {
		JSON::node x("[1\"hi\"]");
		return false;
	}, JSON::ERR_EXPECTED_COMMA},{"Bad array #2", []() {
		JSON::node x("[123,]");
		return false;
	}, JSON::ERR_UNEXPECTED_RIGHT_BRACKET},{"Bad token #1", []() {
		JSON::node x("+123");
		return false;
	}, JSON::ERR_UNKNOWN_CHARACTER},{"Bad token #2", []() {
		JSON::node x(",");
		return false;
	}, JSON::ERR_UNEXPECTED_COMMA},{"Bad token #3", []() {
		JSON::node x("}");
		return false;
	}, JSON::ERR_UNEXPECTED_RIGHT_BRACE},{"Bad token #4", []() {
		JSON::node x("]");
		return false;
	}, JSON::ERR_UNEXPECTED_RIGHT_BRACKET},{"Bad token #5", []() {
		JSON::node x(":");
		return false;
	}, JSON::ERR_UNEXPECTED_COLON},{"Bad token #6", []() {
		JSON::node x("[}");
		return false;
	}, JSON::ERR_UNEXPECTED_RIGHT_BRACE},{"Bad token #7", []() {
		JSON::node x("{]");
		return false;
	}, JSON::ERR_EXPECTED_STRING_KEY},{"Empty object", []() {
		JSON::node x("{}");
		return (x.type() == JSON::object);
	}},{"Simple object", []() {
		JSON::node x("{\"foo\":\"bar\"}");
		return (x.follow(U"foo").as_string() == U"bar");
	}},{"Nested object", []() {
		JSON::node x("{\"foo\":{\"bar\":\"baz\"}}");
		return (x.follow(U"foo/bar").as_string() == U"baz");
	}},{"Simple object follow escape #1", []() {
		JSON::node x("{\"foo~\":\"bar\"}");
		return (x.follow(U"foo~0").as_string() == U"bar");
	}},{"Simple object follow escape #2", []() {
		JSON::node x("{\"foo/\":\"bar\"}");
		return (x.follow(U"foo~1").as_string() == U"bar");
	}},{"Simple object follow escape #3", []() {
		JSON::node x("{\"foo~1\":\"bar\"}");
		return (x.follow(U"foo~01").as_string() == U"bar");
	}},{"Negative number", []() {
		JSON::node x("-123");
		return (x.as_int() == -123);
	}},{"Serialize null", []() {
		JSON::node x(JSON::null);
		return (x.serialize() == "null");
	}},{"Serialize true", []() {
		JSON::node x(JSON::boolean, true);
		return (x.serialize() == "true");
	}},{"Serialize false", []() {
		JSON::node x(JSON::boolean, false);
		return (x.serialize() == "false");
	}},{"Serialize number #1", []() {
		JSON::node x(JSON::number, (int64_t)-123);
		return (x.serialize() == "-123");
	}},{"Serialize number #2", []() {
		JSON::node x(JSON::number, (uint64_t)456);
		return (x.serialize() == "456");
	}},{"Serialize number #3", []() {
		JSON::node x(JSON::number, (double)-12345.7);
		return (x.serialize() == "-12345.7");
	}},{"Serialize simple string", []() {
		JSON::node x(JSON::string, U"Hello");
		return (x.serialize() == "\"Hello\"");
	}},{"Serialize control characters", []() {
		JSON::node x(JSON::string, U"\n");
		return (x.serialize() == "\"\\n\"");
	}},{"Serialize backslash", []() {
		JSON::node x(JSON::string, U"hello\\world");
		return (x.serialize() == "\"hello\\\\world\"");
	}},{"Serialize doublequote", []() {
		JSON::node x(JSON::string, U"hello\"world");
		return (x.serialize() == "\"hello\\\"world\"");
	}},{"String with 2-byte character", []() {
		JSON::node x("\"hello\\u00a4\"");
		return (x.serialize() == "\"hello\302\244\"");
	}},{"String with 3-byte character", []() {
		JSON::node x("\"hello\\u1234\"");
		return (x.serialize() == "\"hello\341\210\264\"");
	}},{"String with 4-byte character", []() {
		JSON::node x(JSON::string, U"hello\U00045678");
		return (x.serialize() == "\"hello\361\205\231\270\"");
	}},{"Serialize empty array", []() {
		JSON::node x(JSON::array);
		return (x.serialize() == "[]");
	}},{"Serialize one-element array", []() {
		JSON::node x(JSON::array);
		x.append(JSON::node(JSON::boolean, true));
		return (x.serialize() == "[true]");
	}},{"Serialize two-element array", []() {
		JSON::node x(JSON::array);
		x.append(JSON::node(JSON::boolean, true));
		x.append(JSON::node(JSON::boolean, false));
		return (x.serialize() == "[true,false]");
	}},{"Serialize three-element array", []() {
		JSON::node x(JSON::array);
		x.append(JSON::node(JSON::boolean, true));
		x.append(JSON::node(JSON::boolean, false));
		x.append(JSON::node(JSON::null));
		return (x.serialize() == "[true,false,null]");
	}},{"Serialize empty object", []() {
		JSON::node x(JSON::object);
		return (x.serialize() == "{}");
	}},{"Serialize one-element object", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::boolean, true));
		return (x.serialize() == "{\"foo\":true}");
	}},{"Serialize two-element object", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::boolean, true));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		return (x.serialize() == "{\"bar\":false,\"foo\":true}");
	}},{"Serialize three-element object", []() {
		JSON::node x(JSON::object);
		x.insert(U"foo", JSON::node(JSON::boolean, true));
		x.insert(U"bar", JSON::node(JSON::boolean, false));
		x.insert(U"baz", JSON::node(JSON::null));
		return (x.serialize() == "{\"bar\":false,\"baz\":null,\"foo\":true}");
	}},{"Self-Assignment", []() {
		JSON::node x("true");
		x = x;
		return x.as_bool();
	}},{"Create node #1", []() {
		JSON::node x("{}");
		x["foo"] = JSON::node(JSON::string, U"bar");
		return (x["foo"].as_string() == U"bar");
	}},{"Create node #2", []() {
		JSON::node x("[]");
		x["0"] = JSON::node(JSON::string, U"bar");
		return (x["0"].as_string() == U"bar");
	}},{"Create node #3", []() {
		JSON::node x("[]");
		x["-"] = JSON::node(JSON::string, U"bar");
		return (x["0"].as_string() == U"bar");
	}},{"Create node error #1", []() {
		JSON::node x("[]");
		x["-/0"] = JSON::node(JSON::string, U"bar");
		return false;
	}, JSON::ERR_POINTER_BAD_APPEND},{"Create node error #2", []() {
		JSON::node x("[]");
		x["1"] = JSON::node(JSON::string, U"bar");
		return false;
	}, JSON::ERR_POINTER_BAD_APPEND},{"Create node error #3", []() {
		JSON::node x("false");
		x["0"] = JSON::node(JSON::string, U"bar");
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Parsing one null", []() {
		JSON::node x("null");
		return (x.type() == JSON::null);
	}},{"Type of false", []() {
		JSON::node x("false");
		return (x.type() == JSON::boolean);
	}},{"Type of true", []() {
		JSON::node x("true");
		return (x.type() == JSON::boolean);
	}},{"Value of false", []() {
		JSON::node x("false");
		return !x.as_bool();
	}},{"Value of true", []() {
		JSON::node x("true");
		return x.as_bool();
	}},{"n()", []() {
		JSON::node x = JSON::n();
		return (x.type() == JSON::null);
	}},{"_bool()", []() {
		JSON::node x = JSON::b(true);
		return x.as_bool();
	}},{"_double()", []() {
		JSON::node x = JSON::f(123.45);
		return (x.as_double() == 123.45);
	}},{"_int()", []() {
		JSON::node x = JSON::i(-36028797018963969LL);
		return (x.as_int() == -36028797018963969LL);
	}},{"_uint()", []() {
		JSON::node x = JSON::u(9223372036854775809ULL);
		return (x.as_uint() == 9223372036854775809ULL);
	}},{"_string()", []() {
		JSON::node x = JSON::s(U"foo");
		return (x.as_string() == U"foo");
	}},{"array()", []() {
		JSON::node x = JSON::array();
		return (x.type() == JSON::array);
	}},{"object()", []() {
		JSON::node x = JSON::object();
		return (x.type() == JSON::object);
	}},{"Array iteration", []() {
		JSON::node x("[\"ABC\",\"DEF\",\"GHI\"]");
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"ABCDEFGHI");
	}},{"Object iteration", []() {
		JSON::node x("{\"2\":\"ABC\",\"1\":\"DEF\",\"0\":\"GHI\"}");
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"GHIDEFABC");
	}},{"Empty Array iteration", []() {
		JSON::node x("[]");
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"");
	}},{"Empty Object iteration", []() {
		JSON::node x("{}");
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"");
	}},{"Boolean iteration", []() {
		JSON::node x("false");
		for(auto i : x);
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Array iteration (const)", []() {
		JSON::node _x("[\"ABC\",\"DEF\",\"GHI\"]");
		const JSON::node& x = _x;
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"ABCDEFGHI");
	}},{"Object iteration (const)", []() {
		JSON::node _x("{\"2\":\"ABC\",\"1\":\"DEF\",\"0\":\"GHI\"}");
		const JSON::node& x = _x;
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"GHIDEFABC");
	}},{"Empty Array iteration (const)", []() {
		JSON::node _x("[]");
		const JSON::node& x = _x;
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"");
	}},{"Empty Object iteration (const)", []() {
		JSON::node _x("{}");
		const JSON::node& x = _x;
		std::u32string y;
		for(auto i : x) {
			y += i.as_string();
		}
		return (y == U"");
	}},{"Boolean iteration (const)", []() {
		JSON::node _x("false");
		const JSON::node& x = _x;
		for(auto i : x);
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Keyed array iteration", []() {
		JSON::node x("[\"ABC\",\"DEF\",\"GHI\"]");
		const char32_t* str[] = {U"ABC", U"DEF", U"GHI"};
		for(auto i = x.begin(); i != x.end(); i++) {
			if(i.index() > 2)
				return false;
			if(i->as_string() != str[i.index()])
				return false;
		}
		return true;
	}},{"Keyed array iteration (const)", []() {
		JSON::node _x("[\"ABC\",\"DEF\",\"GHI\"]");
		const JSON::node& x = _x;
		const char32_t* str[] = {U"ABC", U"DEF", U"GHI"};
		for(auto i = x.begin(); i != x.end(); i++) {
			if(i.index() > 2)
				return false;
			if(i->as_string() != str[i.index()])
				return false;
		}
		return true;
	}},{"Keyed object iteration (key)", []() {
		JSON::node x("{\"ABC\":\"DEF\"}");
		for(auto i = x.begin(); i != x.end(); i++)
			return (i.key() == U"ABC");
		return false;
	}},{"Keyed object iteration (key, const)", []() {
		JSON::node _x("{\"ABC\":\"DEF\"}");
		const JSON::node& x = _x;
		for(auto i = x.begin(); i != x.end(); i++)
			return (i.key() == U"ABC");
		return false;
	}},{"Keyed array iteration (key)", []() {
		JSON::node x("[\"ABC\",\"DEF\"]");
		for(auto i = x.begin(); i != x.end(); i++) {
			if(i.key() != U"")
				return false;
		}
		return true;
	}},{"Keyed array iteration (key, const)", []() {
		JSON::node _x("[\"ABC\",\"DEF\"]");
		const JSON::node& x = _x;
		for(auto i = x.begin(); i != x.end(); i++) {
			if(i.key() != U"")
				return false;
		}
		return true;
	}},{"Keyed object iteration", []() {
		JSON::node x("{\"2\":\"ABC\",\"1\":\"DEF\",\"0\":\"GHI\"}");
		const char32_t* str[] = {U"GHI", U"DEF", U"ABC"};
		for(auto i = x.begin(); i != x.end(); i++) {
			size_t idx = parse_value<size_t>(utf8::to8(i.key()));
			if(i->as_string() != str[idx])
				return false;
		}
		return true;
	}},{"Keyed object iteration (Const)", []() {
		JSON::node _x("{\"2\":\"ABC\",\"1\":\"DEF\",\"0\":\"GHI\"}");
		const JSON::node& x = _x;
		const char32_t* str[] = {U"GHI", U"DEF", U"ABC"};
		for(auto i = x.begin(); i != x.end(); i++) {
			size_t idx = parse_value<size_t>(utf8::to8(i.key()));
			if(i->as_string() != str[idx])
				return false;
		}
		return true;
	}},{"End-of-object iterator (array)", []() {
		JSON::node x("[]");
		auto i = x.begin();
		try { i.key(); return false; } catch(...) {}
		try { i.index(); return false; } catch(...) {}
		try { *i; return false; } catch(...) {}
		try { i->type(); return false; } catch(...) {}
		try { i++; return false; } catch(...) {}
		try { ++i; return false; } catch(...) {}
		return (i == x.end()) && !(i != x.end());
	}},{"End-of-object iterator (object)", []() {
		JSON::node x("{}");
		auto i = x.begin();
		try { i.key(); return false; } catch(...) {}
		try { i.index(); return false; } catch(...) {}
		try { *i; return false; } catch(...) {}
		try { i->type(); return false; } catch(...) {}
		try { i++; return false; } catch(...) {}
		try { ++i; return false; } catch(...) {}
		return (i == x.end()) && !(i != x.end());
	}},{"End-of-object iterator (array, const)", []() {
		JSON::node _x("[]");
		const JSON::node& x = _x;
		auto i = x.begin();
		try { i.key(); return false; } catch(...) {}
		try { i.index(); return false; } catch(...) {}
		try { *i; return false; } catch(...) {}
		try { i->type(); return false; } catch(...) {}
		try { i++; return false; } catch(...) {}
		try { ++i; return false; } catch(...) {}
		return (i == x.end()) && !(i != x.end());
	}},{"End-of-object iterator (object, const)", []() {
		JSON::node _x("{}");
		const JSON::node& x = _x;
		auto i = x.begin();
		try { i.key(); return false; } catch(...) {}
		try { i.index(); return false; } catch(...) {}
		try { *i; return false; } catch(...) {}
		try { i->type(); return false; } catch(...) {}
		try { i++; return false; } catch(...) {}
		try { ++i; return false; } catch(...) {}
		return (i == x.end()) && !(i != x.end());
	}},{"Iterators to different objects are not equal", []() {
		JSON::node x("[1234]");
		JSON::node y("[1234]");
		return (x.begin() != y.begin());
	}},{"Iterators to different objects are not equal (const)", []() {
		JSON::node _x("[1234]");
		JSON::node _y("[1234]");
		const JSON::node& x = _x;
		const JSON::node& y = _y;
		return (x.begin() != y.begin());
	}},{"Iterators to the same object are equal", []() {
		JSON::node x("[1234]");
		return (x.begin() == x.begin());
	}},{"Iterators to the same object are equal (const)", []() {
		JSON::node _x("[1234]");
		const JSON::node& x = _x;
		return (x.begin() == x.begin());
	}},{"Iterators to different elements are not equal", []() {
		JSON::node x("[1, 2]");
		auto i = x.begin();
		auto j = x.begin();
		j++;
		if(i.key() != U"")
			return false;
		if(j.key() != U"")
			return false;
		if(i == x.end())
			return false;
		if(j == x.end())
			return false;
		return (i != j);
	}},{"Iterators to different elements are not equal (const)", []() {
		JSON::node _x("[1, 2]");
		const JSON::node& x = _x;
		auto i = x.begin();
		auto j = x.begin();
		j++;
		if(i.key() != U"")
			return false;
		if(j.key() != U"")
			return false;
		if(i == x.end())
			return false;
		if(j == x.end())
			return false;
		return (i != j);
	}},{"Iterators to different instances of key are not equal", []() {
		JSON::node x("{\"foo\":1,\"foo\":2}");
		auto i = x.begin();
		auto j = x.begin();
		j++;
		if(i.key() != U"foo")
			return false;
		if(j.key() != U"foo")
			return false;
		if(i == x.end())
			return false;
		if(j == x.end())
			return false;
		return (i != j);
	}},{"Iterators to different instances of key are not equal (const)", []() {
		JSON::node _x("{\"foo\":1,\"foo\":2}");
		const JSON::node& x = _x;
		auto i = x.begin();
		auto j = x.begin();
		j++;
		if(i.key() != U"foo")
			return false;
		if(j.key() != U"foo")
			return false;
		if(i == x.end())
			return false;
		if(j == x.end())
			return false;
		return (i != j);
	}},{"Iterators to different keys are not equal", []() {
		JSON::node x("{\"foo\":1,\"foz\":2}");
		auto i = x.begin();
		auto j = x.begin();
		j++;
		if(i.key() != U"foo")
			return false;
		if(j.key() != U"foz")
			return false;
		if(i == x.end())
			return false;
		if(j == x.end())
			return false;
		return (i != j);
	}},{"Iterators to different keys are not equal (const)", []() {
		JSON::node _x("{\"foo\":1,\"foz\":2}");
		const JSON::node& x = _x;
		auto i = x.begin();
		auto j = x.begin();
		j++;
		if(i.key() != U"foo")
			return false;
		if(j.key() != U"foz")
			return false;
		if(i == x.end())
			return false;
		if(j == x.end())
			return false;
		return (i != j);
	}},{"Serialize multiple", []() {
		JSON::node x("{\"foo\":1,\"foo\":2}");
		return (x.serialize() == "{\"foo\":1,\"foo\":2}");
	}},{"Serialize multiple #2", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		return (x.serialize() == "{\"foo\":1,\"foo\":2,\"foz\":3}");
	}},{"erase index from end", []() {
		JSON::node x("[1,2,3]");
		auto i = x.begin();
		i++;
		i++;
		x.erase(i);
		return (x.serialize() == "[1,2]");
	}},{"erase index from middle", []() {
		JSON::node x("[1,2,3]");
		auto i = x.begin();
		i++;
		x.erase(i);
		return (x.serialize() == "[1,3]");
	}},{"erase index from start", []() {
		JSON::node x("[1,2,3]");
		auto i = x.begin();
		x.erase(i);
		return (x.serialize() == "[2,3]");
	}},{"erase index from middle and iterate", []() {
		JSON::node x("[1,2,3]");
		auto i = x.begin();
		i++;
		i = x.erase(i);
		return (i->as_int() == 3);
	}},{"erase index from end and iterate", []() {
		JSON::node x("[1,2,3]");
		auto i = x.begin();
		i++;
		i++;
		i = x.erase(i);
		return (i == x.end());
	}},{"erase_index past end", []() {
		JSON::node x("[1,2,3]");
		x.erase_index(3);
		return (x.serialize() == "[1,2,3]");
	}},{"erase_index on object", []() {
		JSON::node x("{}");
		x.erase_index(0);
		return false;
	}, JSON::ERR_NOT_AN_ARRAY},{"erase index on boolean", []() {
		JSON::node x("[3]");
		auto i = x.begin();
		x.set(JSON::boolean, false);
		x.erase(i);
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"erase index from wrong thing", []() {
		JSON::node x("[1,2,3]");
		JSON::node y("[1,2,3]");
		auto i = x.begin();
		y.erase(i);
		return false;
	}, JSON::ERR_WRONG_OBJECT},{"erase field on boolean", []() {
		JSON::node x("false");
		x.erase_field(U"foo");
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"erase field all on boolean", []() {
		JSON::node x("false");
		x.erase_field_all(U"foo");
		return false;
	}, JSON::ERR_NOT_AN_OBJECT},{"erase key", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		auto i = x.begin();
		i++;
		x.erase(i);
		return (x.serialize() == "{\"foo\":1,\"foz\":3}");
	}},{"erase_field_all", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.erase_field_all(U"foo");
		return (x.serialize() == "{\"foz\":3}");
	}},{"erase_field_all (does not exist)", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.erase_field_all(U"fou");
		return (x.serialize() == "{\"foo\":1,\"foo\":2,\"foz\":3}");
	}},{"erase_field (does not exist)", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.erase_field(U"fou");
		return (x.serialize() == "{\"foo\":1,\"foo\":2,\"foz\":3}");
	}},{"erase_field (first)", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.erase_field(U"foo");
		return (x.serialize() == "{\"foo\":2,\"foz\":3}");
	}},{"erase_field (second)", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.erase_field(U"foo", 1);
		return (x.serialize() == "{\"foo\":1,\"foz\":3}");
	}},{"erase_field (subsequent)", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.erase_field(U"foo", 2);
		return (x.serialize() == "{\"foo\":1,\"foo\":2,\"foz\":3}");
	}},{"erase key and iterate", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		auto i = x.begin();
		i++;
		i = x.erase(i);
		return (i.key() == U"foz" && i->as_int() == 3);
	}},{"erase key (disappears)", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		auto i = x.begin();
		i++;
		i++;
		x.erase(i);
		return (x.serialize() == "{\"foo\":1,\"foo\":2}");
	}},{"clear object", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		x.clear();
		return (x.serialize() == "{}");
	}},{"clear array", []() {
		JSON::node x("[1,2,3]");
		x.clear();
		return (x.serialize() == "[]");
	}},{"clear boolean", []() {
		JSON::node x("false");
		x.clear();
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"iterator on deleted index", []() {
		JSON::node x("[1,2,3]");
		auto i = x.begin();
		i++;
		i++;
		x.erase_index(2);
		*i;
		return false;
	}, JSON::ERR_ITERATOR_DELETED},{"iterator on deleted index (const)", []() {
		JSON::node _x("[1,2,3]");
		const JSON::node& x = _x;
		auto i = x.begin();
		i++;
		i++;
		_x.erase_index(2);
		*i;
		return false;
	}, JSON::ERR_ITERATOR_DELETED},{"iterator on deleted key", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		auto i = x.begin();
		i++;
		i++;
		x.erase_field(U"foz");
		*i;
		return false;
	}, JSON::ERR_ITERATOR_DELETED},{"iterator on deleted key (const)", []() {
		JSON::node _x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		const JSON::node& x = _x;
		auto i = x.begin();
		i++;
		i++;
		_x.erase_field(U"foz");
		*i;
		return false;
	}, JSON::ERR_ITERATOR_DELETED},{"iterator on deleted key-instance", []() {
		JSON::node x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		auto i = x.begin();
		i++;
		x.erase_field(U"foo",0);
		*i;
		return false;
	}, JSON::ERR_ITERATOR_DELETED},{"iterator on deleted key-instance (const)", []() {
		JSON::node _x("{\"foo\":1,\"foo\":2,\"foz\":3}");
		const JSON::node& x = _x;
		auto i = x.begin();
		i++;
		_x.erase_field(U"foo",0);
		*i;
		return false;
	}, JSON::ERR_ITERATOR_DELETED},{"Illegal character #1", []() {
		JSON::node x("\"\\uFFFE\"");
		return false;
	}, JSON::ERR_ILLEGAL_CHARACTER},{"Illegal character #2", []() {
		JSON::node x("\"\\uFFFF\"");
		return false;
	}, JSON::ERR_ILLEGAL_CHARACTER},{"Illegal character #3", []() {
		JSON::node x("\"\\uD87F\\uDFFE\"");
		return false;
	}, JSON::ERR_ILLEGAL_CHARACTER},{"U+F800", []() {
		JSON::node x("\"\\uF800\"");
		return true;
	}},{"Special escapes", []() {
		JSON::node x("\"\\b\\f\\n\\r\\t\\\"\\\\\\u007F\u0080\"");
		return (x.serialize() == "\"\\b\\f\\n\\r\\t\\\"\\\\\x7F\xc2\x80\"");
	}},{"\\v is not whitespace", []() {
		JSON::node x(" [\v123]");
		return false;
	}, JSON::ERR_UNKNOWN_CHARACTER},{"null==null", []() -> bool {
		return JSON::n() == JSON::n();
	}},{"false==false", []() -> bool {
		return JSON::b(false) == JSON::b(false);
	}},{"true==true", []() -> bool {
		return JSON::b(true) == JSON::b(true);
	}},{"null!=false", []() -> bool {
		return JSON::b(false) != JSON::n();
	}},{"\"foo\"==\"foo\"", []() -> bool {
		return JSON::s("foo") == JSON::s("foo");
	}},{"\"foo\"==U\"foo\"", []() -> bool {
		return JSON::s("foo") == JSON::s(U"foo");
	}},{"\"foo\"!=\"bar\"", []() -> bool {
		return JSON::s("foo") != JSON::s("bar");
	}},{"1==1", []() -> bool {
		return JSON::i(1) == JSON::i(1);
	}},{"1==(uint)1", []() -> bool {
		return JSON::i(1) == JSON::u(1U);
	}},{"1==1.0", []() -> bool {
		return JSON::i(1) == JSON::f(1.0);
	}},{"-1.25!=-1", []() -> bool {
		return JSON::f(-1.25) != JSON::i(-1);
	}},{"1.25!=1U", []() -> bool {
		return JSON::f(1.25) != JSON::u(1);
	}},{"1.25!=1.5", []() -> bool {
		return JSON::f(1.25) != JSON::f(1.5);
	}},{"(uint)1==1.0", []() -> bool {
		return JSON::u(1) == JSON::f(1.0);
	}},{"1!=2", []() -> bool {
		return JSON::i(1) != JSON::i(2);
	}},{"-1!=0xFFFFFFFFFFFFFFFFULL", []() -> bool {
		return JSON::i(-1) != JSON::u(0xFFFFFFFFFFFFFFFFULL);
	}},{"0xFFFFFFFFFFFFFFFFULL!=-1", []() -> bool {
		return JSON::u(0xFFFFFFFFFFFFFFFFULL) != JSON::i(-1);
	}},{"1!=\1\1", []() -> bool {
		return JSON::i(1) != JSON::s("1");
	}},{"false!=0", []() -> bool {
		return JSON::b(false) != JSON::i(0);
	}},{"true!=1", []() -> bool {
		return JSON::b(true) != JSON::i(1);
	}},{"{}!=[]", []() -> bool {
		return JSON::node("{}") != JSON::node("[]");
	}},{"{}=={}", []() -> bool {
		return JSON::node("{}") == JSON::node("{}");
	}},{"[]==[]", []() -> bool {
		return JSON::node("[]") == JSON::node("[]");
	}},{"[1,2,3]==[1,2,3]", []() -> bool {
		return JSON::node("[1,2,3]") == JSON::node("[1,2,3]");
	}},{"[1,2,3]!=[1,2,4]", []() -> bool {
		return JSON::node("[1,2,3]") != JSON::node("[1,2,4]");
	}},{"[1,2,\"hi\"]==[1,2,\"hi\"]", []() -> bool {
		return JSON::node("[1,2,\"hi\"]") == JSON::node("[1,2,\"hi\"]");
	}},{"[1,2,[4,5]]==[1,2,[4,5]]", []() -> bool {
		return JSON::node("[1,2,[4,5]]") == JSON::node("[1,2,[4,5]]");
	}},{"[1,2,[4,6]]!=[1,2,[4,5]]", []() -> bool {
		return JSON::node("[1,2,[4,6]]") != JSON::node("[1,2,[4,5]]");
	}},{"[1,2,3]!=[1,2]", []() -> bool {
		return JSON::node("[1,2,3]") != JSON::node("[1,2]");
	}},{"[1,2]!=[1,2,3]", []() -> bool {
		return JSON::node("[1,2]") != JSON::node("[1,2,3]");
	}},{"{\"foo\": 1}=={\"foo\": 1}", []() -> bool {
		return JSON::node("{\"foo\": 1}") == JSON::node("{\"foo\": 1}");
	}},{"{\"foo\": [1,2]}=={\"foo\": [1,2]}", []() -> bool {
		return JSON::node("{\"foo\": [1,2]}") == JSON::node("{\"foo\": [1,2]}");
	}},{"{\"foo\": [1,2]}!={\"foo\": [1,3]}", []() -> bool {
		return JSON::node("{\"foo\": [1,2]}") != JSON::node("{\"foo\": [1,3]}");
	}},{"{\"foo\": 1}!={\"bar\": 1}", []() -> bool {
		return JSON::node("{\"foo\": 1}") != JSON::node("{\"bar\": 1}");
	}},{"{\"foo\": 1}!={\"foo\": 1, \"x\": 2}", []() -> bool {
		return JSON::node("{\"foo\": 1}") != JSON::node("{\"foo\": 1, \"x\": 2}");
	}},{"{\"foo\": 1, \"x\": 2}!={\"foo\": 1}", []() -> bool {
		return JSON::node("{\"foo\": 1, \"x\": 2}") != JSON::node("{\"foo\": 1}");
	}},{"{\"foo\": 1}!={\"foo\": 2}", []() -> bool {
		return JSON::node("{\"foo\": 1}") != JSON::node("{\"foo\": 2}");
	}},{"{\"foo\": 1,\"foo\": 2}=={\"foo\": 1,\"foo\": 2}", []() -> bool {
		return JSON::node("{\"foo\": 1,\"foo\": 2}") == JSON::node("{\"foo\": 1,\"foo\": 2}");
	}},{"{\"foo\": 1,\"foo\": 2}!={\"foo\": 2,\"foo\": 1}", []() -> bool {
		return JSON::node("{\"foo\": 1,\"foo\": 2}") != JSON::node("{\"foo\": 2,\"foo\": 1}");
	}},{"{\"foo\": 1,\"foo\": 2}!={\"foo\": 1}", []() -> bool {
		return JSON::node("{\"foo\": 1,\"foo\": 2}") != JSON::node("{\"foo\": 1}");
	}},{"{\"foo\": 1}!={\"foo\": 1,\"foo\": 2}", []() -> bool {
		return JSON::node("{\"foo\": 1}") != JSON::node("{\"foo\": 1,\"foo\": 2}");
	}},{"Escape U+0012", []() {
		JSON::node x("\"\\u0012\"");
		return (x.serialize() == "\"\\u0012\"");
	}},{"Escape U+001F", []() {
		JSON::node x("\"\\u001F\"");
		return (x.serialize() == "\"\\u001f\"");
	}},{"Compare itself", []() {
		JSON::node x("\"\\u001F\"");
		return x == x;
	}},{"Boolean not null", []() {
		JSON::node x("false");
		return x.type() != JSON::null;
	}},{"Insert_node to empty array with -", []() {
		JSON::node x("[]");
		x.insert_node("-", JSON::i(1));
		return x.serialize() == "[1]";
	}},{"Insert_node to empty array with 0", []() {
		JSON::node x("[]");
		x.insert_node("0", JSON::i(1));
		return x.serialize() == "[1]";
	}},{"Insert_node to end of non-empty array with -", []() {
		JSON::node x("[1,2,3]");
		x.insert_node("-", JSON::i(4));
		return x.serialize() == "[1,2,3,4]";
	}},{"Insert_node to end of non-empty array with index", []() {
		JSON::node x("[1,2,3]");
		x.insert_node("3", JSON::i(4));
		return x.serialize() == "[1,2,3,4]";
	}},{"Insert_node to middle of non-empty array", []() {
		JSON::node x("[1,2,4]");
		x.insert_node("2", JSON::i(3));
		return x.serialize() == "[1,2,3,4]";
	}},{"Insert_node to middle of nested array", []() {
		JSON::node x("[[1,2,4]]");
		x.insert_node("0/2", JSON::i(3));
		return x.serialize() == "[[1,2,3,4]]";
	}},{"Insert_node to invalid array", []() {
		JSON::node x("[[1,2,4]]");
		x.insert_node("1/2", JSON::i(3));
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Insert_node to boolean", []() {
		JSON::node x("false");
		x.insert_node("0", JSON::i(4));
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Insert_node to empty object", []() {
		JSON::node x("{}");
		x.insert_node("foo",JSON::i(1));
		return x.serialize() == "{\"foo\":1}";
	}},{"Insert_node to non-empty object", []() {
		JSON::node x("{\"bar\":2}");
		x.insert_node("foo", JSON::i(1));
		return x.serialize() == "{\"bar\":2,\"foo\":1}";
	}},{"Insert_node to replacing entry in object", []() {
		JSON::node x("{\"foo\":2}");
		x.insert_node("foo", JSON::i(1));
		return x.serialize() == "{\"foo\":1}";
	}},{"Insert_node to replacing entries in object", []() {
		JSON::node x("{\"foo\":1,\"foo\":2}");
		x.insert_node("foo", JSON::i(3));
		return x.serialize() == "{\"foo\":3,\"foo\":2}";
	}},{"Insert_node returns object #1", []() {
		JSON::node x("{\"bar\":2}");
		JSON::node& y = x.insert_node("foo", JSON::s("TEST"));
		return y.serialize() == "\"TEST\"";
	}},{"Insert_node returns object #2", []() {
		JSON::node x("[]");
		JSON::node& y = x.insert_node("0", JSON::s("TEST"));
		return y.serialize() == "\"TEST\"";
	}},{"Insert_node returns object #3", []() {
		JSON::node x("[]");
		JSON::node& y = x.insert_node("-", JSON::s("TEST"));
		return y.serialize() == "\"TEST\"";
	}},{"Insert_node returns object #4", []() {
		JSON::node x("[1]");
		JSON::node& y = x.insert_node("-", JSON::s("TEST"));
		return y.serialize() == "\"TEST\"";
	}},{"Insert_node to index too large", []() {
		JSON::node x("[[1,2,4]]");
		x.insert_node("0/5", JSON::i(5));
		return false;
	}, JSON::ERR_POINTER_BAD_APPEND},{"delete_node to boolean", []() {
		JSON::node x("false");
		x.delete_node("0");
		return false;
	}, JSON::ERR_NOT_ARRAY_NOR_OBJECT},{"Delete_node to middle of non-empty array", []() {
		JSON::node x("[1,2,3,4]");
		x.delete_node("2");
		return x.serialize() == "[1,2,4]";
	}},{"Delete_node to middle of nested array", []() {
		JSON::node x("[[1,2,3,4]]");
		x.delete_node("0/2");
		return x.serialize() == "[[1,2,4]]";
	}},{"Delete_node to invalid array", []() {
		JSON::node x("[[1,2,3,4]]");
		x.delete_node("1/2");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Delete_node returns object", []() {
		JSON::node x("{\"bar\":\"test!\"}");
		JSON::node y = x.delete_node("bar");
		return y.serialize() == "\"test!\"";
	}},{"Delete_node returns object #2", []() {
		JSON::node x("[1,\"test!\"]");
		JSON::node y = x.delete_node("1");
		return y.serialize() == "\"test!\"";
	}},{"Delete_node to deleting entry in object", []() {
		JSON::node x("{\"bar\":1,\"foo\":2}");
		x.delete_node("foo");
		return x.serialize() == "{\"bar\":1}";
	}},{"Delete_node to deleting entries in object", []() {
		JSON::node x("{\"bar\":1,\"foo\":2,\"foo\":3}");
		x.delete_node("foo");
		return x.serialize() == "{\"bar\":1}";
	}},{"Delete_node to invalid key in object", []() {
		JSON::node x("{\"bar\":1,\"foo\":2}");
		x.delete_node("foz");
		return false;
	}, JSON::ERR_KEY_INVALID},{"Delete_node to invalid index", []() {
		JSON::node x("[1,2,3,4]");
		x.delete_node("4");
		return false;
	}, JSON::ERR_INDEX_INVALID},{"Delete_node to index -", []() {
		JSON::node x("[1,2,3,4]");
		x.delete_node("-");
		return false;
	}, JSON::ERR_POINTER_BAD_APPEND},{"operator[] on bad index", []() {
		JSON::node x("[1,2,3,4]");
		x["f"];
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"insert_node on bad index", []() {
		JSON::node x("[1,2,3,4]");
		x.insert_node("f", JSON::b(false));
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"delete_node on bad index", []() {
		JSON::node x("[1,2,3,4]");
		x.delete_node("f");
		return false;
	}, JSON::ERR_POINTER_BAD_INDEX},{"patching with an object", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("{}");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"Duplicate op", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"test\",\"path\":\"3\",\"value\":4,\"op\":\"add\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"simple test", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"test\",\"path\":\"3\",\"value\":4}]");
		JSON::node z = x.patch(y);
		return x == z;
	}},{"simple failed test", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"test\",\"path\":\"3\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_TEST_FAILED},{"test dupe path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"test\",\"path\":\"3\",\"path\":\"2\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"test number path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"test\",\"path\":3,\"value\":4}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"test dupe value", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"test\",\"path\":\"3\",\"value\":\"2\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"simple remove", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"remove\",\"path\":\"2\"}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[1,2,4]";
	}},{"remove dupe path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"remove\",\"path\":\"2\",\"path\":\"3\"}]");
		JSON::node z = x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"remove numeric path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"remove\",\"path\":2}]");
		JSON::node z = x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"simple add", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"add\",\"path\":\"-\",\"value\":5}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[1,2,3,4,5]";
	}},{"add dupe path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"add\",\"path\":\"3\",\"path\":\"2\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"add number path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"add\",\"path\":3,\"value\":4}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"add dupe value", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"add\",\"path\":\"3\",\"value\":\"2\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"simple replace", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"replace\",\"path\":\"3\",\"value\":5}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[1,2,3,5]";
	}},{"Replace nonexistent", []() {
		JSON::node x("{\"foo\":5}");
		JSON::node y("[{\"op\":\"replace\",\"path\":\"foz\",\"value\":5}]");
		JSON::node z = x.patch(y);
		return false;
	}, JSON::ERR_KEY_INVALID},{"replace dupe path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"replace\",\"path\":\"3\",\"path\":\"2\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"replace number path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"replace\",\"path\":3,\"value\":4}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"replace dupe value", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"replace\",\"path\":\"3\",\"value\":\"2\",\"value\":3}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"simple move", []() {
		JSON::node x("[[1,2,3,4],[]]");
		JSON::node y("[{\"op\":\"move\",\"from\":\"0/1\",\"path\":\"1/-\"}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[[1,3,4],[2]]";
	}},{"move within array", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"move\",\"from\":\"1\",\"path\":\"3\"}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[1,3,4,2]";
	}},{"move nonexistent", []() {
		JSON::node x("{\"foo\":5}");
		JSON::node y("[{\"op\":\"move\",\"path\":\"for\",\"from\":\"foz\"}]");
		JSON::node z = x.patch(y);
		return false;
	}, JSON::ERR_KEY_INVALID},{"move dupe path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"move\",\"path\":\"3\",\"path\":\"2\",\"from\":\"3\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"move number path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"move\",\"path\":3,\"from\":\"4\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"move dupe from", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"move\",\"path\":\"3\",\"from\":\"2\",\"from\":\"3\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"move number from", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"move\",\"path\":\"3\",\"from\":4}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"move illegal", []() {
		JSON::node x("{\"foo\":{}}");
		JSON::node y("[{\"op\":\"move\",\"path\":\"foo/bar\",\"from\":\"foo\"}]");
		JSON::node z = x.patch(y);
		return false;
	}, JSON::ERR_PATCH_ILLEGAL_MOVE},{"move self", []() {
		JSON::node x("{\"foo\":{}}");
		JSON::node y("[{\"op\":\"move\",\"path\":\"foo\",\"from\":\"foo\"}]");
		JSON::node z = x.patch(y);
		return x == z;
	}},{"simple copy", []() {
		JSON::node x("[[1,2,3,4],[]]");
		JSON::node y("[{\"op\":\"copy\",\"from\":\"0/1\",\"path\":\"1/-\"}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[[1,2,3,4],[2]]";
	}},{"copy within array", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"copy\",\"from\":\"1\",\"path\":\"3\"}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[1,2,3,2,4]";
	}},{"copy nonexistent", []() {
		JSON::node x("{\"foo\":5}");
		JSON::node y("[{\"op\":\"copy\",\"path\":\"for\",\"from\":\"foz\"}]");
		JSON::node z = x.patch(y);
		return false;
	}, JSON::ERR_KEY_INVALID},{"copy dupe path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"copy\",\"path\":\"3\",\"path\":\"2\",\"from\":\"3\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"copy number path", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"copy\",\"path\":3,\"from\":\"4\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"copy dupe from", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"copy\",\"path\":\"3\",\"from\":\"2\",\"from\":\"3\"}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"copy number from", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"copy\",\"path\":\"3\",\"from\":4}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"copy self", []() {
		JSON::node x("{\"foo\":{}}");
		JSON::node y("[{\"op\":\"copy\",\"path\":\"foo\",\"from\":\"foo\"}]");
		JSON::node z = x.patch(y);
		return x == z;
	}},{"copy idential path", []() {
		JSON::node x("[1]");
		JSON::node y("[{\"op\":\"copy\",\"path\":\"0\",\"from\":\"0\"}]");
		JSON::node z = x.patch(y);
		return z.serialize() == "[1,1]";
	}},{"unknown patch op", []() {
		JSON::node x("[1,2,3,4]");
		JSON::node y("[{\"op\":\"foo\",\"path\":\"3\",\"from\":\"4\",\"value\":6}]");
		x.patch(y);
		return false;
	}, JSON::ERR_PATCH_BAD},{"Basic type_of", []() {
		JSON::node x("[1,false,null,{\"foo\":\"bar\"}]");
		if(x.type_of("0") != JSON::number) return false;
		if(x.type_of("1") != JSON::boolean) return false;
		if(x.type_of("2") != JSON::null) return false;
		if(x.type_of("3") != JSON::object) return false;
		if(x.type_of("3/foo") != JSON::string) return false;
		if(x.type_of("4") != JSON::none) return false;
		if(x.type_of("foo") != JSON::none) return false;
		return true;
	}},{"Basic type_of_indirect", []() {
		JSON::node x("{\"objects\":{\"foo\":\"bar\",\"baz\":true},\"pointers\":[false,\"objects/foo\", "
			"\"objects/baz\",\"objects/enone\"]}");
		if(x.type_of_indirect("objects/baz") != JSON::boolean) return false;
		if(x.type_of_indirect("pointers/0") != JSON::boolean) return false;
		if(x.type_of_indirect("pointers/1") != JSON::string) return false;
		if(x.type_of_indirect("pointers/2") != JSON::boolean) return false;
		if(x.type_of_indirect("pointers/3") != JSON::none) return false;
		if(x.type_of_indirect("pointers/4") != JSON::none) return false;
		return true;
	}},{"Basic resolve_indirect", []() {
		JSON::node x("{\"objects\":{\"foo\":\"bar\",\"baz\":true},\"pointers\":[false,\"objects/foo\", "
			"\"objects/baz\",\"objects/enone\"]}");
		if(x.resolve_indirect("objects/baz") != "objects/baz") return false;
		if(x.resolve_indirect("pointers/0") != "pointers/0") return false;
		if(x.resolve_indirect("pointers/1") != "objects/foo") return false;
		if(x.resolve_indirect("pointers/2") != "objects/baz") return false;
		if(x.resolve_indirect("pointers/3") != "objects/enone") return false;
		if(x.resolve_indirect("pointers/4") != "pointers/4") return false;
		return true;
	}},{"Basic pointer obj #1", []() {
		JSON::pointer p;
		p.field_inplace("foo").field_inplace("bar");
		return p.as_string8() == "foo/bar";
	}},{"Basic pointer obj #2", []() {
		JSON::pointer p("foo/bar");
		return p.as_string() == U"foo/bar";
	}},{"Basic pointer obj #3", []() {
		JSON::pointer p(U"foo/bar");
		return p.as_string8() == "foo/bar";
	}},{"Basic pointer obj #4", []() {
		JSON::pointer p;
		p = p.index(5);
		p = p.index(3);
		return p.as_string8() == "5/3";
	}},{"Basic pointer obj #5", []() {
		JSON::pointer p;
		p.index_inplace(5);
		p.index_inplace(3);
		return p.as_string8() == "5/3";
	}},{"Basic pointer obj #6", []() {
		JSON::pointer p;
		p.index_inplace(5);
		p.index_inplace(3);
		p.pastend_inplace();
		return p.as_string8() == "5/3/-";
	}},{"Basic pointer obj #7", []() {
		JSON::pointer p;
		p = p.index(5);
		p = p.index(3);
		p = p.pastend();
		return p.as_string8() == "5/3/-";
	}},{"Basic pointer obj #8", []() {
		JSON::pointer p;
		p = p.field("foo").field("bar");
		return p.as_string8() == "foo/bar";
	}},{"Basic pointer obj #9", []() {
		JSON::pointer p;
		p = p.field("~").field("/");
		return p.as_string8() == "~0/~1";
	}},{"Basic pointer obj #10", []() {
		JSON::pointer p;
		p = p.field("foo").field("bar").field("baz").remove();
		return p.as_string8() == "foo/bar";
	}},{"Basic pointer obj #11", []() {
		JSON::pointer p;
		p.field_inplace("foo").field_inplace("bar").field_inplace("baz").remove_inplace();
		return p.as_string8() == "foo/bar";
	}},{"Basic pointer obj #12", []() {
		JSON::pointer p("baz/zot");
		std::string x = (stringfmt() << p).str();
		return x == "baz/zot";
	}},{"Basic pointer obj #13", []() {
		JSON::pointer p("baz/zot");
		std::basic_ostringstream<char32_t> y;
		y << p;
		return y.str() == U"baz/zot";
	}},{"Basic pointer obj #14", []() {
		JSON::pointer p;
		p = p.field("foo").remove();
		return p.as_string8() == "";
	}},{"Basic pointer obj #15", []() {
		JSON::pointer p;
		p.field_inplace("foo").remove_inplace();
		return p.as_string8() == "";
	}},
};

void run_test(unsigned i, size_t& total, size_t& pass, size_t& fail)
{
	try {
		std::cout << "#" << (i + 1) << ": " << tests[i].title << "..." << std::flush;
		if(tests[i].dotest() && !tests[i].expect) {
			std::cout << "\e[32mPASS\e[0m" << std::endl;
			pass++;
		} else {
			std::cout << "\e[31mFAIL\e[0m" << std::endl;
			fail++;
		}
	} catch(JSON::error& e) {
		if(e.get_code() == tests[i].expect) {
			std::cout << "\e[32mPASS\e[0m" << std::endl;
			pass++;
		} else {
			std::cout << "\e[31mERR: " << e.what() << "\e[0m" << std::endl;
			fail++;
		}
	} catch(std::exception& e) {
		std::cout << "\e[31mERR: " << e.what() << "\e[0m" << std::endl;
		fail++;
	}
	total++;
}

int main(int argc, char** argv)
{
	size_t total = 0;
	size_t pass = 0;
	size_t fail = 0;
	if(argc == 1)
		for(size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
			run_test(i, total, pass, fail);
	else
		for(int i = 1; i < argc; i++)
			run_test(parse_value<unsigned>(argv[i]) - 1, total, pass, fail);
	std::cout << "Total: " << total << " Pass: " << pass << " Fail: " << fail << std::endl;
	return (fail != 0);
}
