#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>
#include "moviefile.hpp"
#include "zip.hpp"
#include "misc.hpp"
#include "rrdata.hpp"
#include <sstream>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

void strip_CR(std::string& x) throw(std::bad_alloc)
{
	if(x.length() > 0 && x[x.length() - 1] == '\r') {
		if(x.length() > 1)
			x = x.substr(0, x.length() - 1);
		else
			x = "";
	}
}

void read_linefile(zip_reader& r, const std::string& member, std::string& out, bool conditional = false)
	throw(std::bad_alloc, std::runtime_error)
{
	if(conditional && !r.has_member(member))
		return;
	std::istream& m = r[member];
	try {
		std::getline(m, out);
		strip_CR(out);
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
}

void write_linefile(zip_writer& w, const std::string& member, const std::string& value, bool conditional = false)
	throw(std::bad_alloc, std::runtime_error)
{
	if(conditional && value == "")
		return;
	std::ostream& m = w.create_file(member);
	try {
		m << value << std::endl;
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void write_raw_file(zip_writer& w, const std::string& member, std::vector<char>& content) throw(std::bad_alloc,
	std::runtime_error)
{
	std::ostream& m = w.create_file(member);
	try {
		m.write(&content[0], content.size());
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

std::vector<char> read_raw_file(zip_reader& r, const std::string& member) throw(std::bad_alloc, std::runtime_error)
{
	std::vector<char> out;
	std::istream& m = r[member];
	try {
		boost::iostreams::back_insert_device<std::vector<char>> rd(out);
		boost::iostreams::copy(m, rd);
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
	return out;
}

void read_authors_file(zip_reader& r, std::vector<std::pair<std::string, std::string>>& authors) throw(std::bad_alloc,
	std::runtime_error)
{
	std::istream& m = r["authors"];
	try {
		std::string x;
		while(std::getline(m, x)) {
			strip_CR(x);
			fieldsplitter fields(x);
			std::string y = static_cast<std::string>(fields);
			std::string z = static_cast<std::string>(fields);
			authors.push_back(std::make_pair(y, z));
		}
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
}

std::string read_rrdata(zip_reader& r, const std::string& projectid) throw(std::bad_alloc, std::runtime_error)
{
	std::istream& m = r["rrdata"];
	uint64_t count;
	try {
		rrdata::read_base(projectid);
		count = rrdata::read(m);
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
	std::ostringstream x;
	x << count;
	return x.str();
}

void write_rrdata(zip_writer& w) throw(std::bad_alloc, std::runtime_error)
{
	std::ostream& m = w.create_file("rrdata");
	uint64_t count;
	try {
		count = rrdata::write(m);
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
	std::ostream& m2 = w.create_file("rerecords");
	try {
		m2 << count << std::endl;
		if(!m2)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void write_authors_file(zip_writer& w, std::vector<std::pair<std::string, std::string>>& authors)
	throw(std::bad_alloc, std::runtime_error)
{
	std::ostream& m = w.create_file("authors");
	try {
		for(auto i = authors.begin(); i != authors.end(); ++i)
			if(i->second == "")
				m << i->first << std::endl;
			else
				m << i->first << "|" << i->second << std::endl;
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void write_input(zip_writer& w, std::vector<controls_t>& input, porttype_t port1, porttype_t port2)
	throw(std::bad_alloc, std::runtime_error)
{
	std::vector<cencode::fn_t> encoders;
	encoders.push_back(port_types[port1].encoder);
	encoders.push_back(port_types[port2].encoder);
	std::ostream& m = w.create_file("input");
	try {
		for(auto i = input.begin(); i != input.end(); ++i)
			m << i->tostring(encoders) << std::endl;
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void read_input(zip_reader& r, std::vector<controls_t>& input, porttype_t port1, porttype_t port2, unsigned version)
	throw(std::bad_alloc, std::runtime_error)
{
	std::vector<cdecode::fn_t> decoders;
	decoders.push_back(port_types[port1].decoder);
	decoders.push_back(port_types[port2].decoder);
	std::istream& m = r["input"];
	try {
		std::string x;
		while(std::getline(m, x)) {
			strip_CR(x);
			if(x != "") {
				input.push_back(controls_t(x, decoders, version));
			}
		}
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}

}

porttype_t parse_controller_type(const std::string& type, bool port) throw(std::bad_alloc, std::runtime_error)
{
	porttype_t port1 = PT_INVALID;
	for(unsigned i = 0; i <= PT_LAST_CTYPE; i++)
		if(type == port_types[i].name && (port || port_types[i].valid_port1))
			port1 = static_cast<porttype_t>(i);
	if(port1 == PT_INVALID)
		throw std::runtime_error(std::string("Illegal port") + (port ? "2" : "1") + " device '" + type + "'");
	return port1;
}


moviefile::moviefile() throw(std::bad_alloc)
{
	gametype = GT_INVALID;
	port1 = PT_GAMEPAD;
	port2 = PT_NONE;
	coreversion = bsnes_core_version;
	projectid = get_random_hexstring(40);
	rerecords = "0";
	is_savestate = false;
}

moviefile::moviefile(const std::string& movie) throw(std::bad_alloc, std::runtime_error)
{
	is_savestate = false;
	std::string tmp;
	zip_reader r(movie);
	read_linefile(r, "systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	read_linefile(r, "controlsversion", tmp);
	if(tmp != "0")
		throw std::runtime_error("Can't decode movie data");
	read_linefile(r, "gametype", tmp);
	try {
		gametype = gtype::togametype(tmp);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Illegal game type '" + tmp + "'");
	}
	tmp = port_types[PT_GAMEPAD].name;
	read_linefile(r, "port1", tmp, true);
	port1 = port_type::lookup(tmp, false).ptype;
	tmp = port_types[PT_NONE].name;
	read_linefile(r, "port2", tmp, true);
	port2 = port_type::lookup(tmp, true).ptype;
	read_linefile(r, "gamename", gamename, true);
	read_linefile(r, "projectid", projectid);
	rerecords = read_rrdata(r, projectid);
	read_linefile(r, "coreversion", coreversion);
	read_linefile(r, "rom.sha256", rom_sha256, true);
	read_linefile(r, "romxml.sha256", romxml_sha256, true);
	read_linefile(r, "slota.sha256", slota_sha256, true);
	read_linefile(r, "slotaxml.sha256", slotaxml_sha256, true);
	read_linefile(r, "slotb.sha256", slotb_sha256, true);
	read_linefile(r, "slotbxml.sha256", slotbxml_sha256, true);
	if(r.has_member("savestate")) {
		is_savestate = true;
		movie_state = read_raw_file(r, "moviestate");
		if(r.has_member("hostmemory"))
			host_memory = read_raw_file(r, "hostmemory");
		savestate = read_raw_file(r, "savestate");
		for(auto name = r.begin(); name != r.end(); ++name)
			if((*name).length() >= 5 && (*name).substr(0, 5) == "sram.")
				sram[(*name).substr(5)] = read_raw_file(r, *name);
		screenshot = read_raw_file(r, "screenshot");
	}
	std::string name = r.find_first();
	for(auto name = r.begin(); name != r.end(); ++name)
		if((*name).length() >= 10 && (*name).substr(0, 10) == "moviesram.")
			sram[(*name).substr(10)] = read_raw_file(r, *name);
	read_authors_file(r, authors);
	read_input(r, input, port1, port2, 0);
}

void moviefile::save(const std::string& movie, unsigned compression) throw(std::bad_alloc, std::runtime_error)
{
	zip_writer w(movie, compression);
	write_linefile(w, "gametype", gtype::tostring(gametype));
	if(port1 != PT_GAMEPAD)
		write_linefile(w, "port1", port_types[port1].name);
	if(port2 != PT_NONE)
		write_linefile(w, "port2", port_types[port2].name);
	write_linefile(w, "gamename", gamename, true);
	write_linefile(w, "systemid", "lsnes-rr1");
	write_linefile(w, "controlsversion", "0");
	coreversion = bsnes_core_version;
	write_linefile(w, "coreversion", coreversion);
	write_linefile(w, "projectid", projectid);
	write_rrdata(w);
	write_linefile(w, "rom.sha256", rom_sha256, true);
	write_linefile(w, "romxml.sha256", romxml_sha256, true);
	write_linefile(w, "slota.sha256", slota_sha256, true);
	write_linefile(w, "slotaxml.sha256", slotaxml_sha256, true);
	write_linefile(w, "slotb.sha256", slotb_sha256, true);
	write_linefile(w, "slotbxml.sha256", slotbxml_sha256, true);
	for(auto i = movie_sram.begin(); i != movie_sram.end(); ++i)
		write_raw_file(w, "moviesram." + i->first, i->second);
	if(is_savestate) {
		write_raw_file(w, "moviestate", movie_state);
		write_raw_file(w, "hostmemory", host_memory);
		write_raw_file(w, "savestate", savestate);
		write_raw_file(w, "screenshot", screenshot);
		for(auto i = sram.begin(); i != sram.end(); ++i)
			write_raw_file(w, "sram." + i->first, i->second);
	}
	write_authors_file(w, authors);
	write_input(w, input, port1, port2);
	
	w.commit();
}

uint64_t moviefile::get_frame_count() throw()
{
	uint64_t frames = 0;
	for(size_t i = 0; i < input.size(); i++) {
		if(input[i](CONTROL_FRAME_SYNC))
			frames++;
	}
	return frames;
}

namespace
{
	const int BLOCK_SECONDS = 0;
	const int BLOCK_FRAMES = 1;
	const int STEP_W = 2;
	const int STEP_N = 3;

	uint64_t magic[2][4] = {
		{178683, 10738636, 16639264, 596096},
		{6448, 322445, 19997208, 266440}
	};
}

uint64_t moviefile::get_movie_length() throw()
{
	uint64_t frames = get_frame_count();
	uint64_t* _magic = magic[(gametype == GT_SNES_PAL || gametype == GT_SGB_PAL) ? 1 : 0];
	uint64_t t = _magic[BLOCK_SECONDS] * 1000000000ULL * (frames / _magic[BLOCK_FRAMES]);
	frames %= _magic[BLOCK_FRAMES];
	t += frames * _magic[STEP_W] + (frames * _magic[STEP_N] / _magic[BLOCK_FRAMES]);
	return t;
}

gametype_t gametype_compose(rom_type type, rom_region region)
{
	switch(type) {
	case ROMTYPE_SNES:
		return (region == REGION_PAL) ? GT_SNES_PAL : GT_SNES_NTSC;
	case ROMTYPE_BSX:
		return GT_BSX;
	case ROMTYPE_BSXSLOTTED:
		return GT_BSX_SLOTTED;
	case ROMTYPE_SUFAMITURBO:
		return GT_SUFAMITURBO;
	case ROMTYPE_SGB:
		return (region == REGION_PAL) ? GT_SGB_PAL : GT_SGB_NTSC;
	default:
		return GT_INVALID;
	}
}

rom_region gametype_region(gametype_t type)
{
	switch(type) {
	case GT_SGB_PAL:
	case GT_SNES_PAL:
		return REGION_PAL;
	default:
		return REGION_NTSC;
	}
}

rom_type gametype_romtype(gametype_t type)
{
	switch(type) {
	case GT_SNES_NTSC:
	case GT_SNES_PAL:
		return ROMTYPE_SNES;
	case GT_BSX:
		return ROMTYPE_BSX;
	case GT_BSX_SLOTTED:
		return ROMTYPE_BSXSLOTTED;
	case GT_SUFAMITURBO:
		return ROMTYPE_SUFAMITURBO;
	case GT_SGB_PAL:
	case GT_SGB_NTSC:
		return ROMTYPE_SGB;
	default:
		return ROMTYPE_NONE;
	};
}

