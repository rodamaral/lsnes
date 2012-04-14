#include "lua/internal.hpp"
#include "lua/simd.hpp"
#include "library/minmax.hpp"
#include <algorithm>
#include <cmath>

lua_simdvector::lua_simdvector(size_t size)
{
	data.resize(size);
	for(size_t i = 0; i < size; i++)
		data[i] = 0;
}

namespace
{
	function_ptr_luafun simd_new("simd.new", [](lua_State* LS, const std::string& fname) -> int {
		size_t s = get_numeric_argument<size_t>(LS, 1, fname.c_str());
		lua_simdvector* v = lua_class<lua_simdvector>::create(LS, s);
		return 1;
	});

	template<typename T>
	class lua_store_op : public lua_function
	{
	public:
		lua_store_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<T> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			size_t p = get_numeric_argument<size_t>(L, 2, fname.c_str());
			uint8_t v = get_numeric_argument<T>(L, 3, fname.c_str());
			if(p1.elts > p)
				p1.data[p] = v;
			return 0;
		}
	};

	template<typename T>
	class lua_load_op : public lua_function
	{
	public:
		lua_load_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<T> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			size_t p = get_numeric_argument<size_t>(L, 2, fname.c_str());
			if(p1.elts > p) {
				lua_pushnumber(L, p1.data[p]);
				return 1;
			} else
				return 0;
		}
	};

	template<typename T>
	class lua_count_op : public lua_function
	{
	public:
		lua_count_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<T> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			lua_pushnumber(L, p1.elts);
			return 1;
		}
	};

	template<typename Td, typename Ts, Td (*fn)(Ts a)>
	class lua_unary_op : public lua_function
	{
	public:
		lua_unary_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<Td> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			if(lua_class<lua_simdvector>::is(L, 2)) {
				lua_simdvector_doverlay<Ts> p2(lua_class<lua_simdvector>::get(L, 2, fname.c_str()));
				size_t e = min(p1.elts, p2.elts);
				for(size_t i = 0; i < e; i++)
					p1.data[i] = fn(p2.data[i]);
			} else if(lua_isnumber(L, 2)) {
				Ts v = get_numeric_argument<Ts>(L, 2, fname.c_str());
				size_t e = p1.elts;
				for(size_t i = 0; i < e; i++)
					p1.data[i] = fn(v);
			} else {
				lua_pushstring(L, "Expected SIMDVECTOR or numeric as argument 2 for unary simd op");
				lua_error(L);
			}
			return 0;
		}
	};

	template<typename Td, typename Ts1, typename Ts2, Td (*fn)(Ts1 a, Ts2 b)>
	class lua_binary_op : public lua_function
	{
	public:
		lua_binary_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<Td> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			if(lua_class<lua_simdvector>::is(L, 2) && lua_class<lua_simdvector>::is(L, 3)) {
				lua_simdvector_doverlay<Ts1> p2(lua_class<lua_simdvector>::get(L, 2, fname.c_str()));
				lua_simdvector_doverlay<Ts2> p3(lua_class<lua_simdvector>::get(L, 3, fname.c_str()));
				size_t e = min(p1.elts, min(p2.elts, p3.elts));
				for(size_t i = 0; i < e; i++)
					p1.data[i] = fn(p2.data[i], p3.data[i]);
			} else if(lua_class<lua_simdvector>::is(L, 2) && lua_isnumber(L, 3)) {
				lua_simdvector_doverlay<Ts1> p2(lua_class<lua_simdvector>::get(L, 2, fname.c_str()));
				Ts2 p3 = get_numeric_argument<Ts2>(L, 3, fname.c_str());
				size_t e = min(p1.elts, p2.elts);
				for(size_t i = 0; i < e; i++)
					p1.data[i] = fn(p2.data[i], p3);
			} else if(lua_class<lua_simdvector>::is(L, 3) && lua_isnumber(L, 2)) {
				Ts1 p2 = get_numeric_argument<Ts1>(L, 2, fname.c_str());
				lua_simdvector_doverlay<Ts2> p3(lua_class<lua_simdvector>::get(L, 3, fname.c_str()));
				size_t e = min(p1.elts, p3.elts);
				for(size_t i = 0; i < e; i++)
					p1.data[i] = fn(p2, p3.data[i]);
			} else if(lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
				Ts1 p2 = get_numeric_argument<Ts1>(L, 2, fname.c_str());
				Ts2 p3 = get_numeric_argument<Ts2>(L, 3, fname.c_str());
				size_t e = p1.elts;
				for(size_t i = 0; i < e; i++)
					p1.data[i] = fn(p2, p3);
			} else {
				lua_pushstring(L, "Expected SIMDVECTOR or numeric as arguments 2&3 for binary "
					"simd op");
				lua_error(L);
			}
			return 0;
		}
	};

	template<typename T, T (*fn)(T a, T b), T(*init)()>
	class lua_cascade_op : public lua_function
	{
	public:
		lua_cascade_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			T v = init();
			lua_simdvector_doverlay<T> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			size_t e = p1.elts;
			for(size_t i = 0; i < e; i++)
				v = fn(v, p1.data[i]);
			lua_pushnumber(L, v);
			return 1;
		}
	};

	template<typename Ti, typename Td>
	struct idxcompare_helper
	{
		idxcompare_helper(Td* _arr) { arr = _arr; }
		bool operator()(Ti a, Ti b) { return arr[a] < arr[b]; }
		Td* arr;
	};
	
	template<typename Ti, typename Td>
	class lua_isort_op : public lua_function
	{
	public:
		lua_isort_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<Ti> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			lua_simdvector_doverlay<Td> p2(lua_class<lua_simdvector>::get(L, 2, fname.c_str()));
			size_t e = min(p1.elts, p2.elts);
			for(size_t i = 0; i < e; i++)
				p1.data[i] = i;
			std::sort(p1.data, p1.data + p1.elts, idxcompare_helper<Ti, Td>(&p2.data[0]));
			return 0;
		}
	};

	template<typename Ti, typename Td>
	class lua_ipermute_op : public lua_function
	{
	public:
		lua_ipermute_op(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			lua_simdvector_doverlay<Td> p1(lua_class<lua_simdvector>::get(L, 1, fname.c_str()));
			lua_simdvector_doverlay<Ti> p2(lua_class<lua_simdvector>::get(L, 2, fname.c_str()));
			lua_simdvector_doverlay<Td> p3(lua_class<lua_simdvector>::get(L, 3, fname.c_str()));
			size_t e = min(p1.elts, p2.elts);
			for(size_t i = 0; i < e; i++)
				if(p2.data[i] >= p3.elts)
					p1.data[i] = 0;
				else
					p1.data[i] = p3.data[p2.data[i]];
			return 0;
		}
	};

	function_ptr_luafun simd_copy("simd.copy", [](lua_State* LS, const std::string& fname) -> int {
		lua_simdvector_doverlay<uint8_t> d(lua_class<lua_simdvector>::get(LS, 1, fname.c_str()));
		size_t doff = get_numeric_argument<size_t>(LS, 2, fname.c_str());
		lua_simdvector_doverlay<uint8_t> s(lua_class<lua_simdvector>::get(LS, 3, fname.c_str()));
		size_t soff = get_numeric_argument<size_t>(LS, 4, fname.c_str());
		size_t len = get_numeric_argument<size_t>(LS, 5, fname.c_str());
		if(doff >= d.elts || soff >= s.elts)
			return 0;
		len = min(len, d.elts - doff);
		len = min(len, s.elts - soff);
		memcpy(&d.data[doff], &s.data[soff], len);
		return 0;
	});


	template<typename T> inline T copy(T a) { return a; }
	template<typename T> inline T neg(T a) { return -a; }
	template<typename T> inline T add(T a, T b) { return a + b; }
	template<typename T> inline T sub(T a, T b) { return a - b; }
	template<typename T> inline T mul(T a, T b) { return a * b; }
	template<typename T> inline T div(T a, T b) { return a / b; }
	template<typename T> inline T rem(T a, T b) { return a % b; }
	template<typename T> inline T lshift(T a, T b) { return a << b; }
	template<typename T> inline T rshift(T a, T b) { return a >> b; }
	template<typename T> inline T _xor(T a, T b) { return a ^ b; }
	template<typename T> inline T _and(T a, T b) { return a & b; }
	template<typename T> inline T _or(T a, T b) { return a | b; }
	inline float cplxabsf(float a, float b) { return sqrtf(a * a + b * b); }
	inline double cplxabs(double a, double b) { return sqrt(a * a + b * b); }
	template<typename T> inline T _not(T a) { return ~a; }
	template<typename T> inline T zero() { return 0; }
	template<typename T> inline T one() { return 1; }
	template<typename Td, typename Ts> inline Td convert(Ts a) { return (Td)a; }
	template<typename T> inline uint8_t lt(T a, T b) { return a < b; }
	template<typename T> inline uint8_t le(T a, T b) { return a <= b; }
	template<typename T> inline uint8_t eq(T a, T b) { return a == b; }
	template<typename T> inline uint8_t ne(T a, T b) { return a != b; }
	template<typename T> inline uint8_t ge(T a, T b) { return a >= b; }
	template<typename T> inline uint8_t gt(T a, T b) { return a > b; }
	
	lua_count_op<uint8_t> simd_length_b("simd.length_b");
	lua_count_op<uint16_t> simd_length_w("simd.length_w");
	lua_count_op<uint32_t> simd_length_d("simd.length_d");
	lua_count_op<uint64_t> simd_length_q("simd.length_q");
	lua_count_op<float> simd_length_s("simd.length_s");
	lua_count_op<double> simd_length_f("simd.length_f");
	lua_store_op<int8_t> simd_store_sb("simd.store_sb");
	lua_store_op<uint8_t> simd_store_ub("simd.store_ub");
	lua_store_op<int16_t> simd_store_sw("simd.store_sw");
	lua_store_op<uint16_t> simd_store_uw("simd.store_uw");
	lua_store_op<int32_t> simd_store_sd("simd.store_sd");
	lua_store_op<uint32_t> simd_store_ud("simd.store_ud");
	lua_store_op<int64_t> simd_store_sq("simd.store_sq");
	lua_store_op<uint64_t> simd_store_uq("simd.store_uq");
	lua_store_op<float> simd_store_s("simd.store_s");
	lua_store_op<double> simd_store_f("simd.store_f");
	lua_load_op<int8_t> simd_load_sb("simd.load_sb");
	lua_load_op<uint8_t> simd_load_ub("simd.load_ub");
	lua_load_op<int16_t> simd_load_sw("simd.load_sw");
	lua_load_op<uint16_t> simd_load_uw("simd.load_uw");
	lua_load_op<int32_t> simd_load_sd("simd.load_sd");
	lua_load_op<uint32_t> simd_load_ud("simd.load_ud");
	lua_load_op<int64_t> simd_load_sq("simd.load_sq");
	lua_load_op<uint64_t> simd_load_uq("simd.load_uq");
	lua_load_op<float> simd_load_s("simd.load_s");
	lua_load_op<double> simd_load_f("simd.load_f");
	lua_unary_op<int8_t, int8_t, copy<int8_t>> simd_copy_b("simd.copy_b");
	lua_unary_op<int16_t, int16_t, copy<int16_t>> simd_copy_w("simd.copy_w");
	lua_unary_op<int32_t, int32_t, copy<int32_t>> simd_copy_d("simd.copy_d");
	lua_unary_op<int64_t, int64_t, copy<int64_t>> simd_copy_q("simd.copy_q");
	lua_unary_op<float, float, copy<float>> simd_copy_s("simd.copy_s");
	lua_unary_op<double, double, copy<double>> simd_copy_f("simd.copy_f");
	lua_unary_op<int8_t, int8_t, neg<int8_t>> simd_neg_b("simd.neg_b");
	lua_unary_op<int16_t, int16_t, neg<int16_t>> simd_neg_w("simd.neg_w");
	lua_unary_op<int32_t, int32_t, neg<int32_t>> simd_neg_d("simd.neg_d");
	lua_unary_op<int64_t, int64_t, neg<int64_t>> simd_neg_q("simd.neg_q");
	lua_unary_op<float, float, neg<float>> simd_neg_s("simd.neg_s");
	lua_unary_op<double, double, neg<double>> simd_neg_f("simd.neg_f");
	lua_binary_op<int8_t, int8_t, int8_t, add<int8_t>> simd_add_b("simd.add_b");
	lua_binary_op<int16_t, int16_t, int16_t, add<int16_t>> simd_add_w("simd.add_w");
	lua_binary_op<int32_t, int32_t, int32_t, add<int32_t>> simd_add_d("simd.add_d");
	lua_binary_op<int64_t, int64_t, int64_t, add<int64_t>> simd_add_q("simd.add_q");
	lua_binary_op<float, float, float, add<float>> simd_add_s("simd.add_s");
	lua_binary_op<double, double, double, add<double>> simd_add_f("simd.add_f");
	lua_binary_op<int8_t, int8_t, int8_t, sub<int8_t>> simd_sub_b("simd.sub_b");
	lua_binary_op<int16_t, int16_t, int16_t, sub<int16_t>> simd_sub_w("simd.sub_w");
	lua_binary_op<int32_t, int32_t, int32_t, sub<int32_t>> simd_sub_d("simd.sub_d");
	lua_binary_op<int64_t, int64_t, int64_t, sub<int64_t>> simd_sub_q("simd.sub_q");
	lua_binary_op<float, float, float, sub<float>> simd_sub_s("simd.sub_s");
	lua_binary_op<double, double, double, sub<double>> simd_sub_f("simd.sub_f");
	lua_binary_op<int8_t, int8_t, int8_t, mul<int8_t>> simd_mul_sb("simd.mul_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, mul<uint8_t>> simd_mul_ub("simd.mul_ub");
	lua_binary_op<int16_t, int16_t, int16_t, mul<int16_t>> simd_mul_sw("simd.mul_sw");
	lua_binary_op<uint16_t, uint16_t, uint16_t, mul<uint16_t>> simd_mul_uw("simd.mul_uw");
	lua_binary_op<int32_t, int32_t, int32_t, mul<int32_t>> simd_mul_sd("simd.mul_sd");
	lua_binary_op<uint32_t, uint32_t, uint32_t, mul<uint32_t>> simd_mul_ud("simd.mul_ud");
	lua_binary_op<int64_t, int64_t, int64_t, mul<int64_t>> simd_mul_sq("simd.mul_sq");
	lua_binary_op<uint64_t, uint64_t, uint64_t, mul<uint64_t>> simd_mul_uq("simd.mul_uq");
	lua_binary_op<float, float, float, mul<float>> simd_mul_s("simd.mul_s");
	lua_binary_op<double, double, double, mul<double>> simd_mul_f("simd.mul_f");
	lua_binary_op<int8_t, int8_t, int8_t, div<int8_t>> simd_div_sb("simd.div_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, div<uint8_t>> simd_div_ub("simd.div_ub");
	lua_binary_op<int16_t, int16_t, int16_t, div<int16_t>> simd_div_sw("simd.div_sw");
	lua_binary_op<uint16_t, uint16_t, uint16_t, div<uint16_t>> simd_div_uw("simd.div_uw");
	lua_binary_op<int32_t, int32_t, int32_t, div<int32_t>> simd_div_sd("simd.div_sd");
	lua_binary_op<uint32_t, uint32_t, uint32_t, div<uint32_t>> simd_div_ud("simd.div_ud");
	lua_binary_op<int64_t, int64_t, int64_t, div<int64_t>> simd_div_sq("simd.div_sq");
	lua_binary_op<uint64_t, uint64_t, uint64_t, div<uint64_t>> simd_div_uq("simd.div_uq");
	lua_binary_op<float, float, float, div<float>> simd_div_s("simd.div_s");
	lua_binary_op<double, double, double, div<double>> simd_div_f("simd.div_f");
	lua_binary_op<int8_t, int8_t, int8_t, rem<int8_t>> simd_rem_sb("simd.rem_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, rem<uint8_t>> simd_rem_ub("simd.rem_ub");
	lua_binary_op<int16_t, int16_t, int16_t, rem<int16_t>> simd_rem_sw("simd.rem_sw");
	lua_binary_op<uint16_t, uint16_t, uint16_t, rem<uint16_t>> simd_rem_uw("simd.rem_uw");
	lua_binary_op<int32_t, int32_t, int32_t, rem<int32_t>> simd_rem_sd("simd.rem_sd");
	lua_binary_op<uint32_t, uint32_t, uint32_t, rem<uint32_t>> simd_rem_ud("simd.rem_ud");
	lua_binary_op<int64_t, int64_t, int64_t, rem<int64_t>> simd_rem_sq("simd.rem_sq");
	lua_binary_op<uint64_t, uint64_t, uint64_t, rem<uint64_t>> simd_rem_uq("simd.rem_uq");
	lua_binary_op<int8_t, int8_t, int8_t, lshift<int8_t>> simd_lshift_sb("simd.lshift_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, lshift<uint8_t>> simd_lshift_ub("simd.lshift_ub");
	lua_binary_op<int16_t, int16_t, int16_t, lshift<int16_t>> simd_lshift_sw("simd.lshift_sw");
	lua_binary_op<uint16_t, uint16_t, uint16_t, lshift<uint16_t>> simd_lshift_uw("simd.lshift_uw");
	lua_binary_op<int32_t, int32_t, int32_t, lshift<int32_t>> simd_lshift_sd("simd.lshift_sd");
	lua_binary_op<uint32_t, uint32_t, uint32_t, lshift<uint32_t>> simd_lshift_ud("simd.lshift_ud");
	lua_binary_op<int64_t, int64_t, int64_t, lshift<int64_t>> simd_lshift_sq("simd.lshift_sq");
	lua_binary_op<uint64_t, uint64_t, uint64_t, lshift<uint64_t>> simd_lshift_uq("simd.lshift_uq");
	lua_binary_op<int8_t, int8_t, int8_t, rshift<int8_t>> simd_rshift_sb("simd.rshift_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, rshift<uint8_t>> simd_rshift_ub("simd.rshift_ub");
	lua_binary_op<int16_t, int16_t, int16_t, rshift<int16_t>> simd_rshift_sw("simd.rshift_sw");
	lua_binary_op<uint16_t, uint16_t, uint16_t, rshift<uint16_t>> simd_rshift_uw("simd.rshift_uw");
	lua_binary_op<int32_t, int32_t, int32_t, rshift<int32_t>> simd_rshift_sd("simd.rshift_sd");
	lua_binary_op<uint32_t, uint32_t, uint32_t, rshift<uint32_t>> simd_rshift_ud("simd.rshift_ud");
	lua_binary_op<int64_t, int64_t, int64_t, rshift<int64_t>> simd_rshift_sq("simd.rshift_sq");
	lua_binary_op<uint64_t, uint64_t, uint64_t, rshift<uint64_t>> simd_rshift_uq("simd.rshift_uq");
	lua_cascade_op<int8_t, add<int8_t>, zero<int8_t>> simd_cadd_b("simd.cadd_b");
	lua_cascade_op<int16_t, add<int16_t>, zero<int16_t>> simd_cadd_w("simd.cadd_w");
	lua_cascade_op<int32_t, add<int32_t>, zero<int32_t>> simd_cadd_d("simd.cadd_d");
	lua_cascade_op<int64_t, add<int64_t>, zero<int64_t>> simd_cadd_q("simd.cadd_q");
	lua_cascade_op<float, add<float>, zero<float>> simd_cadd_s("simd.cadd_s");
	lua_cascade_op<double, add<double>, zero<double>> simd_cadd_f("simd.cadd_f");
	lua_cascade_op<int8_t, mul<int8_t>, one<int8_t>> simd_cmul_sb("simd.cmul_sb");
	lua_cascade_op<uint8_t, mul<uint8_t>, one<uint8_t>> simd_cmul_ub("simd.cmul_ub");
	lua_cascade_op<int16_t, mul<int16_t>, one<int16_t>> simd_cmul_sw("simd.cmul_sw");
	lua_cascade_op<uint16_t, mul<uint16_t>, one<uint16_t>> simd_cmul_uw("simd.cmul_uw");
	lua_cascade_op<int32_t, mul<int32_t>, one<int32_t>> simd_cmul_sd("simd.cmul_sd");
	lua_cascade_op<uint32_t, mul<uint32_t>, one<uint32_t>> simd_cmul_ud("simd.cmul_ud");
	lua_cascade_op<int64_t, mul<int64_t>, one<int64_t>> simd_cmul_sq("simd.cmul_sq");
	lua_cascade_op<uint64_t, mul<uint64_t>, one<uint64_t>> simd_cmul_uq("simd.cmul_uq");
	lua_cascade_op<float, mul<float>, one<float>> simd_cmul_s("simd.cmul_s");
	lua_cascade_op<double, mul<double>, one<double>> simd_cmul_f("simd.cmul_f");
	lua_unary_op<int8_t, int8_t, convert<int8_t, int8_t>> simd_convert_sb_sb("simd.convert_sb_sb");
	lua_unary_op<int8_t, uint8_t, convert<int8_t, uint8_t>> simd_convert_sb_ub("simd.convert_sb_ub");
	lua_unary_op<int8_t, int16_t, convert<int8_t, int16_t>> simd_convert_sb_sw("simd.convert_sb_sw");
	lua_unary_op<int8_t, uint16_t, convert<int8_t, uint16_t>> simd_convert_sb_uw("simd.convert_sb_uw");
	lua_unary_op<int8_t, int32_t, convert<int8_t, int32_t>> simd_convert_sb_sd("simd.convert_sb_sd");
	lua_unary_op<int8_t, uint32_t, convert<int8_t, uint32_t>> simd_convert_sb_ud("simd.convert_sb_ud");
	lua_unary_op<int8_t, int64_t, convert<int8_t, int64_t>> simd_convert_sb_sq("simd.convert_sb_sq");
	lua_unary_op<int8_t, uint64_t, convert<int8_t, uint64_t>> simd_convert_sb_uq("simd.convert_sb_uq");
	lua_unary_op<int8_t, float, convert<int8_t, float>> simd_convert_sb_s("simd.convert_sb_s");
	lua_unary_op<int8_t, double, convert<int8_t, double>> simd_convert_sb_f("simd.convert_sb_f");
	lua_unary_op<uint8_t, int8_t, convert<uint8_t, int8_t>> simd_convert_ub_sb("simd.convert_ub_sb");
	lua_unary_op<uint8_t, uint8_t, convert<uint8_t, uint8_t>> simd_convert_ub_ub("simd.convert_ub_ub");
	lua_unary_op<uint8_t, int16_t, convert<uint8_t, int16_t>> simd_convert_ub_sw("simd.convert_ub_sw");
	lua_unary_op<uint8_t, uint16_t, convert<uint8_t, uint16_t>> simd_convert_ub_uw("simd.convert_ub_uw");
	lua_unary_op<uint8_t, int32_t, convert<uint8_t, int32_t>> simd_convert_ub_sd("simd.convert_ub_sd");
	lua_unary_op<uint8_t, uint32_t, convert<uint8_t, uint32_t>> simd_convert_ub_ud("simd.convert_ub_ud");
	lua_unary_op<uint8_t, int64_t, convert<uint8_t, int64_t>> simd_convert_ub_sq("simd.convert_ub_sq");
	lua_unary_op<uint8_t, uint64_t, convert<uint8_t, uint64_t>> simd_convert_ub_uq("simd.convert_ub_uq");
	lua_unary_op<uint8_t, float, convert<uint8_t, float>> simd_convert_ub_s("simd.convert_ub_s");
	lua_unary_op<uint8_t, double, convert<uint8_t, double>> simd_convert_ub_f("simd.convert_ub_f");
	lua_unary_op<int16_t, int8_t, convert<int16_t, int8_t>> simd_convert_sw_sb("simd.convert_sw_sb");
	lua_unary_op<int16_t, uint8_t, convert<int16_t, uint8_t>> simd_convert_sw_ub("simd.convert_sw_ub");
	lua_unary_op<int16_t, int16_t, convert<int16_t, int16_t>> simd_convert_sw_sw("simd.convert_sw_sw");
	lua_unary_op<int16_t, uint16_t, convert<int16_t, uint16_t>> simd_convert_sw_uw("simd.convert_sw_uw");
	lua_unary_op<int16_t, int32_t, convert<int16_t, int32_t>> simd_convert_sw_sd("simd.convert_sw_sd");
	lua_unary_op<int16_t, uint32_t, convert<int16_t, uint32_t>> simd_convert_sw_ud("simd.convert_sw_ud");
	lua_unary_op<int16_t, int64_t, convert<int16_t, int64_t>> simd_convert_sw_sq("simd.convert_sw_sq");
	lua_unary_op<int16_t, uint64_t, convert<int16_t, uint64_t>> simd_convert_sw_uq("simd.convert_sw_uq");
	lua_unary_op<int16_t, float, convert<int16_t, float>> simd_convert_sw_s("simd.convert_sw_s");
	lua_unary_op<int16_t, double, convert<int16_t, double>> simd_convert_sw_f("simd.convert_sw_f");
	lua_unary_op<uint16_t, int8_t, convert<uint16_t, int8_t>> simd_convert_uw_sb("simd.convert_uw_sb");
	lua_unary_op<uint16_t, uint8_t, convert<uint16_t, uint8_t>> simd_convert_uw_ub("simd.convert_uw_ub");
	lua_unary_op<uint16_t, int16_t, convert<uint16_t, int16_t>> simd_convert_uw_sw("simd.convert_uw_sw");
	lua_unary_op<uint16_t, uint16_t, convert<uint16_t, uint16_t>> simd_convert_uw_uw("simd.convert_uw_uw");
	lua_unary_op<uint16_t, int32_t, convert<uint16_t, int32_t>> simd_convert_uw_sd("simd.convert_uw_sd");
	lua_unary_op<uint16_t, uint32_t, convert<uint16_t, uint32_t>> simd_convert_uw_ud("simd.convert_uw_ud");
	lua_unary_op<uint16_t, int64_t, convert<uint16_t, int64_t>> simd_convert_uw_sq("simd.convert_uw_sq");
	lua_unary_op<uint16_t, uint64_t, convert<uint16_t, uint64_t>> simd_convert_uw_uq("simd.convert_uw_uq");
	lua_unary_op<uint16_t, float, convert<uint16_t, float>> simd_convert_uw_s("simd.convert_uw_s");
	lua_unary_op<uint16_t, double, convert<uint16_t, double>> simd_convert_uw_f("simd.convert_uw_f");
	lua_unary_op<int32_t, int8_t, convert<int32_t, int8_t>> simd_convert_sd_sb("simd.convert_sd_sb");
	lua_unary_op<int32_t, uint8_t, convert<int32_t, uint8_t>> simd_convert_sd_ub("simd.convert_sd_ub");
	lua_unary_op<int32_t, int16_t, convert<int32_t, int16_t>> simd_convert_sd_sw("simd.convert_sd_sw");
	lua_unary_op<int32_t, uint16_t, convert<int32_t, uint16_t>> simd_convert_sd_uw("simd.convert_sd_uw");
	lua_unary_op<int32_t, int32_t, convert<int32_t, int32_t>> simd_convert_sd_sd("simd.convert_sd_sd");
	lua_unary_op<int32_t, uint32_t, convert<int32_t, uint32_t>> simd_convert_sd_ud("simd.convert_sd_ud");
	lua_unary_op<int32_t, int64_t, convert<int32_t, int64_t>> simd_convert_sd_sq("simd.convert_sd_sq");
	lua_unary_op<int32_t, uint64_t, convert<int32_t, uint64_t>> simd_convert_sd_uq("simd.convert_sd_uq");
	lua_unary_op<int32_t, float, convert<int32_t, float>> simd_convert_sd_s("simd.convert_sd_s");
	lua_unary_op<int32_t, double, convert<int32_t, double>> simd_convert_sd_f("simd.convert_sd_f");
	lua_unary_op<uint32_t, int8_t, convert<uint32_t, int8_t>> simd_convert_ud_sb("simd.convert_ud_sb");
	lua_unary_op<uint32_t, uint8_t, convert<uint32_t, uint8_t>> simd_convert_ud_ub("simd.convert_ud_ub");
	lua_unary_op<uint32_t, int16_t, convert<uint32_t, int16_t>> simd_convert_ud_sw("simd.convert_ud_sw");
	lua_unary_op<uint32_t, uint16_t, convert<uint32_t, uint16_t>> simd_convert_ud_uw("simd.convert_ud_uw");
	lua_unary_op<uint32_t, int32_t, convert<uint32_t, int32_t>> simd_convert_ud_sd("simd.convert_ud_sd");
	lua_unary_op<uint32_t, uint32_t, convert<uint32_t, uint32_t>> simd_convert_ud_ud("simd.convert_ud_ud");
	lua_unary_op<uint32_t, int64_t, convert<uint32_t, int64_t>> simd_convert_ud_sq("simd.convert_ud_sq");
	lua_unary_op<uint32_t, uint64_t, convert<uint32_t, uint64_t>> simd_convert_ud_uq("simd.convert_ud_uq");
	lua_unary_op<uint32_t, float, convert<uint32_t, float>> simd_convert_ud_s("simd.convert_ud_s");
	lua_unary_op<uint32_t, double, convert<uint32_t, double>> simd_convert_ud_f("simd.convert_ud_f");
	lua_unary_op<int64_t, int8_t, convert<int64_t, int8_t>> simd_convert_sq_sb("simd.convert_sq_sb");
	lua_unary_op<int64_t, uint8_t, convert<int64_t, uint8_t>> simd_convert_sq_ub("simd.convert_sq_ub");
	lua_unary_op<int64_t, int16_t, convert<int64_t, int16_t>> simd_convert_sq_sw("simd.convert_sq_sw");
	lua_unary_op<int64_t, uint16_t, convert<int64_t, uint16_t>> simd_convert_sq_uw("simd.convert_sq_uw");
	lua_unary_op<int64_t, int32_t, convert<int64_t, int32_t>> simd_convert_sq_sd("simd.convert_sq_sd");
	lua_unary_op<int64_t, uint32_t, convert<int64_t, uint32_t>> simd_convert_sq_ud("simd.convert_sq_ud");
	lua_unary_op<int64_t, int64_t, convert<int64_t, int64_t>> simd_convert_sq_sq("simd.convert_sq_sq");
	lua_unary_op<int64_t, uint64_t, convert<int64_t, uint64_t>> simd_convert_sq_uq("simd.convert_sq_uq");
	lua_unary_op<int64_t, float, convert<int64_t, float>> simd_convert_sq_s("simd.convert_sq_s");
	lua_unary_op<int64_t, double, convert<int64_t, double>> simd_convert_sq_f("simd.convert_sq_f");
	lua_unary_op<uint64_t, int8_t, convert<uint64_t, int8_t>> simd_convert_uq_sb("simd.convert_uq_sb");
	lua_unary_op<uint64_t, uint8_t, convert<uint64_t, uint8_t>> simd_convert_uq_ub("simd.convert_uq_ub");
	lua_unary_op<uint64_t, int16_t, convert<uint64_t, int16_t>> simd_convert_uq_sw("simd.convert_uq_sw");
	lua_unary_op<uint64_t, uint16_t, convert<uint64_t, uint16_t>> simd_convert_uq_uw("simd.convert_uq_uw");
	lua_unary_op<uint64_t, int32_t, convert<uint64_t, int32_t>> simd_convert_uq_sd("simd.convert_uq_sd");
	lua_unary_op<uint64_t, uint32_t, convert<uint64_t, uint32_t>> simd_convert_uq_ud("simd.convert_uq_ud");
	lua_unary_op<uint64_t, int64_t, convert<uint64_t, int64_t>> simd_convert_uq_sq("simd.convert_uq_sq");
	lua_unary_op<uint64_t, uint64_t, convert<uint64_t, uint64_t>> simd_convert_uq_uq("simd.convert_uq_uq");
	lua_unary_op<uint64_t, float, convert<uint64_t, float>> simd_convert_uq_s("simd.convert_uq_s");
	lua_unary_op<uint64_t, double, convert<uint64_t, double>> simd_convert_uq_f("simd.convert_uq_f");
	lua_unary_op<float, int8_t, convert<float, int8_t>> simd_convert_s_sb("simd.convert_s_sb");
	lua_unary_op<float, uint8_t, convert<float, uint8_t>> simd_convert_s_ub("simd.convert_s_ub");
	lua_unary_op<float, int16_t, convert<float, int16_t>> simd_convert_s_sw("simd.convert_s_sw");
	lua_unary_op<float, uint16_t, convert<float, uint16_t>> simd_convert_s_uw("simd.convert_s_uw");
	lua_unary_op<float, int32_t, convert<float, int32_t>> simd_convert_s_sd("simd.convert_s_sd");
	lua_unary_op<float, uint32_t, convert<float, uint32_t>> simd_convert_s_ud("simd.convert_s_ud");
	lua_unary_op<float, int64_t, convert<float, int64_t>> simd_convert_s_sq("simd.convert_s_sq");
	lua_unary_op<float, uint64_t, convert<float, uint64_t>> simd_convert_s_uq("simd.convert_s_uq");
	lua_unary_op<float, float, convert<float, float>> simd_convert_s_s("simd.convert_s_s");
	lua_unary_op<float, double, convert<float, double>> simd_convert_s_f("simd.convert_s_f");
	lua_unary_op<double, int8_t, convert<double, int8_t>> simd_convert_f_sb("simd.convert_f_sb");
	lua_unary_op<double, uint8_t, convert<double, uint8_t>> simd_convert_f_ub("simd.convert_f_ub");
	lua_unary_op<double, int16_t, convert<double, int16_t>> simd_convert_f_sw("simd.convert_f_sw");
	lua_unary_op<double, uint16_t, convert<double, uint16_t>> simd_convert_f_uw("simd.convert_f_uw");
	lua_unary_op<double, int32_t, convert<double, int32_t>> simd_convert_f_sd("simd.convert_f_sd");
	lua_unary_op<double, uint32_t, convert<double, uint32_t>> simd_convert_f_ud("simd.convert_f_ud");
	lua_unary_op<double, int64_t, convert<double, int64_t>> simd_convert_f_sq("simd.convert_f_sq");
	lua_unary_op<double, uint64_t, convert<double, uint64_t>> simd_convert_f_uq("simd.convert_f_uq");
	lua_unary_op<double, float, convert<double, float>> simd_convert_f_s("simd.convert_f_s");
	lua_unary_op<double, double, convert<double, double>> simd_convert_f_f("simd.convert_f_f");
	lua_binary_op<uint8_t, int8_t, int8_t, lt<int8_t>> simd_lt_sb("simd.lt_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, lt<uint8_t>> simd_lt_ub("simd.lt_ub");
	lua_binary_op<uint8_t, int16_t, int16_t, lt<int16_t>> simd_lt_sw("simd.lt_sw");
	lua_binary_op<uint8_t, uint16_t, uint16_t, lt<uint16_t>> simd_lt_uw("simd.lt_uw");
	lua_binary_op<uint8_t, int32_t, int32_t, lt<int32_t>> simd_lt_sd("simd.lt_sd");
	lua_binary_op<uint8_t, uint32_t, uint32_t, lt<uint32_t>> simd_lt_ud("simd.lt_ud");
	lua_binary_op<uint8_t, int64_t, int64_t, lt<int64_t>> simd_lt_sq("simd.lt_sq");
	lua_binary_op<uint8_t, uint64_t, uint64_t, lt<uint64_t>> simd_lt_uq("simd.lt_uq");
	lua_binary_op<uint8_t, float, float, lt<float>> simd_lt_s("simd.lt_s");
	lua_binary_op<uint8_t, double, double, lt<double>> simd_lt_f("simd.lt_f");
	lua_binary_op<uint8_t, int8_t, int8_t, le<int8_t>> simd_le_sb("simd.le_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, le<uint8_t>> simd_le_ub("simd.le_ub");
	lua_binary_op<uint8_t, int16_t, int16_t, le<int16_t>> simd_le_sw("simd.le_sw");
	lua_binary_op<uint8_t, uint16_t, uint16_t, le<uint16_t>> simd_le_uw("simd.le_uw");
	lua_binary_op<uint8_t, int32_t, int32_t, le<int32_t>> simd_le_sd("simd.le_sd");
	lua_binary_op<uint8_t, uint32_t, uint32_t, le<uint32_t>> simd_le_ud("simd.le_ud");
	lua_binary_op<uint8_t, int64_t, int64_t, le<int64_t>> simd_le_sq("simd.le_sq");
	lua_binary_op<uint8_t, uint64_t, uint64_t, le<uint64_t>> simd_le_uq("simd.le_uq");
	lua_binary_op<uint8_t, float, float, le<float>> simd_le_s("simd.le_s");
	lua_binary_op<uint8_t, double, double, le<double>> simd_le_f("simd.le_f");
	lua_binary_op<uint8_t, int8_t, int8_t, eq<int8_t>> simd_eq_sb("simd.eq_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, eq<uint8_t>> simd_eq_ub("simd.eq_ub");
	lua_binary_op<uint8_t, int16_t, int16_t, eq<int16_t>> simd_eq_sw("simd.eq_sw");
	lua_binary_op<uint8_t, uint16_t, uint16_t, eq<uint16_t>> simd_eq_uw("simd.eq_uw");
	lua_binary_op<uint8_t, int32_t, int32_t, eq<int32_t>> simd_eq_sd("simd.eq_sd");
	lua_binary_op<uint8_t, uint32_t, uint32_t, eq<uint32_t>> simd_eq_ud("simd.eq_ud");
	lua_binary_op<uint8_t, int64_t, int64_t, eq<int64_t>> simd_eq_sq("simd.eq_sq");
	lua_binary_op<uint8_t, uint64_t, uint64_t, eq<uint64_t>> simd_eq_uq("simd.eq_uq");
	lua_binary_op<uint8_t, float, float, eq<float>> simd_eq_s("simd.eq_s");
	lua_binary_op<uint8_t, double, double, eq<double>> simd_eq_f("simd.eq_f");
	lua_binary_op<uint8_t, int8_t, int8_t, ne<int8_t>> simd_ne_sb("simd.ne_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, ne<uint8_t>> simd_ne_ub("simd.ne_ub");
	lua_binary_op<uint8_t, int16_t, int16_t, ne<int16_t>> simd_ne_sw("simd.ne_sw");
	lua_binary_op<uint8_t, uint16_t, uint16_t, ne<uint16_t>> simd_ne_uw("simd.ne_uw");
	lua_binary_op<uint8_t, int32_t, int32_t, ne<int32_t>> simd_ne_sd("simd.ne_sd");
	lua_binary_op<uint8_t, uint32_t, uint32_t, ne<uint32_t>> simd_ne_ud("simd.ne_ud");
	lua_binary_op<uint8_t, int64_t, int64_t, ne<int64_t>> simd_ne_sq("simd.ne_sq");
	lua_binary_op<uint8_t, uint64_t, uint64_t, ne<uint64_t>> simd_ne_uq("simd.ne_uq");
	lua_binary_op<uint8_t, float, float, ne<float>> simd_ne_s("simd.ne_s");
	lua_binary_op<uint8_t, double, double, ne<double>> simd_ne_f("simd.ne_f");
	lua_binary_op<uint8_t, int8_t, int8_t, ge<int8_t>> simd_ge_sb("simd.ge_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, ge<uint8_t>> simd_ge_ub("simd.ge_ub");
	lua_binary_op<uint8_t, int16_t, int16_t, ge<int16_t>> simd_ge_sw("simd.ge_sw");
	lua_binary_op<uint8_t, uint16_t, uint16_t, ge<uint16_t>> simd_ge_uw("simd.ge_uw");
	lua_binary_op<uint8_t, int32_t, int32_t, ge<int32_t>> simd_ge_sd("simd.ge_sd");
	lua_binary_op<uint8_t, uint32_t, uint32_t, ge<uint32_t>> simd_ge_ud("simd.ge_ud");
	lua_binary_op<uint8_t, int64_t, int64_t, ge<int64_t>> simd_ge_sq("simd.ge_sq");
	lua_binary_op<uint8_t, uint64_t, uint64_t, ge<uint64_t>> simd_ge_uq("simd.ge_uq");
	lua_binary_op<uint8_t, float, float, ge<float>> simd_ge_s("simd.ge_s");
	lua_binary_op<uint8_t, double, double, ge<double>> simd_ge_f("simd.ge_f");
	lua_binary_op<uint8_t, int8_t, int8_t, gt<int8_t>> simd_gt_sb("simd.gt_sb");
	lua_binary_op<uint8_t, uint8_t, uint8_t, gt<uint8_t>> simd_gt_ub("simd.gt_ub");
	lua_binary_op<uint8_t, int16_t, int16_t, gt<int16_t>> simd_gt_sw("simd.gt_sw");
	lua_binary_op<uint8_t, uint16_t, uint16_t, gt<uint16_t>> simd_gt_uw("simd.gt_uw");
	lua_binary_op<uint8_t, int32_t, int32_t, gt<int32_t>> simd_gt_sd("simd.gt_sd");
	lua_binary_op<uint8_t, uint32_t, uint32_t, gt<uint32_t>> simd_gt_ud("simd.gt_ud");
	lua_binary_op<uint8_t, int64_t, int64_t, gt<int64_t>> simd_gt_sq("simd.gt_sq");
	lua_binary_op<uint8_t, uint64_t, uint64_t, gt<uint64_t>> simd_gt_uq("simd.gt_uq");
	lua_binary_op<uint8_t, float, float, gt<float>> simd_gt_s("simd.gt_s");
	lua_binary_op<uint8_t, double, double, gt<double>> simd_gt_f("simd.gt_f");
	lua_isort_op<uint32_t, int8_t> simd_isort_d_sb("simd.isort_d_sb");
	lua_isort_op<uint32_t, uint8_t> simd_isort_d_ub("simd.isort_d_ub");
	lua_isort_op<uint32_t, int16_t> simd_isort_d_sw("simd.isort_d_sw");
	lua_isort_op<uint32_t, uint16_t> simd_isort_d_uw("simd.isort_d_uw");
	lua_isort_op<uint32_t, int32_t> simd_isort_d_sd("simd.isort_d_sd");
	lua_isort_op<uint32_t, uint32_t> simd_isort_d_ud("simd.isort_d_ud");
	lua_isort_op<uint32_t, int64_t> simd_isort_d_sq("simd.isort_d_sq");
	lua_isort_op<uint32_t, uint64_t> simd_isort_d_uq("simd.isort_d_uq");
	lua_isort_op<uint32_t, float> simd_isort_d_s("simd.isort_d_s");
	lua_isort_op<uint32_t, double> simd_isort_d_f("simd.isort_d_f");
	lua_isort_op<uint64_t, int8_t> simd_isort_q_sb("simd.isort_q_sb");
	lua_isort_op<uint64_t, uint8_t> simd_isort_q_ub("simd.isort_q_ub");
	lua_isort_op<uint64_t, int16_t> simd_isort_q_sw("simd.isort_q_sw");
	lua_isort_op<uint64_t, uint16_t> simd_isort_q_uw("simd.isort_q_uw");
	lua_isort_op<uint64_t, int32_t> simd_isort_q_sd("simd.isort_q_sd");
	lua_isort_op<uint64_t, uint32_t> simd_isort_q_ud("simd.isort_q_ud");
	lua_isort_op<uint64_t, int64_t> simd_isort_q_sq("simd.isort_q_sq");
	lua_isort_op<uint64_t, uint64_t> simd_isort_q_uq("simd.isort_q_uq");
	lua_isort_op<uint64_t, float> simd_isort_q_s("simd.isort_q_s");
	lua_isort_op<uint64_t, double> simd_isort_q_f("simd.isort_q_f");
	lua_ipermute_op<uint32_t, int8_t> simd_ipermute_d_sb("simd.ipermute_d_sb");
	lua_ipermute_op<uint32_t, uint8_t> simd_ipermute_d_ub("simd.ipermute_d_ub");
	lua_ipermute_op<uint32_t, int16_t> simd_ipermute_d_sw("simd.ipermute_d_sw");
	lua_ipermute_op<uint32_t, uint16_t> simd_ipermute_d_uw("simd.ipermute_d_uw");
	lua_ipermute_op<uint32_t, int32_t> simd_ipermute_d_sd("simd.ipermute_d_sd");
	lua_ipermute_op<uint32_t, uint32_t> simd_ipermute_d_ud("simd.ipermute_d_ud");
	lua_ipermute_op<uint32_t, int64_t> simd_ipermute_d_sq("simd.ipermute_d_sq");
	lua_ipermute_op<uint32_t, uint64_t> simd_ipermute_d_uq("simd.ipermute_d_uq");
	lua_ipermute_op<uint32_t, float> simd_ipermute_d_s("simd.ipermute_d_s");
	lua_ipermute_op<uint32_t, double> simd_ipermute_d_f("simd.ipermute_d_f");
	lua_ipermute_op<uint64_t, int8_t> simd_ipermute_q_sb("simd.ipermute_q_sb");
	lua_ipermute_op<uint64_t, uint8_t> simd_ipermute_q_ub("simd.ipermute_q_ub");
	lua_ipermute_op<uint64_t, int16_t> simd_ipermute_q_sw("simd.ipermute_q_sw");
	lua_ipermute_op<uint64_t, uint16_t> simd_ipermute_q_uw("simd.ipermute_q_uw");
	lua_ipermute_op<uint64_t, int32_t> simd_ipermute_q_sd("simd.ipermute_q_sd");
	lua_ipermute_op<uint64_t, uint32_t> simd_ipermute_q_ud("simd.ipermute_q_ud");
	lua_ipermute_op<uint64_t, int64_t> simd_ipermute_q_sq("simd.ipermute_q_sq");
	lua_ipermute_op<uint64_t, uint64_t> simd_ipermute_q_uq("simd.ipermute_q_uq");
	lua_ipermute_op<uint64_t, float> simd_ipermute_q_s("simd.ipermute_q_s");
	lua_ipermute_op<uint64_t, double> simd_ipermute_q_f("simd.ipermute_q_f");
	lua_unary_op<float, float, sqrtf> simd_sqrt_s("simd.sqrt_s");
	lua_unary_op<double, double, sqrt> simd_sqrt_f("simd.sqrt_f");
	lua_unary_op<float, float, sinf> simd_sin_s("simd.sin_s");
	lua_unary_op<double, double, sin> simd_sin_f("simd.sin_f");
	lua_unary_op<float, float, cosf> simd_cos_s("simd.cos_s");
	lua_unary_op<double, double, cos> simd_cos_f("simd.cos_f");
	lua_unary_op<float, float, tanf> simd_tan_s("simd.tan_s");
	lua_unary_op<double, double, tan> simd_tan_f("simd.tan_f");
	lua_unary_op<float, float, atanf> simd_atan_s("simd.atan_s");
	lua_unary_op<double, double, atan> simd_atan_f("simd.atan_f");
	lua_unary_op<float, float, fabsf> simd_abs_s("simd.abs_s");
	lua_unary_op<double, double, fabs> simd_abs_f("simd.abs_f");
	lua_binary_op<float, float, float, cplxabsf> simd_cplxabs_s("simd.cplxabs_s");
	lua_binary_op<double, double, double, cplxabs> simd_cplxabs_f("simd.cplxabs_f");
	lua_binary_op<uint8_t, uint8_t, uint8_t, _xor<uint8_t>> simd_bxor("simd.bxor");
	lua_binary_op<uint8_t, uint8_t, uint8_t, _or<uint8_t>> simd_bor("simd.bor");
	lua_binary_op<uint8_t, uint8_t, uint8_t, _and<uint8_t>> simd_band("simd.band");
	lua_unary_op<uint8_t, uint8_t, _not<uint8_t>> simd_bnot("simd.bnot");
}

DECLARE_LUACLASS(lua_simdvector, "SIMDVECTOR");
