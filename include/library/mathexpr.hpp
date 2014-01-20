#ifndef _library__mathexpr__hpp__included__
#define _library__mathexpr__hpp__included__

#include <functional>
#include <vector>
#include <stdexcept>
#include "gc.hpp"
#include "mathexpr-error.hpp"
#include <set>

struct mathexpr_typeinfo;
struct mathexpr;

struct mathexpr_value
{
	mathexpr_typeinfo* type;
	void* value;
};

struct mathexpr_format
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

struct mathexpr_operinfo
{
	mathexpr_operinfo(std::string funcname);
	mathexpr_operinfo(std::string opername, unsigned _operands, int _percedence, bool _rtl = false);
	virtual ~mathexpr_operinfo();
	virtual void evaluate(mathexpr_value target, std::vector<std::function<mathexpr_value()>> promises) = 0;
	const std::string fnname;
	const bool is_operator;
	const unsigned  operands; 		//Only for operators (max 2 operands).
	const int precedence;			//Higher binds more tightly.
	const bool rtl;			//If true, Right-to-left associvity.
};

struct mathexpr_typeinfo
{
	virtual ~mathexpr_typeinfo();
	virtual void* allocate() = 0;
	virtual void* parse(const std::string& str, bool string) = 0;
	virtual void parse_u(void* obj, uint64_t v) = 0;
	virtual void parse_s(void* obj, int64_t v) = 0;
	virtual void parse_f(void* obj, double v) = 0;
	virtual void scale(void* val, uint64_t scale) = 0;
	virtual void deallocate(void* obj) = 0;
	virtual void copy(void* target, void* source) = 0;
	virtual std::string tostring(void* obj) = 0;
	virtual std::string format(void* obj, mathexpr_format fmt) = 0;
	virtual uint64_t tounsigned(void* obj) = 0;
	virtual int64_t tosigned(void* obj) = 0;
	virtual bool toboolean(void* obj) = 0;
	virtual std::set<mathexpr_operinfo*> operations() = 0;
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

template<class T> struct mathexpr_operinfo_wrapper : public mathexpr_operinfo
{
	mathexpr_operinfo_wrapper(std::string funcname, T (*_fn)(std::vector<std::function<T&()>> promises))
		: mathexpr_operinfo(funcname), fn(_fn)
	{
	}
	mathexpr_operinfo_wrapper(std::string opername, unsigned _operands, int _percedence, bool _rtl,
		T (*_fn)(std::vector<std::function<T&()>> promises))
		: mathexpr_operinfo(opername, _operands, _percedence, _rtl), fn(_fn)
	{
	}
	~mathexpr_operinfo_wrapper()
	{
	}
	void evaluate(mathexpr_value target, std::vector<std::function<mathexpr_value()>> promises)
	{
		std::vector<std::function<T&()>> _promises;
		for(auto i : promises) {
			std::function<mathexpr_value()> f = i;
			_promises.push_back([f, this]() -> T& {
				auto r = f();
				return *(T*)r.value;
			});
		}
		*(T*)(target.value) = fn(_promises);
	}
private:
	T (*fn)(std::vector<std::function<T&()>> promises);
};

template<class T> struct mathexpr_opfun_info
{
	std::string name;
	T (*_fn)(std::vector<std::function<T&()>> promises);
	bool is_operator;
	unsigned operands;
	int precedence;
	bool rtl;
};

template<class T> struct mathexpr_operinfo_set
{
	mathexpr_operinfo_set(std::initializer_list<mathexpr_opfun_info<T>> list)
	{
		for(auto i : list) {
			if(i.is_operator)
				set.insert(new mathexpr_operinfo_wrapper<T>(i.name, i.operands, i.precedence,
					i.rtl, i._fn));
			else
				set.insert(new mathexpr_operinfo_wrapper<T>(i.name, i._fn));
		}
	}
	~mathexpr_operinfo_set()
	{
		for(auto i : set)
			delete i;
	}
	std::set<mathexpr_operinfo*>& get_set()
	{
		return set;
	}
private:
	std::set<mathexpr_operinfo*> set;
};

template<class T> struct mathexpr_typeinfo_wrapper : public mathexpr_typeinfo
{
	struct unsigned_tag {};
	struct signed_tag {};
	struct float_tag {};
	~mathexpr_typeinfo_wrapper()
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
	std::string format(void* obj, mathexpr_format fmt)
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
	std::set<mathexpr_operinfo*> operations()
	{
		std::set<mathexpr_operinfo*> ret;
		auto tmp = T::operations();
		for(auto i : tmp)
			ret.insert(i);
		return ret;
	}
};

class mathexpr : public garbage_collectable
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
	mathexpr(mathexpr_typeinfo* _type);
	//Forward evaluation.
	mathexpr(mathexpr_typeinfo* _type, gcroot_pointer<mathexpr> fwd);
	//Value of specified type.
	mathexpr(mathexpr_value value);
	//Value of specified type.
	mathexpr(mathexpr_typeinfo* _type, const std::string& value, bool string);
	//Specified Operator.
	mathexpr(mathexpr_typeinfo* _type, mathexpr_operinfo* fn, std::vector<gcroot_pointer<mathexpr>> _args,
		bool _owns_operator = false);
	//Dtor.
	~mathexpr();
	//Copy ctor.
	mathexpr(const mathexpr& m);
	mathexpr& operator=(const mathexpr& m);
	//Evaluate. Returns pointer to internal state.
	mathexpr_value evaluate();
	//Get type.
	mathexpr_typeinfo& get_type() { return type; }
	//Reset.
	void reset();
	//Parse an expression.
	static gcroot_pointer<mathexpr> parse(mathexpr_typeinfo& _type, const std::string& expr,
		std::function<gcroot_pointer<mathexpr>(const std::string&)> vars);
protected:
	void trace();
private:
	void mark_error_and_throw(mathexpr_error::errorcode _errcode, const std::string& _error);
	eval_state state;
	mathexpr_typeinfo& type;		//Type of value.
	void* value;				//Value if state is EVALUATED or FIXED.
	mathexpr_operinfo* fn;			//Function (if state is TO_BE_EVALUATED, EVALUATING or EVALUATED)
	mathexpr_error::errorcode errcode;	//Error code if state is FAILED.
	std::string error;			//Error message if state is FAILED.
	std::vector<mathexpr*> arguments;
	mutable bool owns_operator;
};

#endif
