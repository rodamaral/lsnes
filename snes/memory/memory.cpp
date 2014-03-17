#include <snes/snes.hpp>

#define MEMORY_CPP
namespace SNES {

Bus bus;

unsigned Bus::mirror(unsigned addr, unsigned size) {
  unsigned base = 0;
  if(size) {
    unsigned mask = 1 << 23;
    while(addr >= size) {
      while(!(addr & mask)) mask >>= 1;
      addr -= mask;
      if(size > mask) {
        size -= mask;
        base += mask;
      }
      mask >>= 1;
    }
    base += addr;
  }
  return base;
}

void Bus::map(
  MapMode mode,
  unsigned bank_lo, unsigned bank_hi,
  unsigned addr_lo, unsigned addr_hi,
  unsigned mclass,
  const function<uint8 (unsigned)> &rd,
  const function<void (unsigned, uint8)> &wr,
  unsigned base, unsigned length
) {
  assert(bank_lo <= bank_hi && bank_lo <= 0xff);
  assert(addr_lo <= addr_hi && addr_lo <= 0xffff);
  unsigned id = idcount++;
  assert(id < 255);
  reader[id] = rd;
  writer[id] = wr;

  if(length == 0) length = (bank_hi - bank_lo + 1) * (addr_hi - addr_lo + 1);

  unsigned offset = 0;
  for(unsigned bank = bank_lo; bank <= bank_hi; bank++) {
    region_start.insert((bank << 16) | addr_lo);
    for(unsigned addr = addr_lo; addr <= addr_hi; addr++) {
      unsigned destaddr = (bank << 16) | addr;
      if(mode == MapMode::Linear) destaddr = mirror(base + offset++, length);
      if(mode == MapMode::Shadow) destaddr = mirror(base + destaddr, length);
      lookup[(bank << 16) | addr] = id;
      target[(bank << 16) | addr] = destaddr;
      if(mclass) classmap[(bank << 16) | addr] = mclass;
    }
  }
}

void Bus::map_reset() {
  function<uint8 (unsigned)> reader = [](unsigned) { return cpu.regs.mdr; };
  function<void (unsigned, uint8)> writer = [](unsigned, uint8) {};

  idcount = 0;
  map(MapMode::Direct, 0x00, 0xff, 0x0000, 0xffff, 0xFF, reader, writer);
  region_start.clear();
}

void Bus::map_xml() {
  for(auto &m : cartridge.mapping) {
    map(m.mode, m.banklo, m.bankhi, m.addrlo, m.addrhi, (unsigned)m.clazz, m.read, m.write, m.offset, m.size);
  }
}

unsigned Bus::enumerateMirrors(uint8 clazz, uint32 offset, unsigned start)
{
  if(clazz == 255) {
    if(start > offset)
      return 0x1000000;
    else
      return start;
  }
  //Given region can not contain the same address twice.
  for(std::set<uint32>::iterator i = region_start.lower_bound(start); i != region_start.end(); i++) {
    if(classmap[*i] != clazz) continue;
    if(target[*i] > offset) continue;
    uint32 wouldbe = offset - target[*i] + *i;
    if(wouldbe > 0xFFFFFF) continue;
    if(classmap[wouldbe] == clazz && target[wouldbe] == offset) return wouldbe;
  }
  return 0x1000000;
}

void Bus::clearDebugFlags()
{
  u_debugflags = 0;
  memset(debugflags, 0, 0x1000000);
}

void Bus::debugFlags(uint8 setf, uint8 clrf)
{
  u_debugflags = (u_debugflags | setf) & ~clrf;
}

void Bus::debugFlags(uint8 setf, uint8 clrf, uint8 clazz, uint32 offset)
{
  if(clazz == 255) {
    setf <<= 3;
    clrf <<= 3;
    debugflags[offset] = (debugflags[offset] | setf) & ~clrf;
  } else {
    uint32 i = 0;
    while(true) {
      i = enumerateMirrors(clazz, offset, i);
      if(i >= 0x1000000) break;
      debugflags[i] = (debugflags[i] | setf) & ~clrf;
      i++;
    }
  }
}

Bus::Bus() {
  u_debugflags = 0;
  lookup = new uint8 [112 * 1024 * 1024];
  target = (uint32*)(lookup + 0x3000000);
  classmap = lookup + 0x1000000;
  debugflags = lookup + 0x2000000;
  memset(debugflags, 0, 0x1000000);
}

Bus::~Bus() {
  delete[] lookup;
}

}
