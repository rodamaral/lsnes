#define BSNES_SUPPORTS_MOUSE_SPEED_FIX

struct Configuration {
  Input::Device controller_port1;
  Input::Device controller_port2;
  System::ExpansionPortDevice expansion_port;
  System::Region region;
  bool random;
  bool mouse_speed_fix;

  struct CPU {
    unsigned version;
    unsigned ntsc_frequency;
    unsigned pal_frequency;
    unsigned wram_init_value;
    bool alt_poll_timings;
  } cpu;

  struct SMP {
    unsigned ntsc_frequency;
    unsigned pal_frequency;
  } smp;

  struct PPU1 {
    unsigned version;
  } ppu1;

  struct PPU2 {
    unsigned version;
  } ppu2;

  struct SuperFX {
    unsigned speed;
  } superfx;

  Configuration();
};

extern Configuration config;
