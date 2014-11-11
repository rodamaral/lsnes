#ifndef _library__mathexpr__hpp__included__
#define _library__mathexpr__hpp__included__

#include <functional>
#include <vector>
#include <stdexcept>
#include "gc.hpp"
#include "mathexpr-error.hpp"
#include <set>

namespace mathexpr
{
struct typeinfo;
struct mathexpr;

struct value
{
	typeinfo* type;
	void* _value;
};

struct _format
{
	enum _type
	{
		BOOLEAN,
		BINARY,
		OCTAL,
		DECIMAL,
		HEXADECIMAL,
		STRING,
		DEFAULT,
	} type;
	bool showsign;
	bool fillzeros;
	int width;
	int precision;
	bool uppercasehex;
};

struct operinfo
{
	operinfo(std::string funcname);
	operinfo(std::string opername, unsigned _operands, int _percedence, bool _rtl = false);
	virtual ~operinfo();
	virtual void evaluate(value target, std::vector<std::function<value()>> promises) = 0;
	const std::string fnname;
	const bool is_operator;
	const unsigned  operands; 		//Only for operators (max 2 operands).
	const int precedence;			//Higher binds more tightly.
	const bool rtl;			//If true, Right-to-left associvity.
};

struct typeinfo
{
	virtual ~typeinfo();
	virtual void* allocate() = 0;
	virtual void* parse(const std::string& str, bool string) = 0;
	virtual void parse_u(void* obj, uint64_t v) = 0;
	virtual void parse_s(void* obj, int64_t v) = 0;
	virtual void parse_f(void* obj, double v) = 0;
	virtual void parse_b(void* obj, bool v) = 0;
	virtual void scale(void* val, uint64_t scale) = 0;
	virtual void deallocate(void* obj) = 0;
	virtual void copy(void* target, void* source) = 0;
	virtual std::string tostring(void* obj) = 0;
	virtual std::string format(void* obj, _format fmt) = 0;
	virtual uint64_t tounsigned(void* obj) = 0;
	virtual int64_t tosigned(void* obj) = 0;
	virtual bool toboolean(void* obj) = 0;
	virtual std::set<operinfo*> operations() = 0;
	void* copy_allocate(void* src)
	{
		void* p = allocate();
		try {
			copy(p, src);
		} catch(...) {
			deallocate(p);
			throw;
		}
		return p;
	}
};

template<class T> struct operinfo_wrapper : public operinfo
{
	operinfo_wrapper(std::string funcname, T (*_fn)(std::vector<std::function<T&()>> promises))
		: operinfo(funcname), fn(_fn)
	{
	}
	operinfo_wrapper(std::string opername, unsigned _operands, int _percedence, bool _rtl,
		T (*_fn)(std::vector<std::function<T&()>> promises))
		: operinfo(opername, _operands, _percedence, _rtl), fn(_fn)
	{
	}
	~operinfo_wrapper()
	{
	}
	void evaluate(value target, std::vector<std::function<value()>> promises)
	{
		std::vector<std::function<T&()>> _promises;
		for(auto i : promises) {
			std::function<value()> f = i;
			_promises.push_back([f, this]() -> T& {
				auto r = f();
				return *(T*)r._value;
			});
		}
		*(T*)(target._value) = fn(_promises);
	}
private:
	T (*fn)(std::vector<std::function<T&()>> promises);
};

template<class T> struct opfun_info
{
	std::string name;
	T (*_fn)(std::vector<std::function<T&()>> promises);
	bool is_operator;
	unsigned operands;
	int precedence;
	bool rtl;
};

template<class T> struct operinfo_set
{
	operinfo_set(std::initializer_list<opfun_info<T>> list)
	{
		for(auto i : list) {
			if(i.is_operator)
				set.insert(new operinfo_wrapper<T>(i.name, i.operands, i.precedence,
					i.rtl, i._fn));
			else
				set.insert(new operinfo_wrapper<T>(i.name, i._fn));
		}
	}
	~operinfo_set()
	{
		for(auto i : set)
			delete i;
	}
	std::set<operinfo*>& get_set()
	{
		return set;
	}
private:
	std::set<operinfo*> set;
};

template<class T> struct typeinfo_wrapper : public typeinfo
{
	struct unsigned_tag {};
	struct signed_tag {};
	struct float_tag {};
	struct boolean_tag {};
	~typeinfo_wrapper()
	{
	}
	void* allocate()
	{
		return new T;
	}
	void* parse(const std::string& str, bool string)
	{
		return new T(str, string);
	}
	void parse_u(void* obj, uint64_t v)
	{
		*(T*)obj = T(unsigned_tag(), v);
	}
	void parse_s(void* obj, int64_t v)
	{
		*(T*)obj = T(signed_tag(), v);
	}
	void parse_f(void* obj, double v)
	{
		*(T*)obj = T(float_tag(), v);
	}
	void parse_b(void* obj, bool b)
	{
		*(T*)obj = T(boolean_tag(), b);
	}
	void deallocate(void* obj)
	{
		delete (T*)obj;
	}
	void copy(void* target, void* source)
	{
		*(T*)target = *(T*)source;
	}
	std::string tostring(void* obj)
	{
		return ((T*)obj)->tostring();
	}
	std::string format(void* obj, _format fmt)
	{
		return ((T*)obj)->format(fmt);
	}
	uint64_t tounsigned(void* obj)
	{
		return ((T*)obj)->tounsigned();
	}
	int64_t tosigned(void* obj)
	{
		return ((T*)obj)->tosigned();
	}
	bool toboolean(void* obj)
	{
		return ((T*)obj)->toboolean();
	}
	void scale(void* val, uint64_t _scale)
	{
		return ((T*)val)->scale(_scale);
	}
	std::set<operinfo*> operations()
	{
		std::set<operinfo*> ret;
		auto tmp = T::operations();
		for(auto i : tmp)
			ret.insert(i);
		return ret;
	}
};

class mathexpr : public GC::item
{
public:
	enum eval_state
	{
		TO_BE_EVALUATED,	//Not even attempted to evaluate yet.
		EVALUATING,		//Evaluation in progress.
		EVALUATED,		//Evaluation completed, value available.
		FIXED,			//This operand has fixed value.
		UNDEFINED,		//This operand has undefined value.
		FAILED,			//Evaluation failed.
		FORWARD,		//Forward evaluation to first of args.
		FORWARD_EVALING,	//Forward evaluation to first of args, evaluating.
		FORWARD_EVALD,		//Forward evaluation to first of args, evaluated.
	};
	//Undefined value of specified type.
	mathexpr(typeinfo* _type);
	//Forward evaluation.
	mathexpr(typeinfo* _type, GC::pointer<mathexpr> fwd);
	//Value of specified type.
	mathexpr(value value);
	//Value of specified type.
	mathexpr(typeinfo* _type, const std::string& value, bool string);
	//Specified Operator.
	mathexpr(typeinfo* _type, operinfo* fn, std::vector<GC::pointer<mathexpr>> _args,
		bool _owns_operator = false);
	//Dtor.
	~mathexpr();
	//Copy ctor.
	mathexpr(const mathexpr& m);
	mathexpr& operator=(const mathexpr& m);
	//Evaluate. Returns pointer to internal state.
	value evaluate();
	//Get type.
	typeinfo& get_type() { return type; }
	//Reset.
	void reset();
	//Parse an expression.
	static GC::pointer<mathexpr> parse(typeinfo& _type, const std::string& expr,
		std::function<GC::pointer<mathexpr>(const std::string&)> vars);
protected:
	void trace();
private:
	void mark_error_and_throw(error::errorcode _errcode, const std::string& _error);
	eval_state state;
	typeinfo& type;				//Type of value.
	void* _value;				//Value if state is EVALUATED or FIXED.
	operinfo* fn;				//Function (if state is TO_BE_EVALUATED, EVALUATING or EVALUATED)
	error::errorcode errcode;		//Error code if state is FAILED.
	std::string _error;			//Error message if state is FAILED.
	std::vector<mathexpr*> arguments;
	mutable bool owns_operator;
};
}

#endif
