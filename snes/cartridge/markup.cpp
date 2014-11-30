#ifdef CARTRIDGE_CPP

void Cartridge::parse_markup(const char *markup) {
  mapping.reset();
  information.nss.setting.reset();

  XML::Document document(markup);
  auto &cartridge = document["cartridge"];
  region = cartridge["region"].data != "PAL" ? Region::NTSC : Region::PAL;

  parse_markup_rom(cartridge["rom"]);
  parse_markup_ram(cartridge["ram"]);
  parse_markup_nss(cartridge["nss"]);
  parse_markup_icd2(cartridge["icd2"]);
  parse_markup_sa1(cartridge["sa1"]);
  parse_markup_superfx(cartridge["superfx"]);
  parse_markup_necdsp(cartridge["necdsp"]);
  parse_markup_hitachidsp(cartridge["hitachidsp"]);
  parse_markup_bsx(cartridge["bsx"]);
  parse_markup_sufamiturbo(cartridge["sufamiturbo"]);
  parse_markup_srtc(cartridge["srtc"]);
  parse_markup_sdd1(cartridge["sdd1"]);
  parse_markup_spc7110(cartridge["spc7110"]);
  parse_markup_obc1(cartridge["obc1"]);
  parse_markup_setarisc(cartridge["setarisc"]);
  parse_markup_msu1(cartridge["msu1"]);
  parse_markup_link(cartridge["link"]);
}

//

unsigned Cartridge::parse_markup_integer(string &data) {
  if(strbegin(data, "0x")) return hex(data);
  return decimal(data);
}

void Cartridge::parse_markup_map(Mapping &m, XML::Node &map) {
  m.offset = parse_markup_integer(map["offset"].data);
  m.size = parse_markup_integer(map["size"].data);

  string data = map["mode"].data;
  if(data == "direct") m.mode = Bus::MapMode::Direct;
  if(data == "linear") m.mode = Bus::MapMode::Linear;
  if(data == "shadow") m.mode = Bus::MapMode::Shadow;

  lstring part;
  part.split(":", map["address"].data);
  if(part.size() != 2) return;

  lstring subpart;
  subpart.split("-", part[0]);
  if(subpart.size() == 1) {
    m.banklo = hex(subpart[0]);
    m.bankhi = m.banklo;
  } else if(subpart.size() == 2) {
    m.banklo = hex(subpart[0]);
    m.bankhi = hex(subpart[1]);
  }

  subpart.split("-", part[1]);
  if(subpart.size() == 1) {
    m.addrlo = hex(subpart[0]);
    m.addrhi = m.addrlo;
  } else if(subpart.size() == 2) {
    m.addrlo = hex(subpart[0]);
    m.addrhi = hex(subpart[1]);
  }
}

//

void Cartridge::parse_markup_rom(XML::Node &root) {
  if(root.exists() == false) return;
  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m(rom);
    m.clazz = MemoryClass::ROM;
    parse_markup_map(m, node);
    if(m.size == 0) m.size = rom.size();
    mapping.append(m);
  }
}

void Cartridge::parse_markup_ram(XML::Node &root) {
  if(root.exists() == false) return;
  ram_size = parse_markup_integer(root["size"].data);
  for(auto &node : root) {
    Mapping m(ram);
    m.clazz = MemoryClass::SRAM;
    parse_markup_map(m, node);
    if(m.size == 0) m.size = ram_size;
    mapping.append(m);
  }
}

void Cartridge::parse_markup_nss(XML::Node &root) {
  if(root.exists() == false) return;
  has_nss_dip = true;
  for(auto &node : root) {
    if(node.name != "setting") continue;
    unsigned number = information.nss.setting.size();
    if(number >= 16) break;  //more than 16 DIP switches is not physically possible

    information.nss.option[number].reset();
    information.nss.setting[number] = node["name"].data;
    for(auto &leaf : node) {
      if(leaf.name != "option") continue;
      string name = leaf["name"].data;
      unsigned value = parse_markup_integer(leaf["value"].data);
      information.nss.option[number].append({ hex<4>(value), ":", name });
    }
  }
}

void Cartridge::parse_markup_icd2(XML::Node &root) {
  if(root.exists() == false) return;
  if(mode != Mode::SuperGameBoy) return;

  icd2.revision = max(1, parse_markup_integer(root["revision"].data));

  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m({ &ICD2::read, &icd2 }, { &ICD2::write, &icd2 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_superfx(XML::Node &root) {
  if(root.exists() == false) return;
  has_superfx = true;

  for(auto &node : root) {
    if(node.name == "rom") {
      for(auto &leaf : node) {
        if(leaf.name != "map") continue;
        Mapping m(superfx.rom);
        //m.clazz = MemoryClass::SUPERFXROM;  -- Aliases ROM.
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
    if(node.name == "ram") {
      for(auto &leaf : node) {
        if(leaf.name == "size") {
          ram_size = parse_markup_integer(leaf.data);
          continue;
        }
        if(leaf.name != "map") continue;
        Mapping m(superfx.ram);
        //m.clazz = MemoryClass::SUPERFXRAM;  -- Aliases SRAM.
        parse_markup_map(m, leaf);
        if(m.size == 0) m.size = ram_size;
        mapping.append(m);
      }
    }
    if(node.name == "mmio") {
      for(auto &leaf : node) {
        if(leaf.name != "map") continue;
        Mapping m({ &SuperFX::mmio_read, &superfx }, { &SuperFX::mmio_write, &superfx });
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
  }
}

void Cartridge::parse_markup_sa1(XML::Node &root) {
  if(root.exists() == false) return;
  has_sa1 = true;

  auto &mcurom = root["mcu"]["rom"];
  auto &mcuram = root["mcu"]["ram"];
  auto &iram = root["iram"];
  auto &bwram = root["bwram"];
  auto &mmio = root["mmio"];

  for(auto &node : mcurom) {
    if(node.name != "map") continue;
    Mapping m({ &SA1::mmc_read, &sa1 }, { &SA1::mmc_write, &sa1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : mcuram) {
    if(node.name != "map") continue;
    Mapping m({ &SA1::mmc_cpu_read, &sa1 }, { &SA1::mmc_cpu_write, &sa1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : iram) {
    if(node.name != "map") continue;
    Mapping m(sa1.cpuiram);
    m.clazz = MemoryClass::SA1IRAM;
    parse_markup_map(m, node);
    if(m.size == 0) m.size = 2048;
    mapping.append(m);
  }

  ram_size = parse_markup_integer(bwram["size"].data);
  for(auto &node : bwram) {
    if(node.name != "map") continue;
    Mapping m(sa1.cpubwram);
    //m.clazz = MemoryClass::SA1BWRAM;   -- Aliases SRAM
    parse_markup_map(m, node);
    if(m.size == 0) m.size = ram_size;
    mapping.append(m);
  }

  for(auto &node : mmio) {
    if(node.name != "map") continue;
    Mapping m({ &SA1::mmio_read, &sa1 }, { &SA1::mmio_write, &sa1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_necdsp(XML::Node &root) {
  if(root.exists() == false) return;
  has_necdsp = true;

  for(unsigned n = 0; n < 16384; n++) necdsp.programROM[n] = 0x000000;
  for(unsigned n = 0; n <  2048; n++) necdsp.dataROM[n] = 0x0000;

  necdsp.frequency = parse_markup_integer(root["frequency"].data);
  if(necdsp.frequency == 0) necdsp.frequency = 8000000;
  necdsp.revision
  = root["model"].data == "uPD7725"  ? NECDSP::Revision::uPD7725
  : root["model"].data == "uPD96050" ? NECDSP::Revision::uPD96050
  : NECDSP::Revision::uPD7725;
  string firmware = root["firmware"].data;
  string sha256 = root["sha256"].data;

  string path = { dir(interface->path(Slot::Base, ".dsp")), firmware };
  unsigned promsize = (necdsp.revision == NECDSP::Revision::uPD7725 ? 2048 : 16384);
  unsigned dromsize = (necdsp.revision == NECDSP::Revision::uPD7725 ? 1024 :  2048);
  unsigned filesize = promsize * 3 + dromsize * 2;

  file fp;
  if(fp.open(path, file::mode::read) == false) {
    interface->message({ "Warning: NEC DSP firmware ", firmware, " is missing." });
  } else if(fp.size() != filesize) {
    interface->message({ "Warning: NEC DSP firmware ", firmware, " is of the wrong file size." });
    fp.close();
  } else {
    for(unsigned n = 0; n < promsize; n++) necdsp.programROM[n] = fp.readm(3);
    for(unsigned n = 0; n < dromsize; n++) necdsp.dataROM[n] = fp.readm(2);

    if(sha256 != "") {
      //XML file specified SHA256 sum for program. Verify file matches the hash.
      fp.seek(0);
      uint8_t data[filesize];
      fp.read(data, filesize);

      if(sha256 != nall::sha256(data, filesize)) {
        interface->message({ "Warning: NEC DSP firmware ", firmware, " SHA256 sum is incorrect." });
      }
    }

    fp.close();
  }

  for(auto &node : root) {
    if(node.name == "dr") {
      for(auto &leaf : node) {
        Mapping m({ &NECDSP::dr_read, &necdsp }, { &NECDSP::dr_write, &necdsp });
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
    if(node.name == "sr") {
      for(auto &leaf : node) {
        Mapping m({ &NECDSP::sr_read, &necdsp }, { &NECDSP::sr_write, &necdsp });
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
    if(node.name == "dp") {
      for(auto &leaf : node) {
        Mapping m({ &NECDSP::dp_read, &necdsp }, { &NECDSP::dp_write, &necdsp });
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
  }
}

void Cartridge::parse_markup_hitachidsp(XML::Node &root) {
  if(root.exists() == false) return;
  has_hitachidsp = true;

  for(unsigned n = 0; n < 1024; n++) hitachidsp.dataROM[n] = 0x000000;

  hitachidsp.frequency = parse_markup_integer(root["frequency"].data);
  if(hitachidsp.frequency == 0) hitachidsp.frequency = 20000000;
  string firmware = root["firmware"].data;
  string sha256 = root["sha256"].data;

  string path = { dir(interface->path(Slot::Base, ".dsp")), firmware };
  file fp;
  if(fp.open(path, file::mode::read) == false) {
    interface->message({ "Warning: Hitachi DSP firmware ", firmware, " is missing." });
  } else if(fp.size() != 1024 * 3) {
    interface->message({ "Warning: Hitachi DSP firmware ", firmware, " is of the wrong file size." });
    fp.close();
  } else {
    for(unsigned n = 0; n < 1024; n++) hitachidsp.dataROM[n] = fp.readl(3);

    if(sha256 != "") {
      //XML file specified SHA256 sum for program. Verify file matches the hash.
      fp.seek(0);
      uint8 data[3072];
      fp.read(data, 3072);

      if(sha256 != nall::sha256(data, 3072)) {
        interface->message({ "Warning: Hitachi DSP firmware ", firmware, " SHA256 sum is incorrect." });
      }
    }

    fp.close();
  }

  for(auto &node : root) {
    if(node.name == "rom") {
      for(auto &leaf : node) {
        if(leaf.name != "map") continue;
        Mapping m({ &HitachiDSP::rom_read, &hitachidsp }, { &HitachiDSP::rom_write, &hitachidsp });
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
    if(node.name == "mmio") {
      for(auto &leaf : node) {
        Mapping m({ &HitachiDSP::dsp_read, &hitachidsp }, { &HitachiDSP::dsp_write, &hitachidsp });
        parse_markup_map(m, leaf);
        mapping.append(m);
      }
    }
  }
}

void Cartridge::parse_markup_bsx(XML::Node &root) {
  if(root.exists() == false) return;
  if(mode != Mode::BsxSlotted && mode != Mode::Bsx) return;

  for(auto &node : root["slot"]) {
    if(node.name != "map") continue;
    Mapping m(bsxflash.memory);
    m.clazz = MemoryClass::BSXFLASH;
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : root["mmio"]) {
    if(node.name != "map") continue;
    Mapping m({ &BSXCartridge::mmio_read, &bsxcartridge }, { &BSXCartridge::mmio_write, &bsxcartridge });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : root["mcu"]) {
    if(node.name != "map") continue;
    Mapping m({ &BSXCartridge::mcu_read, &bsxcartridge }, { &BSXCartridge::mcu_write, &bsxcartridge });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_sufamiturbo(XML::Node &root) {
  if(root.exists() == false) return;
  if(mode != Mode::SufamiTurbo) return;

  for(auto &slot : root) {
    if(slot.name != "slot") continue;
    bool slotid = slot["id"].data == "A" ? 0 : slot["id"].data == "B" ? 1 : 0;
    for(auto &node : slot) {
      if(node.name == "rom") {
        for(auto &leaf : node) {
          if(leaf.name != "map") continue;
          Memory &memory = slotid == 0 ? sufamiturbo.slotA.rom : sufamiturbo.slotB.rom;
          Mapping m(memory);
          m.clazz = slotid ? MemoryClass::SUFAMITURBO_ROMB : MemoryClass::SUFAMITURBO_ROMA;
          parse_markup_map(m, leaf);
          if(m.size == 0) m.size = memory.size();
          if(m.size) mapping.append(m);
        }
      }
      if(node.name == "ram") {
        unsigned ram_size = parse_markup_integer(node["size"].data);
        for(auto &leaf : node) {
          if(leaf.name != "map") continue;
          Memory &memory = slotid == 0 ? sufamiturbo.slotA.ram : sufamiturbo.slotB.ram;
          Mapping m(memory);
          m.clazz = slotid ? MemoryClass::SUFAMITURBO_RAMB : MemoryClass::SUFAMITURBO_RAMA;
          parse_markup_map(m, leaf);
          if(m.size == 0) m.size = ram_size;
          if(m.size) mapping.append(m);
        }
      }
    }
  }
}

void Cartridge::parse_markup_srtc(XML::Node &root) {
  if(root.exists() == false) return;
  has_srtc = true;

  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m({ &SRTC::read, &srtc }, { &SRTC::write, &srtc });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_sdd1(XML::Node &root) {
  if(root.exists() == false) return;
  has_sdd1 = true;

  for(auto &node : root["mmio"]) {
    if(node.name != "map") continue;
    Mapping m({ &SDD1::mmio_read, &sdd1 }, { &SDD1::mmio_write, &sdd1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : root["mcu"]) {
    if(node.name != "map") continue;
    Mapping m({ &SDD1::mcu_read, &sdd1 }, { &SDD1::mcu_write, &sdd1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_spc7110(XML::Node &root) {
  if(root.exists() == false) return;
  has_spc7110 = true;

  auto &ram = root["ram"];
  auto &mmio = root["mmio"];
  auto &mcu = root["mcu"];
  auto &dcu = root["dcu"];
  auto &rtc = root["rtc"];

  ram_size = parse_markup_integer(ram["size"].data);
  for(auto &node : ram) {
    if(node.name != "map") continue;
    Mapping m({ &SPC7110::ram_read, &spc7110 }, { &SPC7110::ram_write, &spc7110 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : mmio) {
    if(node.name != "map") continue;
    Mapping m({ &SPC7110::mmio_read, &spc7110 }, { &SPC7110::mmio_write, &spc7110 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  spc7110.data_rom_offset = parse_markup_integer(mcu["offset"].data);
  if(spc7110.data_rom_offset == 0) spc7110.data_rom_offset = 0x100000;
  for(auto &node : mcu) {
    if(node.name != "map") continue;
    Mapping m({ &SPC7110::mcu_read, &spc7110 }, { &SPC7110::mcu_write, &spc7110 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : dcu) {
    if(node.name != "map") continue;
    Mapping m({ &SPC7110::dcu_read, &spc7110 }, { &SPC7110::dcu_write, &spc7110 });
    parse_markup_map(m, node);
    mapping.append(m);
  }

  for(auto &node : rtc) {
    if(node.name != "map") continue;
    Mapping m({ &SPC7110::mmio_read, &spc7110 }, { &SPC7110::mmio_write, &spc7110 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_obc1(XML::Node &root) {
  if(root.exists() == false) return;
  has_obc1 = true;

  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m({ &OBC1::read, &obc1 }, { &OBC1::write, &obc1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_setarisc(XML::Node &root) {
  if(root.exists() == false) return;
  has_st0018 = true;

  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m({ &ST0018::mmio_read, &st0018 }, { &ST0018::mmio_write, &st0018 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_msu1(XML::Node &root) {
  if(root.exists() == false) {
    has_msu1 = file::exists(interface->path(Cartridge::Slot::Base, ".msu"));
    if(has_msu1) {
      Mapping m({ &MSU1::mmio_read, &msu1 }, { &MSU1::mmio_write, &msu1 });
      m.banklo = 0x00, m.bankhi = 0x3f, m.addrlo = 0x2000, m.addrhi = 0x2007;
      mapping.append(m);
      m.banklo = 0x80, m.bankhi = 0xbf, m.addrlo = 0x2000, m.addrhi = 0x2007;
      mapping.append(m);
    }
    return;
  }

  has_msu1 = true;

  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m({ &MSU1::mmio_read, &msu1 }, { &MSU1::mmio_write, &msu1 });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

void Cartridge::parse_markup_link(XML::Node &root) {
  if(root.exists() == false) return;
  has_link = true;

  link.frequency = max(1, parse_markup_integer(root["frequency"].data));
  link.program = root["program"].data;

  for(auto &node : root) {
    if(node.name != "map") continue;
    Mapping m({ &Link::read, &link }, { &Link::write, &link });
    parse_markup_map(m, node);
    mapping.append(m);
  }
}

Cartridge::Mapping::Mapping() {
  clazz = MemoryClass::MISC;
  mode = Bus::MapMode::Direct;
  banklo = bankhi = addrlo = addrhi = offset = size = 0;
}

Cartridge::Mapping::Mapping(Memory &memory) {
  clazz = MemoryClass::MISC;
  read = { &Memory::read, &memory };
  write = { &Memory::write, &memory };
  mode = Bus::MapMode::Direct;
  banklo = bankhi = addrlo = addrhi = offset = size = 0;
}

Cartridge::Mapping::Mapping(const function<uint8 (unsigned)> &read_, const function<void (unsigned, uint8)> &write_) {
  read = read_;
  write = write_;
  mode = Bus::MapMode::Direct;
  banklo = bankhi = addrlo = addrhi = offset = size = 0;
  clazz = MemoryClass::MISC;
}

#endif
