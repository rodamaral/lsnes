uint8 ram_read(uint16 addr);
void ram_write(uint16 addr, uint8 data);

uint8 op_busread(uint16 addr, bool exec);
void op_buswrite(uint16 addr, uint8 data);

void op_io();
debugvirtual uint8 op_read(uint16 addr, bool exec);
debugvirtual void op_write(uint16 addr, uint8 data);
