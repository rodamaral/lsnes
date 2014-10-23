bool gb_override_flag = false;

const struct gameboy_interface default_gb_if = {
	[](GameBoy::Interface* interface) -> void {
		GameBoy::interface = interface;
		GameBoy::system.init();
		GameBoy::system.power();
	}, []() -> unsigned {
		return GameBoy::lcd.status.ly;
	}, []() -> uint16_t* {
		return GameBoy::lcd.screen + GameBoy::lcd.status.ly * 160;
	}, [](uint8_t req) -> void {
		GameBoy::cpu.status.mlt_req = req;
	}, [](serializer& s) -> void {
		GameBoy::system.serialize_all(s);
	}, []() -> void {
		GameBoy::system.runtosave();
	}, [](Coprocessor* proc) -> void {
		GameBoy::system.run();
		proc->step(GameBoy::system.clocks_executed);
		GameBoy::system.clocks_executed = 0;
	}, []() -> nall::string {
		return GameBoy::cartridge.sha256();
	}, true
};

const struct gameboy_interface* gb_if = &default_gb_if;
const struct gameboy_interface* gb_load_if = NULL;

nall::string gameboy_cartridge_sha256()
{
	return (gb_load_if ? gb_load_if : &default_gb_if)->cartridge_sha256();
}
