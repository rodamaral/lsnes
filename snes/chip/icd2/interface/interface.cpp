#ifdef ICD2_CPP

//called on rendered lines 0-143 (not on Vblank lines 144-153)
void ICD2::lcdScanline() {
  if((gb_if->get_ly() & 7) == 0) {
    lcd.row = (lcd.row + 1) & 3;
  }

  unsigned offset = (lcd.row * 160 * 8) + ((gb_if->get_ly() & 7) * 160);
  memcpy(lcd.buffer + offset, gb_if->get_cur_scanline(), 160 * sizeof(uint16));
}

void ICD2::joypWrite(bool p15, bool p14) {
  //joypad handling
  if(p15 == 1 && p14 == 1) {
    if(joyp15lock == 0 && joyp14lock == 0) {
      joyp15lock = 1;
      joyp14lock = 1;
      joyp_id = (joyp_id + 1) & 3;
    }
  }

  if(p15 == 0 && p14 == 1) joyp15lock = 0;
  if(p15 == 1 && p14 == 0) joyp14lock = 0;

  //packet handling
  if(p15 == 0 && p14 == 0) {  //pulse
    pulselock = false;
    packetoffset = 0;
    bitoffset = 0;
    strobelock = true;
    packetlock = false;
    return;
  }

  if(pulselock) return;

  if(p15 == 1 && p14 == 1) {
    strobelock = false;
    return;
  }

  if(strobelock) {
    if(p15 == 1 || p14 == 1) {  //malformed packet
      packetlock = false;
      pulselock = true;
      bitoffset = 0;
      packetoffset = 0;
    } else {
      return;
    }
  }

  //p15:1, p14:0 = 0
  //p15:0, p14:1 = 1
  bool bit = (p15 == 0);
  strobelock = true;

  if(packetlock) {
    if(p15 == 1 && p14 == 0) {
      if(packetsize < 64) packet[packetsize++] = joyp_packet;
      packetlock = false;
      pulselock = true;
    }
    return;
  }

  bitdata = (bit << 7) | (bitdata >> 1);
  if(++bitoffset < 8) return;

  bitoffset = 0;
  joyp_packet[packetoffset] = bitdata;
  if(++packetoffset < 16) return;
  packetlock = true;
}

void ICD2::videoRefresh(const uint16_t *data) {
}

void ICD2::audioSample(int16_t center, int16_t left, int16_t right) {
  audio.coprocessor_sample(left, right);
}

bool ICD2::inputPoll(unsigned id) {
  gb_if->set_mlt_req(joyp_id & mlt_req);

  unsigned data = 0x00;
  switch(joyp_id & mlt_req) {
    case 0: data = ~r6004; break;
    case 1: data = ~r6005; break;
    case 2: data = ~r6006; break;
    case 3: data = ~r6007; break;
  }

  switch((gameboy_input)id) {
    case gameboy_input::Start:  return data & 0x80;
    case gameboy_input::Select: return data & 0x40;
    case gameboy_input::B:      return data & 0x20;
    case gameboy_input::A:      return data & 0x10;
    case gameboy_input::Down:   return data & 0x08;
    case gameboy_input::Up:     return data & 0x04;
    case gameboy_input::Left:   return data & 0x02;
    case gameboy_input::Right:  return data & 0x01;
  }

  return 0;
}

#endif
