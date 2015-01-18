#include <regex>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <set>
#include <string>
#include <fstream>
#include <stdexcept>
#include <map>
#include <cstring>
#include <list>

#define ROMSIZE 32768

std::regex_constants::syntax_option_type regex_flags = std::regex::ECMAScript | std::regex::icase;

unsigned parse_arg(const std::string& arg);

struct location
{
public:
	location()
	{
		loc = "<unknown>";
	}

	location(const std::string& filename)
	{
		loc = filename;
	}

	location(const std::string& filename, uint64_t linenum)
	{
		std::ostringstream x;
		x << filename << ":" << linenum;
		loc = x.str();
	}

	operator std::string() const { return loc; }
private:
	std::string loc;
};

struct assembly_error : public std::runtime_error
{
public:
	assembly_error(const location& loc, const std::string& msg) : std::runtime_error(tostring(loc, msg)) {}
private:
	static std::string tostring(const location& loc, const std::string& msg)
	{
		std::ostringstream x;
		x << (std::string)loc << ": " << msg;
		return x.str();
	}
};

struct regex
{
private:
	bool something;
	std::regex r;
	size_t hash_start;
	size_t hash_end;
	unsigned hash;
	static unsigned hash_range(const std::string& str, size_t start, size_t end)
	{
		//This is FNV-1a.
		unsigned hash = 2166136261;
		for(size_t i = start; i < end; i++) {
			hash ^= ((unsigned char)str[i] & 0xdf);	//Mask bit for ignore case.
			hash *= 16777619;
		}
		return hash;
	}
	size_t pick_hash_end(const std::string& str)
	{
		size_t i;
		for(i = 0; i < str.length(); i++)
			if(strchr("^$\\.*+?()[]{}| ", (unsigned char)str[i]))
				break;
		return i;
	}
public:
	struct match
	{
	public:
		match() : m(false) {}
		match(std::list<std::string> sub) : m(true), ms(sub.begin(), sub.end()) {}
		operator bool() const { return m; }
		bool operator!() const { return !m; }
		size_t size() const { return ms.size(); }
		const std::string& operator[](size_t i) const { return ms[i]; }
private:
		bool m;
		std::vector<std::string> ms;
	};
	regex()
		: something(false), hash_start(0), hash_end(0), hash(0)
	{
	}
	regex(const std::string& rgx, std::regex_constants::syntax_option_type flags)
		: something(true), r(rgx, flags), hash_start(0), hash_end(pick_hash_end(rgx)),
		hash(hash_range(rgx, hash_start, hash_end))
	{
	}
	match operator()(const std::string& input)
	{
		std::smatch matches;
		if(!something || hash_end > input.length())
			return match();
		if(hash_range(input, hash_start, hash_end) != hash)
			return match();
		if(!std::regex_match(input, matches, r))
			return match();
		return match(std::list<std::string>(matches.begin(), matches.end()));
	}
};

template<typename T>
struct regex_map_match
{
public:
	regex_map_match(regex::match _m, const T& _a) : m(_m), a(_a) {}
	operator bool() const { return m; }
	bool operator!() const { return !m; }
	const regex::match& get_match() const { return m; }
	const T& get_arg() const { return a; }
private:
	regex::match m;
	T a;
};

template<typename T>
struct regex_map
{
public:
	void add(const regex& r, const T& arg)
	{
		exlist.push_back(std::make_pair(r, arg));
	}
	regex_map_match<T> operator()(const std::string& line)
	{
		regex::match m;
		for(auto& i : exlist) {
			if((m = i.first(line)))
				return regex_map_match<T>(m, i.second);
		}
		return regex_map_match<T>(regex::match(), T());
	}
private:
	std::list<std::pair<regex, T>> exlist;
};

struct instruction_data
{
	enum attr
	{
		BLANK = -1,
		NONE = 0,
		BYTE = 1,
		WORD = 2,
		REL = 3,
		LABEL = 4,
		MULTINOP = 5,
		RAWBYTES = 6,
	};
	instruction_data() : opcode(0), attribute(BLANK) {}
	instruction_data(attr _attr) : opcode(0), attribute(_attr) {}
	instruction_data(unsigned _opc, attr _attr) : opcode(_opc), attribute(_attr) {}
	unsigned opcode;
	attr attribute;
	static attr pattern_attribute(const char* pattern)
	{
		attr a = NONE;
		for(const char* i = pattern; *i; i++) {
			if(*i == '%') {
				switch(*(++i)) {
				case 'b':
					a = combine_attr(a, BYTE);
					break;
				case 'B':
					a = combine_attr(a, BYTE);
					break;
				case 'w':
					a = combine_attr(a, WORD);
					break;
				case 'r':
					a = combine_attr(a, REL);
					break;
				case 's':
					a = combine_attr(a, BYTE);
					break;
				case 'S':
					a = combine_attr(a, BYTE);
					break;
				}
			}
		}
		return a;
	}
	static regex pattern_regex(const char* pattern)
	{
		std::ostringstream x;
		for(const char* i = pattern; *i; i++) {
			int ch = (unsigned char)*i;
			if(ch == '%') {
				int ch2 = (unsigned char)*(++i);
				switch(ch2) {
				case 'b': x << "([[:xdigit:]]{2})"; break;
				case 'B': x << "([[:xdigit:]]{2}|/\\w+(\\.\\w+)?|/\\.\\w+)"; break;
				case 'w': x << "([[:xdigit:]]{4}|/\\w+(\\.\\w+)?|/\\.\\w+)"; break;
				case 'r': x << "(/\\w+(\\.\\w+)?|/\\.\\w+)"; break;
				case 's': x << "([+-][0-7][[:xdigit:]]|-80)"; break;
				case 'S': x << "([+-]?[0-7][[:xdigit:]]|-80)"; break;
				case '%': x << "%"; break;
				default: throw std::logic_error("Unknown % replacement");
				}
			} else if(strchr("^$\\.*+?()[]{}|", ch))
				x << "\\" << (char)ch;
			else if(ch == ',')
				x << " ?, ?";
			else
				x << (char)ch;
		}
		return regex(x.str(), regex_flags);
	}
private:
	static bool is_dummy_attr(attr a)
	{
		return (a == BLANK || a == NONE);
	}
	static attr combine_attr(attr old, attr _new)
	{
		if(!is_dummy_attr(old) && !is_dummy_attr(_new))
			throw std::runtime_error("Conflicting attributes");
		return is_dummy_attr(_new) ? old : _new;
	}
};

struct label
{
public:
	label() {}
	label(const std::string& _name, int _offset, location loc) : name(_name), offset(_offset), locator(loc) {}
	std::string get_name() { return name; }
	int get_offset() { return offset; }
	location get_location() { return locator; }
	void qualify(const std::string& _block) { if(name != "" && name[0] == '.') name = _block + name; }
private:
	std::string name;
	int offset;
	location locator;
};

struct region_map
{
	region_map(unsigned _total)
	{
		ranges.insert(std::make_pair(0, _total));
		used = 0;
	}
	void reserve(unsigned addr, unsigned size, const std::string& name)
	{
		if(!size)
			return;
		unsigned aaddr = 0, aend = 0;
		for(auto i : ranges) {
			if(i.first <= addr && i.second > addr) {
				aaddr = i.first;
				aend = i.second;
				break;
			}
		}
		if(aend < addr + size)
			throw std::runtime_error("Trying to reserve already reserved region");
		ranges.erase(std::make_pair(aaddr, aend));
		if(aaddr < addr)
			ranges.insert(std::make_pair(aaddr, addr));
		if(addr + size < aend)
			ranges.insert(std::make_pair(addr + size, aend));
		if(getenv("GBASM_SHOW_LAYOUT")) {
			std::ostringstream x;
			x << "Reserved " << std::hex << addr << "-" << (addr + size - 1) << " for " << name
				<< std::endl;
			std::cout << x.str();
		}
		used = std::max(used, addr + size);
	}
	unsigned alloc(unsigned size, const std::string& name)
	{
		if(!size)
			return 0;
		unsigned aaddr = 0, aend = 0;
		for(auto i : ranges) {
			if(i.second - i.first >= size) {
				aaddr = i.first;
				aend = i.second;
				break;
			}
		}
		if(aend - aaddr < size)
			throw std::runtime_error("Out of ROM space");
		ranges.erase(std::make_pair(aaddr, aend));
		if(aaddr + size < aend)
			ranges.insert(std::make_pair(aaddr + size, aend));
		used = std::max(used, aaddr + size);
		if(getenv("GBASM_SHOW_LAYOUT")) {
			std::ostringstream x;
			x << "Allocated " << std::hex << aaddr << "-" << (aaddr + size - 1) << " for " << name
				<< std::endl;
			std::cout << x.str();
		}
		return aaddr;
	}
	size_t get_used() { return used; }
private:
	std::set<std::pair<unsigned, unsigned>> ranges;
	unsigned used;
};

struct block
{
	enum reloc
	{
		REL = 0,
		ABS = 1,
		FFABS = 2,
	};
	block()
	{
		assigned_addr = -1;
	}
	block(const std::string& _name, const location& _locator)
	{
		assigned_addr = -1;
		name = _name;
		locator = _locator;
	}

	const std::string& get_name() { return name; }
	const location& get_location() { return locator; }
	bool has_assigned_addr() { return (assigned_addr >= 0); }
	unsigned get_assigned_addr() { return assigned_addr; }
	void assign_address(unsigned addr, std::map<std::string, unsigned>& lmap) {
		assigned_addr = addr;
		for(auto& i : labels) lmap[i.get_name()] = assigned_addr + i.get_offset();
	}
	unsigned get_size() { return bytes.size(); }
	void qualify_labels() {
		for(auto& j : relocations) j.first.qualify(name);
		for(auto& j : labels) j.qualify(name);
	}
	void add_label(const std::string& _name, int _offset, location loc)
	{
		labels.push_back(label(_name, _offset, loc));
	}
	void parse_line(regex_map<instruction_data>& ptable, const std::string& line, const location& locator)
	{
		if(line == "") return;
		auto idata = ptable(line);
		if(idata) {
			auto& idatam = idata.get_match();
			auto& idataa = idata.get_arg();
			std::string arg = (idatam.size() > 1) ? idatam[1] : "";
			if(idataa.attribute == instruction_data::LABEL) {
				add_label(arg, bytes.size(), locator);
			} else if(idataa.attribute == instruction_data::RAWBYTES) {
				add_bytes(arg);
			} else {
				add_instruction(idataa.opcode, idataa.attribute, arg, locator);
			}
			return;
		}
		throw std::runtime_error("Unrecognized instruction");
	}
	void write(unsigned char* rom, std::map<std::string, unsigned>& labels)
	{
		if(!has_assigned_addr() || get_assigned_addr() + get_size() > 32768)
			throw assembly_error(locator, "Invalid block assigned address.");
		unsigned addrbase = get_assigned_addr();
		int offset;
		for(size_t j = 0; j < bytes.size(); j++)
			rom[addrbase + j] = bytes[j];
		for(auto& i : relocations) {
			unsigned roff = addrbase + i.first.get_offset();
			location errloc = i.first.get_location();
			if(!labels.count(i.first.get_name()))
				throw assembly_error(errloc, "Undefined reference to '" + i.first.get_name() +
					"'");
			unsigned loff = labels[i.first.get_name()];
			switch(i.second) {
			case block::REL:
				offset = loff - (roff + 1);
				if(offset < -128 || offset > 127)
					throw assembly_error(errloc, "Jump out of range");
				rom[roff] = offset & 0xFF;
				break;
			case block::ABS:
				rom[roff] = loff & 0xFF;
				rom[roff + 1] = loff >> 8;
				break;
			case block::FFABS:
				if((loff >> 8) != 0xFF)
					throw assembly_error(errloc, "Label in ldh does not point to FFxx");
				rom[roff] = loff & 0xFF;
				break;
			}
		}
	}

	void check_duplicate_labels(std::map<std::string, location>& labels_seen)
	{
		for(auto& j : labels) {
			if(labels_seen.count(j.get_name()))
				throw assembly_error(j.get_location(), "Duplicate label'" + j.get_name() +
					"' (previously seen at " + (std::string)labels_seen[j.get_name()] + ")");
			labels_seen[j.get_name()] = j.get_location();
		}
	}

	static void make_regex_table(regex_map<instruction_data>& table, const char** patterns)
	{
		for(unsigned i = 0; i < 512; i++) {
			if(!patterns[i]) continue;
			instruction_data::attr a = instruction_data::pattern_attribute(patterns[i]);
			auto rgx = instruction_data::pattern_regex(patterns[i]);
			table.add(rgx, instruction_data(i, a));
		}
		for(unsigned i = 0; i < 256; i++) {
			char buf[6] = {'x', 'x', 'x', 0, 0, 0};
			buf[4] = "0123456789abcdef"[i / 16];
			buf[5] = "0123456789abcdef"[i % 16];
			table.add(regex(buf, regex_flags), instruction_data(i, instruction_data::NONE));
		}
		//The label does not allow component before '.'.
		table.add(regex("(.?\\w+):", regex_flags), instruction_data(instruction_data::LABEL));
		table.add(regex("nops ([[:xdigit:]]+|\\$\\S+)", regex_flags),
			instruction_data(instruction_data::MULTINOP));
		table.add(regex("data ([[:xdigit:]]{2}( [[:xdigit:]]{2})*)", regex_flags),
			instruction_data(instruction_data::RAWBYTES));
	}
private:
	std::list<label> labels;
	std::list<std::pair<label, reloc>> relocations;
	std::vector<char> bytes;
	std::string name;
	location locator;
	signed assigned_addr;
	void add_bytes(std::string arg)
	{
		size_t ptr = 0;
		while(ptr < arg.length()) {
			size_t nptr = arg.find_first_of(" ", ptr);
			std::string sub;
			if(nptr == std::string::npos) {
				sub = arg.substr(ptr);
				ptr = arg.length();
			} else if(nptr == ptr) {
				ptr++;
				continue;
			} else {
				sub = arg.substr(ptr, nptr - ptr);
				ptr = nptr;
			}
			bytes.push_back(parse_arg(sub) & 0xFF);
		}
	}
	void add_instruction(unsigned opcode, instruction_data::attr attr, const std::string& arg,
		const location& _locator)
	{
		auto _arg = arg;
		bool islabel = (arg.length() > 0 && arg[0] == '/');
		std::string labeltrg = islabel ? _arg.substr(1) : arg;
		unsigned val = 0;
		switch(attr) {
		case instruction_data::BYTE:
			emit_opcode(opcode);
			if(islabel) {
				relocations.push_back(std::make_pair(label(labeltrg, bytes.size(), _locator), FFABS));
			} else {
				val = parse_arg(arg);
			}
			bytes.push_back(val & 0xFF);
		case instruction_data::BLANK:
			break;
		case instruction_data::NONE:
			emit_opcode(opcode);
			break;
		case instruction_data::REL:
			emit_opcode(opcode);
			relocations.push_back(std::make_pair(label(labeltrg, bytes.size(), _locator), REL));
			bytes.push_back(0);
			break;
		case instruction_data::WORD:
			emit_opcode(opcode);
			if(islabel) {
				relocations.push_back(std::make_pair(label(labeltrg, bytes.size(), _locator), ABS));
			} else {
				val = parse_arg(arg);
			}
			bytes.push_back(val & 0xFF);
			bytes.push_back(val >> 8);
			break;
		case instruction_data::MULTINOP:
			for(unsigned i = 0; i < parse_arg(arg); i++)
				bytes.push_back(0);
			break;
		default:
			throw std::logic_error("Unknown attribute type");
		}
	}
	void emit_opcode(unsigned opc)
	{
		switch(opc >> 8) {
		case 0:
			break;
		case 1:
			bytes.push_back((char)0xcb);
			break;
		default:
			throw std::logic_error("Opcode out of range");
		};
		bytes.push_back(opc & 0xFF);
	}
};

struct block_list
{
public:
	struct special_block
	{
		const char* name;
		std::function<void(block&)> handler;
	};
	block_list() {}
	~block_list()
	{
		for(auto i : blocks) delete i.second;
	}
	block* create_block(const std::string& _name, const location& _locator)
	{
		if(blocks_seen.count(_name)) {
			throw assembly_error(_locator, "Duplicate block '" + _name +
				"' (previously seen at " + (std::string)blocks_seen[_name] + ")");
		}
		blocks[_name] = new block(_name, _locator);
		blocks_seen[_name] = _locator;
		return blocks[_name];
	}
	void check_duplicates()
	{
		for(auto& i : blocks) i.second->qualify_labels();
		std::map<std::string, location> labels_seen;
		for(auto& i : blocks)
			i.second->check_duplicate_labels(labels_seen);
	}
	void write(unsigned char* rom, std::map<std::string, unsigned>& labels)
	{
		for(auto& i : blocks) i.second->write(rom, labels);
	}
	size_t layout(std::map<std::string, unsigned>& lmap, std::initializer_list<special_block> specials)
	{
		//The following blocknames are special:
		//__start: Placed at 0x100, must be at most 4 bytes.
		//__header: Placed at 0x134, must be 25 bytes.
		//__fixed150: Placed at 0x150.
		//__freestanding_labels: Placed at 0x0, there may be multiple, must have size 0.
		region_map rmap(ROMSIZE);
		for(auto& i : blocks) {
			for(auto j : specials)
				if(i.second->get_name() == j.name)
					j.handler(*i.second);
		}

		//Allocate/Reserve space.
		location fault_location;
		try {
			//Reserve the autogenerated stuff.
			rmap.reserve(0x104, 0x30, "<Magic>");		//Magic.
			rmap.reserve(0x14D, 3, "<Checksums>");		//Checksum.
			//We do reservations before allocations.
			for(auto& i : blocks) {
				fault_location = i.second->get_location();
				if(i.second->has_assigned_addr())
					rmap.reserve(i.second->get_assigned_addr(), i.second->get_size(),
						"Block: " + i.second->get_name());
			}
			for(auto& i : blocks) {
				fault_location = i.second->get_location();
				if(!i.second->has_assigned_addr())
					i.second->assign_address(rmap.alloc(i.second->get_size(),
						"Block: " + i.second->get_name()), lmap);
			}
		} catch(std::exception& e) {
			throw assembly_error(fault_location, e.what());
		}
		return rmap.get_used();
	}
private:
	block_list(const block_list&);
	block_list& operator=(const block_list&);
	std::map<std::string, location> blocks_seen;
	std::map<std::string, block*> blocks;
};

const char* ins_patterns[512] = {
	"nop",         "ld bc,%w",  "ld (bc),a",  "inc bc",    "inc b",     "dec b",     "ld b,%b",     "rcla",
	"ld (%w),sp",  "add hl,bc", "ld a,(bc)",  "dec bc",    "inc c",     "dec c",     "ld c,%b",     "rrca",
	"stop",        "ld de,%w",  "ld (de),a",  "inc de",    "inc d",     "dec d",     "ld d,%b",     "rla",
	"jr %r",       "add hl,de", "ld a,(de)",  "dec de",    "inc e",     "dec e",     "ld e,%b",     "rra",
	"jrnz %r",     "ld hl,%w",  "ld (hl+),a", "inc hl",    "inc h",     "dec h",     "ld h,%b",     "daa",
	"jrz %r",      "add hl,hl", "ld a,(hl+)", "dec hl",    "inc l",     "dec l",     "ld l,%b",     "cpl",
	"jrnc %r",     "ld sp,%w",  "ld (hl-),a", "inc sp",    "inc (hl)",  "dec (hl)",  "ld (hl),%b",  "scf",
	"jrc %r",      "add hl,sp", "ld a,(hl-)", "dec sp",    "inc a",     "dec a",     "ld a,%b",     "ccf",

	"ld b,b",      "ld b,c",    "ld b,d",     "ld b,e",    "ld b,h",    "ld b,l",    "ld b,(hl)",   "ld b,a",
	"ld c,b",      "ld c,c",    "ld c,d",     "ld c,e",    "ld c,h",    "ld c,l",    "ld c,(hl)",   "ld c,a",
	"ld d,b",      "ld d,c",    "ld d,d",     "ld d,e",    "ld d,h",    "ld d,l",    "ld d,(hl)",   "ld d,a",
	"ld e,b",      "ld e,c",    "ld e,d",     "ld e,e",    "ld e,h",    "ld e,l",    "ld e,(hl)",   "ld e,a",
	"ld h,b",      "ld h,c",    "ld h,d",     "ld h,e",    "ld h,h",    "ld h,l",    "ld h,(hl)",   "ld h,a",
	"ld l,b",      "ld l,c",    "ld l,d",     "ld l,e",    "ld l,h",    "ld l,l",    "ld l,(hl)",   "ld l,a",
	"ld (hl),b",   "ld (hl),c", "ld (hl),d",  "ld (hl),e", "ld (hl),h", "ld (hl),l", "halt",        "ld (hl),a",
	"ld a,b",      "ld a,c",    "ld a,d",     "ld a,e",    "ld a,h",    "ld a,l",    "ld a,(hl)",   "ld a,a",

	"add b",       "add c",     "add d",      "add e",     "add h",     "add l",     "add (hl)",    "add a",
	"adc b",       "adc c",     "adc d",      "adc e",     "adc h",     "adc l",     "adc (hl)",    "adc a",
	"sub b",       "sub c",     "sub d",      "sub e",     "sub h",     "sub l",     "sub (hl)",    "sub a",
	"sbc b",       "sbc c",     "sbc d",      "sbc e",     "sbc h",     "sbc l",     "sbc (hl)",    "sbc a",
	"and b",       "and c",     "and d",      "and e",     "and h",     "and l",     "and (hl)",    "and a",
	"xor b",       "xor c",     "xor d",      "xor e",     "xor h",     "xor l",     "xor (hl)",    "xor a",
	"or b",        "or c",      "or d",       "or e",      "or h",      "or l",      "or (hl)",     "or a",
	"cp b",        "cp c",      "cp d",       "cp e",      "cp h",      "cp l",      "cp (hl)",     "cp a",

	"retnz",       "pop bc",    "jpnz %w",    "jp %w",     "callnz %w", "push bc",   "add %b",      "rst00",
	"retz",        "ret",       "jpz %w",     NULL,        "callz %w",  "call %w",   "adc %b",      "rst08",
	"retnc",       "pop de",    "jpnc %w",    NULL,        "callnc %w", "push de",   "sub %b",      "rst10",
	"retc",        "reti",      "jpc %w",     NULL,        "callc %w",  NULL,        "sbc %b",      "rst18",
	"ldh (%B),a",  "pop hl",    "ldh (c),a",  NULL,        NULL,        "push hl",   "and %b",      "rst20",
	"add sp,%S",   "jp (hl)",   "ld (%w),a",  NULL,        NULL,        NULL,        "xor %b",      "rst28",
	"ldh a,(%B)",  "pop af",    "ldh a,(c)",  "di",        NULL,        "push af",   "or %b",       "rst30",
	"ld hl,sp%s",  "ld sp,hl",  "ld a,(%w)",  "ei",        NULL,        NULL,        "cp %b",       "rst38",

	"rlc b",       "rlc c",     "rlc d",      "rlc e",     "rlc h",     "rlc l",     "rlc (hl)",    "rlc a",
	"rrc b",       "rrc c",     "rrc d",      "rrc e",     "rrc h",     "rrc l",     "rrc (hl)",    "rrc a",
	"rl b",        "rl c",      "rl d",       "rl e",      "rl h",      "rl l",      "rl (hl)",     "rl a",
	"rr b",        "rr c",      "rr d",       "rr e",      "rr h",      "rr l",      "rr (hl)",     "rr a",
	"sla b",       "sla c",     "sla d",      "sla e",     "sla h",     "sla l",     "sla (hl)",    "sla a",
	"sra b",       "sra c",     "sra d",      "sra e",     "sra h",     "sra l",     "sra (hl)",    "sra a",
	"swap b",      "swap c",    "swap d",     "swap e",    "swap h",    "swap l",    "swap (hl)",   "swap a",
	"srl b",       "srl c",     "srl d",      "srl e",     "srl h",     "srl l",     "srl (hl)",    "srl a",

	"bit0 b",      "bit0 c",    "bit0 d",     "bit0 e",    "bit0 h",    "bit0 l",    "bit0 (hl)",   "bit0 a",
	"bit1 b",      "bit1 c",    "bit1 d",     "bit1 e",    "bit1 h",    "bit1 l",    "bit1 (hl)",   "bit1 a",
	"bit2 b",      "bit2 c",    "bit2 d",     "bit2 e",    "bit2 h",    "bit2 l",    "bit2 (hl)",   "bit2 a",
	"bit3 b",      "bit3 c",    "bit3 d",     "bit3 e",    "bit3 h",    "bit3 l",    "bit3 (hl)",   "bit3 a",
	"bit4 b",      "bit4 c",    "bit4 d",     "bit4 e",    "bit4 h",    "bit4 l",    "bit4 (hl)",   "bit4 a",
	"bit5 b",      "bit5 c",    "bit5 d",     "bit5 e",    "bit5 h",    "bit5 l",    "bit5 (hl)",   "bit5 a",
	"bit6 b",      "bit6 c",    "bit6 d",     "bit6 e",    "bit6 h",    "bit6 l",    "bit6 (hl)",   "bit6 a",
	"bit7 b",      "bit7 c",    "bit7 d",     "bit7 e",    "bit7 h",    "bit7 l",    "bit7 (hl)",   "bit7 a",

	"res0 b",      "res0 c",    "res0 d",     "res0 e",    "res0 h",    "res0 l",    "res0 (hl)",   "res0 a",
	"res1 b",      "res1 c",    "res1 d",     "res1 e",    "res1 h",    "res1 l",    "res1 (hl)",   "res1 a",
	"res2 b",      "res2 c",    "res2 d",     "res2 e",    "res2 h",    "res2 l",    "res2 (hl)",   "res2 a",
	"res3 b",      "res3 c",    "res3 d",     "res3 e",    "res3 h",    "res3 l",    "res3 (hl)",   "res3 a",
	"res4 b",      "res4 c",    "res4 d",     "res4 e",    "res4 h",    "res4 l",    "res4 (hl)",   "res4 a",
	"res5 b",      "res5 c",    "res5 d",     "res5 e",    "res5 h",    "res5 l",    "res5 (hl)",   "res5 a",
	"res6 b",      "res6 c",    "res6 d",     "res6 e",    "res6 h",    "res6 l",    "res6 (hl)",   "res6 a",
	"res7 b",      "res7 c",    "res7 d",     "res7 e",    "res7 h",    "res7 l",    "res7 (hl)",   "res7 a",

	"set0 b",      "set0 c",    "set0 d",     "set0 e",    "set0 h",    "set0 l",    "set0 (hl)",   "set0 a",
	"set1 b",      "set1 c",    "set1 d",     "set1 e",    "set1 h",    "set1 l",    "set1 (hl)",   "set1 a",
	"set2 b",      "set2 c",    "set2 d",     "set2 e",    "set2 h",    "set2 l",    "set2 (hl)",   "set2 a",
	"set3 b",      "set3 c",    "set3 d",     "set3 e",    "set3 h",    "set3 l",    "set3 (hl)",   "set3 a",
	"set4 b",      "set4 c",    "set4 d",     "set4 e",    "set4 h",    "set4 l",    "set4 (hl)",   "set4 a",
	"set5 b",      "set5 c",    "set5 d",     "set5 e",    "set5 h",    "set5 l",    "set5 (hl)",   "set5 a",
	"set6 b",      "set6 c",    "set6 d",     "set6 e",    "set6 h",    "set6 l",    "set6 (hl)",   "set6 a",
	"set7 b",      "set7 c",    "set7 d",     "set7 e",    "set7 h",    "set7 l",    "set7 (hl)",   "set7 a",
};

unsigned hexadecimal_value(char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	throw std::runtime_error("Invalid hexadecimal character");
}

unsigned parse_arg(const std::string& arg)
{
	if(arg.length() > 0 && arg[0] == '$') {
		std::string _arg = arg;
		const char* x = getenv(_arg.substr(1).c_str());
		if(!x)
			throw std::runtime_error("Undefined environment variable");
		return (unsigned)atoi(x);
	}
	unsigned off = 0;
	unsigned val = 0;
	bool neg = false;
	if(arg == "")
		throw std::runtime_error("Empty byte argument");
	if(arg[0] == '+')
		off = 1;
	if(arg[0] == '-') {
		neg = true;
		off = 1;
	}
	for(size_t i = off; i < arg.length(); i++)
		val = val * 16 + hexadecimal_value(arg[i]);
	if(neg)
		val = -val;
	return val;
}

std::string trimline(const std::string& line)
{
	std::ostringstream x;
	bool seen_nws = false;
	bool seen_ws = false;
	for(size_t i = 0; i < line.length(); i++) {
		char ch = line[i];
		if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\v' || ch == '\n') {
			seen_ws = true;
		} else if(ch == '#') {
			break;
		} else {
			if(seen_ws && seen_nws)
				x << " ";
			seen_ws = false;
			seen_nws = true;
			x << ch;
		}
	}
	return x.str();
}

enum nb_command
{
	NBC_NONE = 0,
	NBC_BLOCK,
	NBC_INCLUDE,
	NBC_LABEL,
};

std::pair<nb_command, std::string> parse_line_noblock(const std::string& line)
{
	static regex bex("block (\\w+)", regex_flags);
	static regex bex2("include (.*)", regex_flags);
	static regex bex4("label (\\w+) ([[:xdigit:]]{4})", regex_flags);

	regex::match matches;
	if(line == "")
		return std::make_pair(NBC_NONE, "");

	if((matches = bex(line))) return std::make_pair(NBC_BLOCK, matches[1]);
	if((matches = bex2(line))) return std::make_pair(NBC_INCLUDE, matches[1]);
	if((matches = bex4(line)))
		return std::make_pair(NBC_LABEL, std::string(matches[1]) + " " + std::string(matches[2]));
	throw std::runtime_error("Unrecognized command");
}

void parse_file(block_list& blocks, regex_map<instruction_data>& ptable, const std::string& fname,
	std::set<std::string>& parsed, block& freelabels)
{
	static regex ebre("endblock", regex_flags);

	if(parsed.count(fname))
		return;	//Already parsed.
	parsed.insert(fname);

	std::ifstream file(fname);
	if(!file)
		throw assembly_error(location(fname), "Can't open file");

	unsigned lnum = 1;
	struct block* bl = NULL;
	std::string _line;
	while(file) {
		location locator(fname, lnum);
		std::getline(file, _line);
		std::string line = trimline(_line);
		try {
			if(bl) {
				if(ebre(line)) {
					bl = NULL;
					goto out;
				}
				bl->parse_line(ptable, line, locator);
			} else {
				auto x = parse_line_noblock(line);
				switch(x.first) {
				case NBC_BLOCK: {
					bl = blocks.create_block(x.second, locator);
					//The name of block automatically becomes a label.
					bl->add_label(x.second, 0, locator);
					break;
				}
				case NBC_INCLUDE: {
					parse_file(blocks, ptable, x.second, parsed, freelabels);
					break;
				}
				case NBC_LABEL: {
					size_t split = x.second.find_first_of(" ");
					std::string f1 = x.second.substr(0, split);
					std::string f2 = x.second.substr(split + 1);
					unsigned v = parse_arg(f2);
					freelabels.add_label(f1, v, locator);
					break;
				}
				case NBC_NONE:
					break;
				}
			}
		} catch(std::exception& e) {
			throw assembly_error(locator, e.what());
		}
out:
		lnum++;
	}
}

void fix_checksums(unsigned char* rom)
{
	unsigned char hdrc = 0;
	for(unsigned i = 0x134; i < 0x14D; i++)
		hdrc = hdrc - rom[i] - 1;
	unsigned short rchk = 0;
	for(unsigned i = 0; i < ROMSIZE; i++)
		rchk = rchk + (unsigned char)rom[i];
	rom[0x14D] = hdrc;
	rom[0x14E] = rchk >> 8;
	rom[0x14F] = rchk & 0xFF;
}

void clear_rom(unsigned char* rom)
{
	memset(rom, 0, ROMSIZE);
	const unsigned char magic[] = {
		0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
		0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
		0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
	};
	memcpy(rom + 0x104, magic, sizeof(magic));
}

size_t assemble(regex_map<instruction_data>& ptable, const std::string& tfile, const std::string& sfile)
{
	std::set<std::string> parsed;
	std::map<std::string, unsigned> labels;
	block_list blocks;

	unsigned char rom[ROMSIZE];
	block& freelabels = *blocks.create_block("__freestanding_labels", location());
	parse_file(blocks, ptable, sfile, parsed, freelabels);

	blocks.check_duplicates();
	bool found_start = false;
	bool found_header = false;
	auto ret = blocks.layout(labels, {
		{"__start", [&found_start, &labels](block& b) {
			b.assign_address(0x100, labels);
			if(b.get_size() > 4)
				throw assembly_error(b.get_location(), "__start block too long (max 4 bytes)");
			found_start = true;
		}},{"__header", [&found_header, &labels](block& b) {
			b.assign_address(0x134, labels);
			if(b.get_size() != 25)
				throw assembly_error(b.get_location(), "__header block wrong size (!=25 bytes)");
			found_header = true;
		}},{"__freestanding_labels", [&labels](block& b) {
			b.assign_address(0, labels);
			if(b.get_size() != 0)
				throw assembly_error(b.get_location(), "Freestanding label block not empty");
		}},{"__fixed150", [&labels](block& b) {
			b.assign_address(0x150, labels);
		}}
	});
	if(!found_start) throw std::runtime_error("No __start block found");
	if(!found_header) throw std::runtime_error("No __header block found");
	clear_rom(rom);
	blocks.write(rom, labels);
	fix_checksums(rom);

	std::ofstream target(tfile, std::ios::binary);
	if(!target)
		throw assembly_error(location(tfile), "Can't open output file");
	target.write((char*)rom, ROMSIZE);
	if(!target)
		throw assembly_error(location(tfile), "Can't write output file");
	return ret;
}

std::string get_output_filename(std::string input_fn)
{
	size_t sep = input_fn.find_last_of(".");
	if(sep < input_fn.length())
		input_fn = input_fn.substr(0, sep);
	return input_fn + ".gb";
}

int main(int argc, char** argv)
{
	regex_map<instruction_data> ptable;
	bool fail = false;
	try {
		block::make_regex_table(ptable, ins_patterns);
	} catch(std::exception& e) {
		std::cerr << "Error constructing parse table: " << e.what() << std::endl;
		return 2;
	}
	for(int i = 1; i < argc; i++) {
		std::string fname = argv[i];
		try {
			auto size = assemble(ptable, get_output_filename(fname), fname);
			std::cout << "Assembled '" << fname << "' (" << size << " bytes used)" << std::endl;
		} catch(std::exception& e) {
			std::cerr << "Failed to assemble '" << fname << "': " << e.what() << std::endl;
			fail = true;
		}
	}
	return fail ? 1 : 0;
}
