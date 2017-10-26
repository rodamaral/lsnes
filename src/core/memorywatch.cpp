#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/memorywatch.hpp"
#include "core/messages.hpp"
#include "core/project.hpp"
#include "core/rom.hpp"
#include "core/window.hpp"
#include "fonts/wrapper.hpp"
#include "library/directory.hpp"
#include "library/framebuffer-font2.hpp"
#include "library/globalwrap.hpp"
#include "library/int24.hpp"
#include "library/mathexpr-ntype.hpp"
#include "library/memoryspace.hpp"
#include "library/memorywatch-fb.hpp"
#include "library/memorywatch.hpp"
#include "library/memorywatch-list.hpp"
#include "library/memorywatch-null.hpp"
#include "library/string.hpp"

#include <functional>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <iomanip>
#include <stack>
#include <cmath>
#include <sstream>
#include <map>

namespace
{
	globalwrap<std::map<std::string, std::pair<framebuffer::font2*, size_t>>> S_fonts_in_use;

	framebuffer::font2& get_builtin_font2()
	{
		static framebuffer::font2 f(main_font);
		return f;
	}

	framebuffer::font2* get_font(const std::string filename)
	{
		//Handle NULL font.
		if(filename == "")
			return &get_builtin_font2();
		std::string abs_filename = directory::absolute_path(filename);
		if(S_fonts_in_use().count(abs_filename)) {
			S_fonts_in_use()[abs_filename].second++;
			return S_fonts_in_use()[abs_filename].first;
		}
		framebuffer::font2* f = new framebuffer::font2(abs_filename);
		try {
			S_fonts_in_use()[abs_filename] = std::make_pair(f, 1);
		} catch(...) {
			delete f;
			throw;
		}
		return f;
	}
	void put_font(framebuffer::font2* font)
	{
		//Handle NULL font (always there).
		if(!font)
			return;
		//Find font using this.
		std::string filename;
		for(auto& i : S_fonts_in_use())
			if(i.second.first == font)
				filename = i.first;
		if(filename == "")
			return;
		S_fonts_in_use()[filename].second--;
		if(!S_fonts_in_use()[filename].second) {
			delete S_fonts_in_use()[filename].first;
			S_fonts_in_use().erase(filename);
		}
	}

	std::string json_string_default(const JSON::node& node, const std::string& pointer, const std::string& dflt)
	{
		return (node.type_of(pointer) == JSON::string) ? node[pointer].as_string8() : dflt;
	}

	uint64_t json_unsigned_default(const JSON::node& node, const std::string& pointer, uint64_t dflt)
	{
		return (node.type_of(pointer) == JSON::number) ? node[pointer].as_uint() : dflt;
	}

	int64_t json_signed_default(const JSON::node& node, const std::string& pointer, int64_t dflt)
	{
		return (node.type_of(pointer) == JSON::number) ? node[pointer].as_int() : dflt;
	}

	bool json_boolean_default(const JSON::node& node, const std::string& pointer, bool dflt)
	{
		return (node.type_of(pointer) == JSON::boolean) ? node[pointer].as_bool() : dflt;
	}

	void dummy_target_fn(const std::string& n, const std::string& v) {}


	struct regread_oper : public mathexpr::operinfo
	{
		regread_oper();
		~regread_oper();
		//The first promise is the register name.
		void evaluate(mathexpr::value target, std::vector<std::function<mathexpr::value()>> promises);
		//Fields.
		bool signed_flag;
		loaded_rom* rom;
	};

	regread_oper::regread_oper()
		: operinfo("(readregister)")
	{
		signed_flag = false;
		rom = NULL;
	}
	regread_oper::~regread_oper()
	{
	}
	void regread_oper::evaluate(mathexpr::value target, std::vector<std::function<mathexpr::value()>> promises)
	{
		if(promises.size() != 1)
			throw mathexpr::error(mathexpr::error::ARGCOUNT, "register read operator takes 1 argument");
		std::string rname;
		try {
			mathexpr::value val = promises[0]();
			void* res = val._value;
			rname = val.type->tostring(res);
		} catch(std::exception& e) {
			throw mathexpr::error(mathexpr::error::ADDR, e.what());
		}
		const interface_device_reg* regs = rom->get_registers();
		bool found = false;
		for(size_t i = 0; regs && regs[i].name; i++) {
			if(rname != regs[i].name)
				continue;
			found = true;
			if(regs[i].boolean) {
				bool v = (regs[i].read() != 0);
				target.type->parse_b(target._value, v);
			} else if(signed_flag) {
				int64_t v = regs[i].read();
				target.type->parse_s(target._value, v);
			} else {
				uint64_t v = regs[i].read();
				target.type->parse_u(target._value, v);
			}
			return;
		}
		if(!found) {
			//N/A value.
			throw mathexpr::error(mathexpr::error::ADDR, "No such register");
		}
	}
}

memwatch_printer::memwatch_printer()
{
	position = PC_MEMORYWATCH;
	cond_enable = false;
	onscreen_alt_origin_x = false;
	onscreen_alt_origin_y = false;
	onscreen_cliprange_x = false;
	onscreen_cliprange_y = false;
	onscreen_fg_color = 0xFFFFFF;
	onscreen_bg_color = -1;
	onscreen_halo_color = 0;
}

JSON::node memwatch_printer::serialize()
{
	JSON::node ndata(JSON::object);
	switch(position) {
	case PC_DISABLED:	ndata["position"] = JSON::s("disabled"); break;
	case PC_MEMORYWATCH:	ndata["position"] = JSON::s("memorywatch"); break;
	case PC_ONSCREEN:	ndata["position"] = JSON::s("onscreen"); break;
	};
	ndata["cond_enable"] = JSON::b(cond_enable);
	ndata["enabled"] = JSON::s(enabled);
	ndata["onscreen_xpos"] = JSON::s(onscreen_xpos);
	ndata["onscreen_ypos"] = JSON::s(onscreen_ypos);
	ndata["onscreen_alt_origin_x"] = JSON::b(onscreen_alt_origin_x);
	ndata["onscreen_alt_origin_y"] = JSON::b(onscreen_alt_origin_y);
	ndata["onscreen_cliprange_x"] = JSON::b(onscreen_cliprange_x);
	ndata["onscreen_cliprange_y"] = JSON::b(onscreen_cliprange_y);
	ndata["onscreen_font"] = JSON::s(onscreen_font);
	ndata["onscreen_fg_color"] = JSON::i(onscreen_fg_color);
	ndata["onscreen_bg_color"] = JSON::i(onscreen_bg_color);
	ndata["onscreen_halo_color"] = JSON::i(onscreen_halo_color);
	return ndata;
}

void memwatch_printer::unserialize(const JSON::node& node)
{
	std::string _position = json_string_default(node, "position", "");
	if(_position == "disabled") position = PC_DISABLED;
	else if(_position == "memorywatch") position = PC_MEMORYWATCH;
	else if(_position == "onscreen") position = PC_ONSCREEN;
	else position = PC_MEMORYWATCH;
	cond_enable = json_boolean_default(node, "cond_enable", false);
	enabled = json_string_default(node, "enabled", "");
	onscreen_xpos = json_string_default(node, "onscreen_xpos", "");
	onscreen_ypos = json_string_default(node, "onscreen_ypos", "");
	onscreen_alt_origin_x = json_boolean_default(node, "onscreen_alt_origin_x", false);
	onscreen_alt_origin_y = json_boolean_default(node, "onscreen_alt_origin_y", false);
	onscreen_cliprange_x = json_boolean_default(node, "onscreen_cliprange_x", false);
	onscreen_cliprange_y = json_boolean_default(node, "onscreen_cliprange_y", false);
	onscreen_font = json_string_default(node, "onscreen_font", "");
	onscreen_fg_color = json_signed_default(node, "onscreen_fg_color", false);
	onscreen_bg_color = json_signed_default(node, "onscreen_bg_color", false);
	onscreen_halo_color = json_signed_default(node, "onscreen_halo_color", false);
}

GC::pointer<memorywatch::item_printer> memwatch_printer::get_printer_obj(
	std::function<GC::pointer<mathexpr::mathexpr>(const std::string& n)> vars)
{
	GC::pointer<memorywatch::item_printer> ptr;
	memorywatch::output_list* l;
	memorywatch::output_fb* f;

	std::string _enabled = (enabled != "") ? enabled : "true";

	switch(position) {
	case PC_DISABLED:
		ptr = GC::pointer<memorywatch::item_printer>(new memorywatch::output_null);
		break;
	case PC_MEMORYWATCH:
		ptr = GC::pointer<memorywatch::item_printer>(new memorywatch::output_list);
		l = dynamic_cast<memorywatch::output_list*>(ptr.as_pointer());
		l->cond_enable = cond_enable;
		try {
			if(l->cond_enable)
				l->enabled = mathexpr::mathexpr::parse(*mathexpr::expression_value(), _enabled, vars);
			else
				l->enabled = mathexpr::mathexpr::parse(*mathexpr::expression_value(), "true", vars);
		} catch(std::exception& e) {
			(stringfmt() << "Error while parsing conditional: " << e.what()).throwex();
		}
		l->set_output(dummy_target_fn);
		break;
	case PC_ONSCREEN:
		ptr = GC::pointer<memorywatch::item_printer>(new memorywatch::output_fb);
		f = dynamic_cast<memorywatch::output_fb*>(ptr.as_pointer());
		f->font = NULL;
		f->set_dtor_cb([](memorywatch::output_fb& obj) { put_font(obj.font); });
		f->cond_enable = cond_enable;
		std::string while_parsing = "(unknown)";
		try {
			while_parsing = "conditional";
			if(f->cond_enable)
				f->enabled = mathexpr::mathexpr::parse(*mathexpr::expression_value(), _enabled, vars);
			else
				f->enabled = mathexpr::mathexpr::parse(*mathexpr::expression_value(), "true", vars);
			while_parsing = "X position";
			f->pos_x = mathexpr::mathexpr::parse(*mathexpr::expression_value(), onscreen_xpos, vars);
			while_parsing = "Y position";
			f->pos_y = mathexpr::mathexpr::parse(*mathexpr::expression_value(), onscreen_ypos, vars);
		} catch(std::exception& e) {
			(stringfmt() << "Error while parsing " << while_parsing << ": " << e.what()).throwex();
		}
		f->alt_origin_x = onscreen_alt_origin_x;
		f->alt_origin_y = onscreen_alt_origin_y;
		f->cliprange_x = onscreen_cliprange_x;
		f->cliprange_y = onscreen_cliprange_y;
		f->fg = onscreen_fg_color;
		f->bg = onscreen_bg_color;
		f->halo = onscreen_halo_color;
		try {
			f->font = get_font(onscreen_font);
		} catch(std::exception& e) {
			messages << "Bad font '" << onscreen_font << "': " << e.what() << std::endl;
			f->font = &get_builtin_font2();
		}
		break;
	}
	return ptr;
}

memwatch_item::memwatch_item()
{
	bytes = 0;
	signed_flag = false;
	float_flag = false;
	endianess = 0;
	scale_div = 1;
	addr_base = 0;
	addr_size = 0;
}

JSON::node memwatch_item::serialize()
{
	JSON::node ndata(JSON::object);
	ndata["printer"] = printer.serialize();
	ndata["expr"] = JSON::s(expr);
	ndata["format"] = JSON::s(format);
	ndata["bytes"] = JSON::u(bytes);
	ndata["signed"] = JSON::b(signed_flag);
	ndata["float"] = JSON::b(float_flag);
	ndata["endianess"] = JSON::i(endianess);
	ndata["scale_div"] = JSON::u(scale_div);
	ndata["addr_base"] = JSON::u(addr_base);
	ndata["addr_size"] = JSON::u(addr_size);
	return ndata;
}

void memwatch_item::unserialize(const JSON::node& node)
{
	if(node.type_of("printer") == JSON::object)
		printer.unserialize(node["printer"]);
	else
		printer = memwatch_printer();
	expr = json_string_default(node, "expr", "0");
	format = json_string_default(node, "format", "");
	bytes = json_unsigned_default(node, "bytes", 0);
	signed_flag = json_boolean_default(node, "signed", false);
	float_flag = json_boolean_default(node, "float", false);
	endianess = json_signed_default(node, "endianess", false);
	scale_div = json_unsigned_default(node, "scale_div", 1);
	addr_base = json_unsigned_default(node, "addr_base", 0);
	addr_size = json_unsigned_default(node, "addr_size", 0);
}

mathexpr::operinfo* memwatch_item::get_memread_oper(memory_space& memory, loaded_rom& rom)
{
	if(addr_base == 0xFFFFFFFFFFFFFFFFULL && addr_size == 0) {
		//Hack: Registers.
		regread_oper* o = new regread_oper;
		o->rom = &rom;
		o->signed_flag = signed_flag;
		return o;
	}
	if(!bytes)
		return NULL;
	memorywatch::memread_oper* o = new memorywatch::memread_oper;
	o->bytes = bytes;
	o->signed_flag = signed_flag;
	o->float_flag = float_flag;
	o->endianess = endianess;
	o->scale_div = scale_div;
	o->addr_base = addr_base;
	o->addr_size = addr_size;
	o->mspace = &memory;
	return o;
}

void memwatch_item::compatiblity_unserialize(memory_space& memory, const std::string& item)
{
	regex_results r;
	if(!(r = regex("C0x([0-9A-Fa-f]{1,16})z([bBwWoOdDqQfF])(H([0-9A-Ga-g]))?", item)))
		throw std::runtime_error("Unknown compatiblity memory watch");
	std::string _addr = r[1];
	std::string _type = r[2];
	std::string _hext = r[4];
	uint64_t addr = strtoull(_addr.c_str(), NULL, 16);
	char type = _type[0];
	char hext = (_hext != "") ? _hext[0] : 0;
	switch(type) {
	case 'b': bytes = 1; signed_flag = true;  float_flag = false; break;
	case 'B': bytes = 1; signed_flag = false; float_flag = false; break;
	case 'w': bytes = 2; signed_flag = true;  float_flag = false; break;
	case 'W': bytes = 2; signed_flag = false; float_flag = false; break;
	case 'o': bytes = 3; signed_flag = true;  float_flag = false; break;
	case 'O': bytes = 3; signed_flag = false; float_flag = false; break;
	case 'd': bytes = 4; signed_flag = true;  float_flag = false; break;
	case 'D': bytes = 4; signed_flag = false; float_flag = false; break;
	case 'q': bytes = 8; signed_flag = true;  float_flag = false; break;
	case 'Q': bytes = 8; signed_flag = false; float_flag = false; break;
	case 'f': bytes = 4; signed_flag = true;  float_flag = true;  break;
	case 'F': bytes = 8; signed_flag = true;  float_flag = true;  break;
	default:  bytes = 0;                                          break;
	}
	auto mdata = memory.lookup(addr);
	if(mdata.first) {
		addr = mdata.second;
		addr_base = mdata.first->base;
		addr_size = mdata.first->size;
		endianess = mdata.first->endian;
	} else {
		addr_base = 0;
		addr_size = 0;
		endianess = -1;
	}
	if(hext) {
		unsigned width;
		if(hext >= '0' && hext <= '9')
			width = hext - '0';
		else
			width = (hext & 0x1F) + 9;
		format = (stringfmt() << "%0" << width << "x").str();
	} else
		format = "";
	expr = (stringfmt() << "0x" << std::hex << addr).str();
	scale_div = 1;
	printer.position = memwatch_printer::PC_MEMORYWATCH;
	printer.cond_enable = false;
	printer.enabled = "true";
	printer.onscreen_xpos = "0";
	printer.onscreen_ypos = "0";
	printer.onscreen_alt_origin_x = false;
	printer.onscreen_alt_origin_y = false;
	printer.onscreen_cliprange_x = false;
	printer.onscreen_cliprange_y = false;
	printer.onscreen_font = "";
	printer.onscreen_fg_color = 0xFFFFFF;
	printer.onscreen_bg_color = -1;
	printer.onscreen_halo_color = 0;
}

memwatch_set::memwatch_set(memory_space& _memory, project_state& _project, emu_framebuffer& _fbuf,
	loaded_rom& _rom)
	: memory(_memory), project(_project), fbuf(_fbuf), rom(_rom)
{
}

std::set<std::string> memwatch_set::enumerate()
{
	std::set<std::string> r;
	for(auto& i : items)
		r.insert(i.first);
	return r;
}

void memwatch_set::clear(const std::string& name)
{
	std::map<std::string, memwatch_item> nitems = items;
	nitems.erase(name);
	rebuild(nitems);
	std::swap(items, nitems);
	auto pr = project.get();
	if(pr) {
		pr->watches.erase(name);
		pr->flush();
	}
	fbuf.redraw_framebuffer();
}

void memwatch_set::set(const std::string& name, const std::string& item)
{
	memwatch_item _item;
	if(item != "" && item[0] != '{') {
		//Compatiblity.
		try {
			_item.compatiblity_unserialize(memory, item);
		} catch(std::exception& e) {
			messages << "Can't handle old memory watch '" << name << "'" << std::endl;
			return;
		}
	} else
		_item.unserialize(JSON::node(item));
	set(name, _item);
}

memwatch_item& memwatch_set::get(const std::string& name)
{
	if(!items.count(name))
		throw std::runtime_error("No such memory watch named '" + name + "'");
	return items.find(name)->second;
}

std::string memwatch_set::get_string(const std::string& name, JSON::printer* printer)
{
	auto& x = get(name);
	auto y = x.serialize();
	auto z = y.serialize(printer);
	return z;
}

void memwatch_set::watch(struct framebuffer::queue& rq)
{
	//Set framebuffer for all FB watches.
	watch_set.foreach([&rq](memorywatch::item& i) {
		memorywatch::output_fb* fb = dynamic_cast<memorywatch::output_fb*>(i.printer.as_pointer());
		if(fb)
			fb->set_rqueue(rq);
	});
	watch_set.refresh();
	erase_unused_watches();
}

bool memwatch_set::rename(const std::string& oldname, const std::string& newname)
{
	std::map<std::string, memwatch_item> nitems = items;
	if(nitems.count(newname))
		return false;
	if(!nitems.count(oldname))
		return false;
	nitems.insert(std::make_pair(newname, nitems.find(oldname)->second));
	nitems.erase(oldname);
	rebuild(nitems);
	std::swap(items, nitems);
	auto pr = project.get();
	if(pr) {
		pr->watches.erase(oldname);
		pr->watches[newname] = get_string(newname);
		pr->flush();
	}
	fbuf.redraw_framebuffer();
	return true;
}

void memwatch_set::set(const std::string& name, memwatch_item& item)
{
	std::map<std::string, memwatch_item> nitems = items;
	nitems.erase(name); //Insert does not insert if already existing.
	nitems.insert(std::make_pair(name, item));
	rebuild(nitems);
	std::swap(items, nitems);
	auto pr = project.get();
	if(pr) {
		pr->watches[name] = get_string(name);
		pr->flush();
	}
	fbuf.redraw_framebuffer();
}

std::string memwatch_set::get_value(const std::string& name)
{
	return watch_set.get(name).get_value();
}

void memwatch_set::set_multi(std::list<std::pair<std::string, memwatch_item>>& list)
{
	std::map<std::string, memwatch_item> nitems = items;
	for(auto& i : list)
		nitems.insert(i);
	rebuild(nitems);
	std::swap(items, nitems);
	auto pr = project.get();
	if(pr) {
		for(auto& i : list)
			pr->watches[i.first] = get_string(i.first);
		pr->flush();
	}
	fbuf.redraw_framebuffer();
}

void memwatch_set::set_multi(std::list<std::pair<std::string, std::string>>& list)
{
	std::list<std::pair<std::string, memwatch_item>> _list;
	for(auto& i: list) {
		memwatch_item it;
		it.unserialize(JSON::node(i.second));
		_list.push_back(std::make_pair(i.first, it));
	}
	set_multi(_list);
}

void memwatch_set::clear_multi(const std::set<std::string>& names)
{
	std::map<std::string, memwatch_item> nitems = items;
	for(auto& i : names)
		nitems.erase(i);
	rebuild(nitems);
	std::swap(items, nitems);
	auto pr = project.get();
	if(pr) {
		for(auto& i : names)
			pr->watches.erase(i);
		pr->flush();
	}
	fbuf.redraw_framebuffer();
}

void memwatch_set::rebuild(std::map<std::string, memwatch_item>& nitems)
{
	{
		memorywatch::set new_set;
		std::map<std::string, GC::pointer<mathexpr::mathexpr>> vars;
		auto vars_fn = [&vars](const std::string& n) -> GC::pointer<mathexpr::mathexpr> {
			if(!vars.count(n))
				vars[n] = GC::pointer<mathexpr::mathexpr>(GC::obj_tag(),
					mathexpr::expression_value());
			return vars[n];
		};
		for(auto& i : nitems) {
			mathexpr::operinfo* memread_oper = i.second.get_memread_oper(memory, rom);
			try {
				GC::pointer<mathexpr::mathexpr> rt_expr;
				GC::pointer<memorywatch::item_printer> rt_printer;
				std::vector<GC::pointer<mathexpr::mathexpr>> v;
				try {
					rt_expr = mathexpr::mathexpr::parse(*mathexpr::expression_value(),
						i.second.expr, vars_fn);
				} catch(std::exception& e) {
					(stringfmt() << "Error while parsing address/expression: "
						<< e.what()).throwex();
				}
				v.push_back(rt_expr);
				if(memread_oper) {
					rt_expr = GC::pointer<mathexpr::mathexpr>(GC::obj_tag(),
						mathexpr::expression_value(), memread_oper, v, true);
					memread_oper = NULL;
				}
				rt_printer = i.second.printer.get_printer_obj(vars_fn);

				//Set final callback for list objects (since it wasn't known on creation).
				auto list_obj = dynamic_cast<memorywatch::output_list*>(rt_printer.as_pointer());
				if(list_obj)
					list_obj->set_output([this](const std::string& n, const std::string& v) {
						this->watch_output(n, v);
					});

				memorywatch::item it(*mathexpr::expression_value());
				*vars_fn(i.first) = *rt_expr;
				it.expr = vars_fn(i.first);
				it.printer = rt_printer;
				it.format = i.second.format;
				new_set.create(i.first, it);
			} catch(...) {
				delete memread_oper;
				throw;
			}
		}
		watch_set.swap(new_set);
	}
	GC::item::do_gc();
}

void memwatch_set::watch_output(const std::string& name, const std::string& value)
{
	used_memorywatches[name] = true;
	window_vars[name] = utf8::to32(value);
}

void memwatch_set::erase_unused_watches()
{
	for(auto& i : used_memorywatches) {
		if(!i.second)
			window_vars.erase(i.first);
		i.second = false;
	}
}
