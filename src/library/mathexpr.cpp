#include "mathexpr.hpp"
#include "string.hpp"
#include <set>
#include <map>

mathexpr_operinfo::mathexpr_operinfo(std::string funcname)
	: fnname(funcname), is_operator(false), operands(0), precedence(0), rtl(false)
{
}
mathexpr_operinfo::mathexpr_operinfo(std::string opername, unsigned _operands, int _percedence, bool _rtl)
	: fnname(opername), is_operator(true), operands(_operands), precedence(_percedence), rtl(_rtl)
{
}
mathexpr_operinfo::~mathexpr_operinfo()
{
}

mathexpr_typeinfo::~mathexpr_typeinfo()
{
}

mathexpr::mathexpr(mathexpr_typeinfo* _type)
	: type(*_type)
{
	owns_operator = false;
	state = UNDEFINED;
	value = NULL;
	fn = (mathexpr_operinfo*)0xDEADBEEF;
}

mathexpr::mathexpr(mathexpr_typeinfo* _type, gcroot_pointer<mathexpr> fwd)
	: type(*_type)
{
	owns_operator = false;
	state = FORWARD;
	value = type.allocate();
	arguments.push_back(&*fwd);
	fn = NULL;
}

mathexpr::mathexpr(mathexpr_value _value)
	: type(*_value.type)
{
	owns_operator = false;
	state = FIXED;
	value = type.copy_allocate(_value.value);
	fn = NULL;
}

mathexpr::mathexpr(mathexpr_typeinfo* _type, const std::string& _value, bool string)
	: type(*_type)
{
	owns_operator = false;
	state = FIXED;
	value = type.parse(_value, string);
	fn = NULL;
}

mathexpr::mathexpr(mathexpr_typeinfo* _type, mathexpr_operinfo* _fn, std::vector<gcroot_pointer<mathexpr>> _args,
	bool _owns_operator)
	: type(*_type), fn(_fn), owns_operator(_owns_operator)
{
	try {
		for(auto& i : _args)
			arguments.push_back(&*i);
		value = type.allocate();
		state = TO_BE_EVALUATED;
	} catch(...) {
		if(owns_operator)
			delete fn;
		throw;
	}
}

mathexpr::~mathexpr()
{
	if(owns_operator && fn)
		delete fn;
	type.deallocate(value);
}

void mathexpr::reset()
{
	if(state == TO_BE_EVALUATED || state == FIXED || state == UNDEFINED || state == FORWARD)
		return;
	if(state == FORWARD_EVALD || state == FORWARD_EVALING) {
		state = FORWARD;
		return;
	}
	state = TO_BE_EVALUATED;
	for(auto i : arguments)
		i->reset();
}

mathexpr::mathexpr(const mathexpr& m)
	: state(m.state), type(m.type), fn(m.fn), error(m.error), arguments(m.arguments)
{
	value = m.value ? type.copy_allocate(m.value) : NULL;
	if(state == EVALUATING) state = TO_BE_EVALUATED;
}

mathexpr& mathexpr::operator=(const mathexpr& m)
{
	if(this == &m)
		return *this;
	std::string _error = m.error;
	std::vector<mathexpr*> _arguments = m.arguments;
	if(m.value) {
		if(!value)
			value = m.type.copy_allocate(m.value);
		else
			m.type.copy(value, m.value);
	} else if(value) {
		m.type.deallocate(value);
		value = NULL;
	} else
		value = NULL;
	type = m.type;
	fn = m.fn;
	state = m.state;
	owns_operator = m.owns_operator;
	m.owns_operator = false;
	std::swap(arguments, _arguments);
	std::swap(error, _error);
	return *this;
}

mathexpr_value mathexpr::evaluate()
{
	mathexpr_value ret;
	ret.type = &type;
	switch(state) {
	case TO_BE_EVALUATED:
		//Need to evaluate.
		try {
			for(auto i : arguments) {
				if(&i->type != &type) {
					throw mathexpr_error(mathexpr_error::TYPE_MISMATCH,
						"Types for function mismatch");
				}
			}
			state = EVALUATING;
			std::vector<std::function<mathexpr_value()>> promises;
			for(auto i : arguments) {
				mathexpr* m = i;
				promises.push_back([m]() { return m->evaluate(); });
			}
			mathexpr_value tmp;
			tmp.type = &type;
			tmp.value = value;
			fn->evaluate(tmp, promises);
			state = EVALUATED;
		} catch(mathexpr_error& e) {
			state = FAILED;
			errcode = e.get_code();
			error = e.what();
			throw;
		} catch(std::exception& e) {
			state = FAILED;
			errcode = mathexpr_error::UNKNOWN;
			error = e.what();
			throw;
		} catch(...) {
			state = FAILED;
			errcode = mathexpr_error::UNKNOWN;
			error = "Unknown error";
			throw;
		}
		ret.value = value;
		return ret;
	case EVALUATING:
	case FORWARD_EVALING:
		//Circular dependency.
		mark_error_and_throw(mathexpr_error::CIRCULAR, "Circular dependency");
	case EVALUATED:
	case FIXED:
	case FORWARD_EVALD:
		ret.value = value;
		return ret;
	case UNDEFINED:
		throw mathexpr_error(mathexpr_error::UNDEFINED, "Undefined variable");
	case FAILED:
		throw mathexpr_error(errcode, error);
	case FORWARD:
		try {
			state = FORWARD_EVALING;
			mathexpr_value v = arguments[0]->evaluate();
			type.copy(value, v.value);
			state = FORWARD_EVALD;
			return v;
		} catch(...) {
			state = FORWARD;
			throw;
		}
	}
	throw mathexpr_error(mathexpr_error::INTERNAL, "Internal error (shouldn't be here)");
}

void mathexpr::trace()
{
	for(auto i : arguments)
		i->mark();
}

void mathexpr::mark_error_and_throw(mathexpr_error::errorcode _errcode, const std::string& _error)
{
	if(state == EVALUATING) {
		state = FAILED;
		errcode = _errcode;
		error = _error;
	}
	if(state == FORWARD_EVALING) {
		state = FORWARD;
	}
	throw mathexpr_error(_errcode, _error);
}

namespace
{
	/*
	X_EXPR -> VALUE
	X_EXPR -> STRING
	X_EXPR -> NONARY-OP
	X_EXPR -> FUNCTION X_ARGS
	X_EXPR -> UNARY-OP X_EXPR
	X_EXPR -> X_LAMBDA X_EXPR
	X_LAMBDA -> X_EXPR BINARY-OP
	X_ARGS -> OPEN-PAREN CLOSE_PAREN
	X_ARGS -> OPEN-PAREN X_TAIL
	X_TAIL -> X_PAIR X_TAIL
	X_TAIL -> X_EXPR CLOSE-PAREN
	X_PAIR -> X_EXPR COMMA
	 */

	//SUBEXPRESSION -> VALUE
	//SUBEXPRESSION -> STRING
	//SUBEXPRESSION -> FUNCTION OPEN-PAREN CLOSE-PAREN
	//SUBEXPRESSION -> FUNCTION OPEN-PAREN (SUBEXPRESSION COMMA)* SUBEXPRESSION CLOSE-PAREN
	//SUBEXPRESSION -> OPEN-PAREN SUBEXPRESSION CLOSE-PAREN
	//SUBEXPRESSION -> SUBEXPRESSION BINARY-OP SUBEXPRESSION
	//SUBEXPRESSION -> UNARY-OP SUBEXPRESSION
	//SUBEXPRESSION -> NONARY-OP

	bool is_alphanumeric(char ch)
	{
		if(ch >= '0' && ch <= '9') return true;
		if(ch >= 'a' && ch <= 'z') return true;
		if(ch >= 'A' && ch <= 'Z') return true;
		if(ch == '_') return true;
		if(ch == '.') return true;
		return false;
	}

	enum token_kind
	{
		TT_OPEN_PAREN,
		TT_CLOSE_PAREN,
		TT_COMMA,
		TT_FUNCTION,
		TT_OPERATOR,
		TT_VARIABLE,
		TT_VALUE,
		TT_STRING,
	};

	struct operations_set
	{
		operations_set(std::set<mathexpr_operinfo*>& ops)
			: operations(ops)
		{
		}
		mathexpr_operinfo* find_function(const std::string& name)
		{
			mathexpr_operinfo* fn = NULL;
			for(auto j : operations) {
				if(name == j->fnname && !j->is_operator)
					fn = j;
			}
			if(!fn) throw std::runtime_error("No such function '" + name + "'");
			return fn;
		}
		mathexpr_operinfo* find_operator(const std::string& name, unsigned arity)
		{
			for(auto j : operations) {
				if(name == j->fnname && j->is_operator && j->operands == arity)
					return j;
			}
			return NULL;
		}
	private:
		std::set<mathexpr_operinfo*>& operations;
	};

	struct subexpression
	{
		subexpression(token_kind k) : kind(k) {}
		subexpression(token_kind k, const std::string& str) : kind(k), string(str) {}
		token_kind kind;
		std::string string;
	};

	size_t find_last_in_sub(std::vector<subexpression>& ss, size_t first)
	{
		size_t depth;
		switch(ss[first].kind) {
		case TT_FUNCTION:
			if(first + 1 == ss.size() || ss[first + 1].kind != TT_OPEN_PAREN)
				throw std::runtime_error("Function requires argument list");
			first++;
		case TT_OPEN_PAREN:
			depth = 0;
			while(first < ss.size()) {
				if(ss[first].kind == TT_OPEN_PAREN)
					depth++;
				if(ss[first].kind == TT_CLOSE_PAREN)
					if(!--depth) break;
				first++;
			}
			if(first == ss.size())
				throw std::runtime_error("Unmatched '('");
			return first;
		case TT_CLOSE_PAREN:
			throw std::runtime_error("Unmatched ')'");
		case TT_COMMA:
			throw std::runtime_error("',' only allowed in function arguments");
		case TT_VALUE:
		case TT_STRING:
		case TT_OPERATOR:
		case TT_VARIABLE:
			return first;
		}
		throw std::runtime_error("Internal error (shouldn't be here)");
	}

	size_t find_end_of_arg(std::vector<subexpression>& ss, size_t first)
	{
		size_t depth = 0;
		while(first < ss.size()) {
			if(depth == 0 && ss[first].kind == TT_COMMA)
				return first;
			if(ss[first].kind == TT_OPEN_PAREN)
				depth++;
			if(ss[first].kind == TT_CLOSE_PAREN) {
				if(depth == 0)
					return first;
				depth--;
			}
			first++;
		}
		return ss.size();
	}

	struct expr_or_op
	{
		expr_or_op(gcroot_pointer<mathexpr> e) : expr(e), typei(NULL) {}
		expr_or_op(std::string o) : op(o), typei(NULL) {}
		gcroot_pointer<mathexpr> expr;
		std::string op;
		mathexpr_operinfo* typei;
	};

	gcroot_pointer<mathexpr> parse_rec(mathexpr_typeinfo& _type, std::vector<expr_or_op>& operands,
		size_t first, size_t last)
	{
		if(operands.empty())
			return gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type);
		if(last - first > 1) {
			//Find the highest percedence operator.
			size_t best = last;
			for(size_t i = first; i < last; i++) {
				if(operands[i].typei) {
					if(best == last)
						best = i;
					else if(operands[i].typei->precedence < operands[best].typei->precedence) {
						best = i;
					} else if(!operands[best].typei->rtl &&
						operands[i].typei->precedence == operands[best].typei->precedence) {
						best = i;
					}
				}
			}
			if(best == last) throw std::runtime_error("Internal error: No operands?");
			if(operands[best].typei->operands == 1) {
				//The operator is unary, collect up all following unary operators.
				size_t j = first;
				while(operands[j].typei)
					j++;
				std::vector<gcroot_pointer<mathexpr>> args;
				args.push_back(parse_rec(_type, operands, first + 1, j + 1));
				return gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type,
					operands[best].typei, args);
			} else {
				//Binary operator.
				std::vector<gcroot_pointer<mathexpr>> args;
				args.push_back(parse_rec(_type, operands, first, best));
				args.push_back(parse_rec(_type, operands, best + 1, last));
				return gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type,
					operands[best].typei, args);
			}
		}
		return operands[first].expr;
	}

	gcroot_pointer<mathexpr> parse_rec(mathexpr_typeinfo& _type, std::vector<subexpression>& ss,
		std::set<mathexpr_operinfo*>& operations,
		std::function<gcroot_pointer<mathexpr>(const std::string&)> vars, size_t first, size_t last)
	{
		operations_set opset(operations);
		std::vector<expr_or_op> operands;
		std::vector<gcroot_pointer<mathexpr>> args;
		mathexpr_operinfo* fn;
		for(size_t i = first; i < last; i++) {
			size_t l = find_last_in_sub(ss, i);
			if(l >= last) throw std::runtime_error("Internal error: Improper nesting");
			switch(ss[i].kind) {
			case TT_OPEN_PAREN:
				operands.push_back(parse_rec(_type, ss, operations, vars, i + 1, l));
				break;
			case TT_VALUE:
				operands.push_back(gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type,
					ss[i].string, false));
				break;
			case TT_STRING:
				operands.push_back(gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type,
					ss[i].string, true));
				break;
			case TT_VARIABLE:
				//We have to warp this is identify transform to make the evaluation lazy.
				operands.push_back(gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type,
					vars(ss[i].string)));
				break;
			case TT_FUNCTION:
				fn = opset.find_function(ss[i].string);
				i += 2;
				while(ss[i].kind != TT_CLOSE_PAREN) {
					size_t k = find_end_of_arg(ss, i);
					args.push_back(parse_rec(_type, ss, operations, vars, i, k));
					if(k < ss.size() && ss[k].kind == TT_COMMA)
						i = k + 1;
					else
						i = k;
				}
				operands.push_back(gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type, fn,
					args));
				args.clear();
				break;
			case TT_OPERATOR:
				operands.push_back(ss[i].string);
				break;
			case TT_CLOSE_PAREN:
			case TT_COMMA:
				;	//Can't appen.
			}
			i = l;
		}
		if(operands.empty())
			throw std::runtime_error("Empty subexpression");
		//Translate nonary operators to values.
		for(auto& i : operands) {
			if(!(bool)i.expr) {
				auto fn = opset.find_operator(i.op, 0);
				if(fn)
					i.expr = gcroot_pointer<mathexpr>(gcroot_pointer_object_tag(), &_type, fn,
						std::vector<gcroot_pointer<mathexpr>>());
			}
		}
		//Check that there aren't two consequtive subexpressions and mark operators.
		bool was_operand = false;
		for(auto& i : operands) {
			bool is_operand = (bool)i.expr;
			if(!is_operand && !was_operand)
				if(!(i.typei = opset.find_operator(i.op, 1)))
					throw std::runtime_error("'" + i.op + "' is not an unary operator");
			if(!is_operand && was_operand)
				if(!(i.typei = opset.find_operator(i.op, 2)))
					throw std::runtime_error("'" + i.op + "' is not a binary operator");
			if(was_operand && is_operand)
				throw std::runtime_error("Expected operator, got operand");
			was_operand = is_operand;
		}
		if(!was_operand)
			throw std::runtime_error("Expected operand, got end of subexpression");
		//Okay, now the expression has been reduced into series of operators and subexpressions.
		//If there are multiple consequtive operators, the first (except as first item) is binary,
		//and the others are unary.
		return parse_rec(_type, operands, 0, operands.size());
	}

	void tokenize(const std::string& expr, std::set<mathexpr_operinfo*>& operations,
		std::vector<subexpression>& tokenization)
	{
		for(size_t i = 0; i < expr.length();) {
			if(expr[i] == '(') {
				tokenization.push_back(subexpression(TT_OPEN_PAREN));
				i++;
			} else if(expr[i] == ')') {
				tokenization.push_back(subexpression(TT_CLOSE_PAREN));
				i++;
			} else if(expr[i] == ',') {
				tokenization.push_back(subexpression(TT_COMMA));
				i++;
			} else if(expr[i] == ' ') {
				i++;
			} else if(expr[i] == '$') {
				//Variable. If the next character is {, parse until }, otherwise parse until
				//non-alphanum.
				std::string varname = "";
				if(i + 1 < expr.length() && expr[i + 1] == '{') {
					//Terminate by '}'.
					i++;
					while(i + 1 < expr.length() && expr[i + 1] != '}')
						varname += std::string(1, expr[++i]);
					if(i + 1 >= expr.length() || expr[i + 1] != '}')
						throw std::runtime_error("${ without matching }");
					i++;
				} else {
					//Terminate by non-alphanum.
					while(i + 1 < expr.length() && is_alphanumeric(expr[i + 1]))
						varname += std::string(1, expr[++i]);
					}
				tokenization.push_back(subexpression(TT_VARIABLE, varname));
				i++;
			} else if(expr[i] == '"') {
				bool escape = false;
				size_t endpos = i;
				endpos++;
				while(endpos < expr.length() && (escape || expr[endpos] != '"')) {
					if(!escape) {
						if(expr[endpos] == '\\')
							escape = true;
						endpos++;
					} else {
						escape = false;
						endpos++;
					}
				}
				if(endpos == expr.length())
					throw std::runtime_error("Unmatched \"");
				//Copy (i,endpos-1) and descape.
				std::string tmp;
				escape = false;
				for(size_t j = i + 1; j < endpos; j++) {
					if(!escape) {
						if(expr[j] != '\\')
							tmp += std::string(1, expr[j]);
						else
							escape = true;
					} else {
						tmp += std::string(1, expr[j]);
						escape = false;
					}
				}
				tokenization.push_back(subexpression(TT_STRING, tmp));
				i = endpos + 1;
			} else {
				bool found = false;
				//Function names are only recognized if it begins here and is followed by '('.
				for(auto j : operations) {
					if(j->is_operator) continue; //Not a function.
					if(i + j->fnname.length() + 1 > expr.length()) continue; //Too long.
					if(expr[i + j->fnname.length()] != '(') continue; //Not followed by '('.
					for(size_t k = 0; k < j->fnname.length(); k++)
						if(expr[i + k] != j->fnname[k]) goto nomatch; //No match.
					tokenization.push_back(subexpression(TT_FUNCTION, j->fnname));
					i += j->fnname.length();
					found = true;
					break;
				nomatch: ;
				}
				if(found) continue;
				//Operators. These use longest match rule.
				size_t longest_match = 0;
				std::string op;
				for(auto j : operations) {
					if(!j->is_operator) continue; //Not an operator.
					if(i + j->fnname.length() > expr.length()) continue; //Too long.
					for(size_t k = 0; k < j->fnname.length(); k++)
						if(expr[i + k] != j->fnname[k]) goto next; //No match.
					if(j->fnname.length() <= longest_match) continue; //Not longest.
					found = true;
					op = j->fnname;
					longest_match = op.length();
				next:	;
				}
				if(found) {
					tokenization.push_back(subexpression(TT_OPERATOR, op));
					i += op.length();
					continue;
				}
				//Okay, token until next non-alphanum.
				std::string tmp;
				while(i < expr.length() && is_alphanumeric(expr[i]))
					tmp += std::string(1, expr[i++]);
				if(tmp.length()) {
					tokenization.push_back(subexpression(TT_VALUE, tmp));
					continue;
				}
				std::string summary;
				size_t j;
				size_t utfcount = 0;
				for(j = i; j < expr.length() && (j < i + 20 || utfcount); j++) {
					if(utfcount) utfcount--;
					summary += std::string(1, expr[j]);
					if((uint8_t)expr[j] >= 0xF0) utfcount = 3;
					if((uint8_t)expr[j] >= 0xE0) utfcount = 2;
					if((uint8_t)expr[j] >= 0xC0) utfcount = 1;
					if((uint8_t)expr[j] < 0x80) utfcount = 0;
				}
				if(j < expr.length()) summary += "[...]";
				throw std::runtime_error("Expression parse error, at '" + summary + "'");
			}
		}
	}
}

gcroot_pointer<mathexpr> mathexpr::parse(mathexpr_typeinfo& _type, const std::string& expr,
	std::function<gcroot_pointer<mathexpr>(const std::string&)> vars)
{
	if(expr == "")
		throw std::runtime_error("Empty expression");
	auto operations = _type.operations();
	std::vector<subexpression> tokenization;
	tokenize(expr, operations, tokenization);
	return parse_rec(_type, tokenization, operations, vars, 0, tokenization.size());
}
