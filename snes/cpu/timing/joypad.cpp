#ifdef CPU_CPP

//called every 256 clocks; see CPU::add_clocks()
void CPU::step_auto_joypad_poll() {
  if(vcounter() >= (ppu.overscan() == false ? 225 : 240)) {
    //cache enable state at first iteration
    if(status.auto_joypad_counter == 0) status.auto_joypad_latch = status.auto_joypad_poll;
    status.auto_joypad_active = status.auto_joypad_counter <= 15;
    if(status.auto_joypad_active && status.auto_joypad_latch) {
      if(status.auto_joypad_counter == 0) {
        if(dma_trace_fn) dma_trace_fn("-- Start automatic polling --");
        interface->notifyLatched();
        input.port1->latch(1);
        input.port2->latch(1);
        input.port1->latch(0);
        input.port2->latch(0);
      }

      uint2 port0 = input.port1->data();
      uint2 port1 = input.port2->data();

      status.joy1 = (status.joy1 << 1) | (bool)(port0 & 1);
      status.joy2 = (status.joy2 << 1) | (bool)(port1 & 1);
      status.joy3 = (status.joy3 << 1) | (bool)(port0 & 2);
      status.joy4 = (status.joy4 << 1) | (bool)(port1 & 2);
      if(status.auto_joypad_counter == 15) {
        char buf[512];
        sprintf(buf, "-- End automatic polling [%04x %04x %04x %04x] --",
          status.joy1, status.joy2, status.joy3, status.joy4);
        if(dma_trace_fn) dma_trace_fn(buf);
      }
    }

    status.auto_joypad_counter++;
  }
}

//called every 128 clocks; see CPU::add_clocks()
void CPU::step_auto_joypad_poll_NEW(bool polarity) {
  if(status.auto_joypad_counter > 0 && status.auto_joypad_counter <= 34) {
    if(!status.auto_joypad_latch) {
      //FIXME: Is this right, busy flag goes on even if not enabled???
      if(status.auto_joypad_counter == 1)
        status.auto_joypad_active = true;
      if(status.auto_joypad_counter == 34)
        status.auto_joypad_active = false;
    } else {
      if(status.auto_joypad_counter == 1) {
        if(dma_trace_fn) dma_trace_fn("-- Start automatic polling --");
        status.auto_joypad_active = true;
        interface->notifyLatched();
        input.port1->latch(1);
        input.port2->latch(1);
      }
      if(status.auto_joypad_counter == 3) {
        input.port1->latch(0);
        input.port2->latch(0);
      }
      if((status.auto_joypad_counter & 1) != 0 &&  status.auto_joypad_counter != 1) {
        uint2 port0 = input.port1->data();
        uint2 port1 = input.port2->data();

        status.joy1 = (status.joy1 << 1) | (bool)(port0 & 1);
        status.joy2 = (status.joy2 << 1) | (bool)(port1 & 1);
        status.joy3 = (status.joy3 << 1) | (bool)(port0 & 2);
        status.joy4 = (status.joy4 << 1) | (bool)(port1 & 2);
      }
      if(status.auto_joypad_counter == 34) {
        status.auto_joypad_active = false;
        char buf[512];
        sprintf(buf, "-- End automatic polling [%04x %04x %04x %04x] --",
          status.joy1, status.joy2, status.joy3, status.joy4);
        if(dma_trace_fn) dma_trace_fn(buf);
      }
    }
    status.auto_joypad_counter++;
  }
  if(vcounter() >= (ppu.overscan() == false ? 225 : 240) && status.auto_joypad_counter == 0 && !polarity) {
    status.auto_joypad_latch = status.auto_joypad_poll;
    status.auto_joypad_counter = 1;
  }
}


#endif
