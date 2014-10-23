#pragma once

namespace SNES {

struct gameboy_interface
{
	void (*do_init)(GameBoy::Interface* interface);
	unsigned (*get_ly)();
	uint16_t* (*get_cur_scanline)();
	void (*set_mlt_req)(uint8_t req);
	void (*serialize)(serializer& s);
	void (*runtosave)();
	void (*run_timeslice)(Coprocessor* proc);
	nall::string (*cartridge_sha256)();
	bool default_rates;
};

extern bool gb_override_flag;
extern const struct gameboy_interface* gb_if;
extern const struct gameboy_interface* gb_load_if;
extern const struct gameboy_interface default_gb_if;

enum class gameboy_input : unsigned { Up, Down, Left, Right, B, A, Select, Start };

}