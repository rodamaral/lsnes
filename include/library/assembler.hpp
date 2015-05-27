#ifndef _library__assembler__hpp__included__
#define _library__assembler__hpp__included__

#include <cstring>
#include "serialization.hpp"
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <map>
#include <vector>
#include <list>

namespace assembler
{
typedef size_t addr_t;
class label
{
public:
	label()
	{
		kind = L_LOCAL_U;
	}
	label(void* global)
	{
		kind = L_GLOBAL;
		addr = (addr_t)global;
	}
	label(const label& _base, int off)
	{
		switch(_base.kind) {
		case L_LOCAL_U:
			base = &_base;
			offset = off;
			kind = L_RELATIVE;
			break;
		case L_LOCAL_R:
			offset = _base.offset + off;
			kind = L_LOCAL_R;
			break;
		case L_GLOBAL:
			addr = _base.addr + off;
			kind = L_GLOBAL;
			break;
		case L_RELATIVE:
			base = _base.base;
			offset = _base.offset + off;
			kind = L_RELATIVE;
			break;
		}
	}
	void set(int off)
	{
		if(kind != L_LOCAL_U)
			throw std::runtime_error("Not undefined local label");
		offset = off;
		kind = L_LOCAL_R;
	}
	addr_t resolve(addr_t localbase) const
	{
		switch(kind) {
		case L_LOCAL_U: throw std::runtime_error("Unresolved label");
		case L_LOCAL_R: return localbase + offset;
		case L_GLOBAL: return addr;
		case L_RELATIVE: return base->resolve(localbase) + offset;
		}
		throw std::runtime_error("Unknown relocation type");
	}
private:
	enum _kind { L_LOCAL_U, L_LOCAL_R, L_GLOBAL, L_RELATIVE } kind;
	const struct label* base;
	addr_t addr;
	int offset;
};

class label_list
{
public:
	operator label&();
	operator label*();
	label& external(void* addr);
private:
	std::list<label> labels;
};

void i386_reloc_rel8(uint8_t* location, size_t target, size_t source);
void i386_reloc_rel16(uint8_t* location, size_t target, size_t source);
void i386_reloc_rel32(uint8_t* location, size_t target, size_t source);
void i386_reloc_abs32(uint8_t* location, size_t target, size_t source);
void i386_reloc_abs64(uint8_t* location, size_t target, size_t source);
uint8_t i386_modrm(uint8_t reg, uint8_t mod, uint8_t rm);
uint8_t i386_sib(uint8_t base, uint8_t index, uint8_t scale);

struct pad_tag
{
	pad_tag(size_t _amount) : amount(_amount) {}
	size_t amount;
};

struct label_tag
{
	label_tag(label& _l) : l(_l) {}
	label& l;
};

struct relocation_tag
{
	relocation_tag(std::function<void(uint8_t* location, size_t target, size_t source)> _promise,
		const label& _target) : promise(_promise), target(_target) {}
	std::function<void(uint8_t* location, size_t target, size_t source)> promise;
	const label& target;
};

struct byteseq_tag
{
	byteseq_tag(const uint8_t* ss, size_t _sl) { st.resize(_sl); memcpy(&st[0], ss, _sl); }
	std::vector<uint8_t> st;
};

template<typename T> byteseq_tag vle(T v)
{
	uint8_t b[sizeof(T)];
	serialization::write_common<T, false>(b, v);
	return byteseq_tag(b, sizeof(T));
}

template<typename T> byteseq_tag vbe(T v)
{
	uint8_t b[sizeof(T)];
	serialization::write_common<T, true>(b, v);
	return byteseq_tag(b, sizeof(T));
}

struct assembler
{
	assembler();
	template<typename... T> void operator()(uint8_t b, T... args)
	{
		byte(b);
		(*this)(args...);
	}
	template<typename... T> void operator()(pad_tag p, T... args)
	{
		pad(p.amount);
		(*this)(args...);
	}
	template<typename... T> void operator()(label_tag l, T... args)
	{
		label(l.l);
		(*this)(args...);
	}
	template<typename... T> void operator()(relocation_tag r, T... args)
	{
		relocation(r.promise, r.target);
		(*this)(args...);
	}
	template<typename... T> void operator()(byteseq_tag b, T... args)
	{
		size_t o = size();
		pad(b.st.size());
		memcpy(&data[o], &b.st[0], b.st.size());
		(*this)(args...);
	}
	void operator()() {}

	void _label(label& l);
	void _label(label& l, const std::string& globalname);
	void byte(uint8_t b);
	void byte(std::initializer_list<uint8_t> b);
	void byte(const uint8_t* b, size_t l);
	void relocation(std::function<void(uint8_t* location, size_t target, size_t source)> promise,
		const label& target);
	void align(size_t multiple);
	void pad(size_t amount);
	size_t size();
	void dump(const std::string& basename, const std::string& name, void* base, std::map<std::string, void*> map);
	std::map<std::string, void*> flush(void* base);
private:
	struct reloc
	{
		std::function<void(uint8_t* location, size_t target, size_t source)> promise;
		const label* target;
		size_t source;
	};
	std::vector<uint8_t> data;
	std::list<reloc> relocs;
	std::map<std::string, const label*> globals;
};

class dynamic_code
{
public:
	dynamic_code(size_t size);
	~dynamic_code();
	void commit();
	uint8_t* pointer() throw();
private:
	void* base;
	size_t asize;
};

}

#endif
