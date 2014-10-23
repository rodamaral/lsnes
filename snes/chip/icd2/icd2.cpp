#include <snes/snes.hpp>

#include "gameboy.hpp"

#define ICD2_CPP
namespace SNES {

#include "gameboy.cpp"
#include "interface/interface.cpp"
#include "mmio/mmio.cpp"
#include "serialization.cpp"
ICD2 icd2;

void ICD2::Enter() { icd2.enter(); }

void ICD2::enter() {
  while(true) {
    if(scheduler.sync == Scheduler::SynchronizeMode::All) {
      gb_if->runtosave();
      scheduler.exit(Scheduler::ExitReason::SynchronizeEvent);
    }

    if(r6003 & 0x80) {
      gb_if->run_timeslice(this);
    } else {  //DMG halted
      audio.coprocessor_sample(0x0000, 0x0000);
      step(fdiv);
    }
    synchronize_cpu();
  }
}

void ICD2::init() {
}

void ICD2::load() {
}

void ICD2::unload() {
}

void ICD2::power() {
  if(!gb_override_flag)
    gb_if = &default_gb_if;
  gb_override_flag = false;

  fdiv = (gb_if->default_rates ? 1 : 10);

  audio.coprocessor_enable(true);
  audio.coprocessor_frequency((gb_if->default_rates ? 4 : 2) * 1024 * 1024);
}

void ICD2::reset() {
  create(ICD2::Enter, cpu.frequency / (gb_if->default_rates ? 5 : 1));

  fdiv = (gb_if->default_rates ? 1 : 10);
  r6000_ly = 0x00;
  r6000_row = 0x00;
  r6003 = 0x00;
  r6004 = 0xff;
  r6005 = 0xff;
  r6006 = 0xff;
  r6007 = 0xff;
  for(unsigned n = 0; n < 16; n++) r7000[n] = 0x00;
  r7800 = 0x0000;
  mlt_req = 0;

  for(auto &n : lcd.buffer) n = 0;
  for(auto &n : lcd.output) n = 0;
  lcd.row = 0;

  packetsize = 0;
  joyp_id = 3;
  joyp15lock = 0;
  joyp14lock = 0;
  pulselock = true;

  gb_if->do_init(this);
}


}
