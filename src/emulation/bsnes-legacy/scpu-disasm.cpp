#include "interface/disassembler.hpp"
#include "library/hex.hpp"
#include "library/string.hpp"
#include <functional>
#include <sstream>
#include <iomanip>

namespace
{
	template<bool k> struct varsize {};
	template<> struct varsize<false> { typedef uint8_t type_t; };
	template<> struct varsize<true> { typedef uint16_t type_t; };

	template<bool laddr, bool lacc> struct scpu_disassembler : public disassembler
	{
		scpu_disassembler(const std::string& n) : disassembler(n) {}
		std::string disassemble(uint64_t base, std::function<unsigned char()> fetchpc);
	};

	struct ssmp_disassembler : public disassembler
	{
		ssmp_disassembler(const std::string& n) : disassembler(n) {}
		std::string disassemble(uint64_t base, std::function<unsigned char()> fetchpc);
	};

	scpu_disassembler<false, false> d1("snes-xa");
	scpu_disassembler<false, true> d2("snes-xA");
	scpu_disassembler<true, false> d3("snes-Xa");
	scpu_disassembler<true, true> d4("snes-XA");
	ssmp_disassembler d5("snes-smp");

	//%0: Byte-sized quantity, loaded before %b.
	//%a: Accumulator-sized quantity.
	//%b: Byte-sized quantity.
	//%l: Long address.
	//%r: Byte relative.
	//%R: Word relative.
	//%w: Word-sized quantity.
	//%x: Index-sizeed quantity.
	const char* scpu_instructions[] = {
		"brk #%b", 	"ora (%b,x)",	"cop #%b",	"ora %b,s",		//00
		"tsb %b",	"ora %b",	"asl %b",	"ora [%b]",		//04
		"php",		"ora #%a",	"asl a",	"phd",			//08
		"tsb %w",	"ora %w",	"asl %w",	"ora %l",		//0C
		"bpl %r",	"ora (%b),y",	"ora (%b)",	"ora (%b,s),y",		//10
		"trb %b",	"ora %b,x",	"asl %b,x",	"ora [%b],y",		//14
		"clc",		"ora %w,y",	"inc",		"tcs",			//18
		"trb %w",	"ora %w,x",	"asl %w,x",	"ora %l,x",		//1C
		"jsr %w",	"and (%b,x)",	"jsl %l",	"and %b,s",		//20
		"bit %b",	"and %b",	"rol %b",	"and [%b]",		//24
		"plp",		"and #%a",	"rol a",	"pld",			//28
		"bit %w",	"and %w",	"rol %w",	"and %l",		//2C
		"bmi %r",	"and (%b),y",	"and (%b)",	"and (%b,s),y",		//30
		"bit %b",	"and %b,x",	"rol %b,x",	"and [%b],y",		//34
		"sec",		"and %w,y",	"dec",		"tsc",			//38
		"bit %w,x",	"and %w,x",	"rol %w,x",	"and %l,x",		//3C
		"rti", 		"eor (%b,x)",	"wdm #%b",	"eor %b,s",		//40
		"mvp %b,%0",	"eor %b",	"lsr %b",	"eor [%b]",		//44
		"pha",		"eor #%a",	"lsr a",	"phk",			//48
		"jmp %w",	"eor %w",	"lsr %w",	"eor %l",		//4C
		"bvc %r",	"eor (%b),y",	"eor (%b)",	"eor (%b,s),y",		//50
		"mnv %b,%0",	"eor %b,x",	"lsr %b,x",	"eor [%b],y",		//54
		"cli",		"eor %w,y",	"phy",		"tcd",			//58
		"jml %l",	"eor %w,x",	"lsr %w,x",	"eor %l,x",		//5C
		"rts", 		"adc (%b,x)",	"per %w",	"adc %b,s",		//60
		"stz %b",	"adc %b",	"ror %b",	"adc [%b]",		//64
		"pla",		"adc #%a",	"ror a",	"rtl",			//68
		"jmp (%w)",	"adc %w",	"ror %w",	"adc %l",		//6C
		"bvs %r",	"adc (%b),y",	"adc (%b)",	"adc (%b,s),y",		//70
		"stz %b,x",	"adc %b,x",	"ror %b,x",	"adc [%b],y",		//74
		"sei",		"adc %w,y",	"ply",		"tdc",			//78
		"jmp (%w,x)",	"adc %w,x",	"ror %w,x",	"adc %l,x",		//7C
		"bra %r",	"sta (%b,x)",	"brl %R",	"sta %b,s",		//80
		"sty %b",	"sta %b",	"stx %b",	"sta [%b]",		//84
		"dey",		"bit #%a",	"txa",		"phb",			//88
		"sty %w",	"sta %w",	"stx %w",	"sta %l",		//8C
		"bcc %r",	"sta (%b),y",	"sta (%b)",	"sta (%b,s),y",		//90
		"sty %b,x",	"sta %b,x",	"stx %b,y",	"sta [%b],y",		//94
		"tya",		"sta %w,y",	"txs",		"txy",			//98
		"stz %w",	"sta %w,x",	"stz %w,x",	"sta %l,x",		//9C
		"ldy #%x",	"lda (%b,x)",	"ldx #%x",	"lda %b,s",		//A0
		"ldy %b",	"lda %b",	"ldx %b",	"lda [%b]",		//A4
		"tay",		"lda #%a",	"tax",		"plb",			//A8
		"ldy %w",	"lda %w",	"ldx %w",	"lda %l",		//AC
		"bcs %r",	"lda (%b),y",	"lda (%b)",	"lda (%b,s),y",		//B0
		"ldy %b,x",	"lda %b,x",	"ldx %b,y",	"lda [%b],y",		//B4
		"clv",		"lda %w,y",	"tsx",		"tyx",			//B8
		"ldy %w,x",	"lda %w,x",	"ldx %w,x",	"lda %l,x",		//BC
		"cpy #%x",	"cmp (%b,x)",	"rep #%b",	"cmp %b,s",		//C0
		"cpy %b",	"cmp %b",	"dec %b",	"cmp [%b]",		//C4
		"iny",		"cmp #%a",	"dex",		"wai",			//C8
		"cpy %w",	"cmp %w",	"dec %w",	"cmp %l",		//CC
		"bne %r",	"cmp (%b),y",	"cmp (%b)",	"cmp (%b,s),y",		//D0
		"pei (%b)",	"cmp %b,x",	"dec %b,y",	"cmp [%b],y",		//D4
		"cld",		"cmp %w,y",	"phx",		"stp",			//D8
		"jmp [%w]",	"cmp %w,x",	"dec %w,x",	"cmp %l,x",		//DC
		"cpx #%x",	"sbc (%b,x)",	"sep #%b",	"sbc %b,s",		//E0
		"cpx %b",	"sbc %b",	"inc %b",	"sbc [%b]",		//E4
		"inx",		"sbc #%a",	"nop",		"xba",			//E8
		"cpx %w",	"sbc %w",	"inc %w",	"sbc %l",		//EC
		"beq %r",	"sbc (%b),y",	"sbc (%b)",	"sbc (%b,s),y",		//F0
		"pea %w",	"sbc %b,x",	"inc %b,y",	"sbc [%b],y",		//F4
		"sed",		"sbc %w,y",	"plx",		"xce",			//F8
		"jsr (%w,x)",	"sbc %w,x",	"inc %w,x",	"sbc %l,x"		//FC
	};

	//%0: Byte-sized quantity, loaded before %b.
	//%b: Byte-sized quantity.
	//%c: ??? <n>:<m> construct.
	//%r: Relative
	//%R: Relative
	//%w: Word-sized quantity.
	const char* ssmp_instructions[] = {
		"nop",		"jst $ffde",	"set %b:0",	"bbs %b:0=%R",		//00
		"ora %b",	"ora %w",	"ora (x)",	"ora (%b,x)",		//04
		"ora #%b",	"orr %b=%0",	"orc %c",	"asl %b",		//08
		"asl %w",	"php",		"tsb %w",	"brk",			//0C
		"bpl %r",	"jst $ffdc",	"clr %b:0",	"bbc %b:0=%R",		//10
		"ora %b,x",	"ora %w,x",	"ora %w,y",	"ora (%b),y",		//14
		"orr %b=#%0",	"orr (x)=(y)",	"dew %b",	"asl %b,x",		//18
		"asl",		"dex",		"cpx %w",	"jmp (%w,x)",		//1C
		"clp",		"jst $ffda",	"set %b:1",	"bbs %b:1=%R",		//20
		"and %b",	"and %w",	"and (x)",	"and (%b,x)",		//24
		"and #%b",	"and %b=%0",	"orc !%c",	"rol %b",		//28
		"rol %w",	"pha",		"bne %b=%R",	"bra %r",		//2C
		"bmi %r",	"jst $ffd8",	"clr %b:1",	"bbc %b:1=$R",		//30
		"and %b,x",	"and %w,x",	"and %w,y",	"and (%b),y",		//34
		"and %b=#%0",	"and (x)=(y)",	"inw %b",	"rol $b,x",		//38
		"rol",		"inx",		"cpx %b",	"jsr %w",		//3C
		"sep",		"jst $ffd6",	"set %b:2",	"bbs %b:2=$R",		//40
		"eor %b",	"eor %w",	"eor (x)",	"eor ($b,x)",		//44
		"eor #%b",	"eor %b=%0",	"and %c",	"lsr %b",		//48
		"lsr %w",	"phx",		"trb %w",	"jsp $ff",		//4C
		"bvc %r",	"jst $ffd4",	"clr %b:2",	"bbc %b:2=%R",		//50
		"eor %b,x",	"eor %w,x",	"eor %w,y",	"eor (%b),y",		//54
		"eor %b=#%0",	"eor (x)=(y)",	"cpw %w",	"lsr %b,x",		//58
		"lsr",		"tax",		"cpy %w",	"jmp %w",		//5C
		"clc",		"jst $ffd2",	"set %b:3",	"bbs %b:3=$R",		//60
		"cmp %b",	"cmp %w",	"cmp (x)",	"cmp ($b,x)",		//64
		"cmp #%b",	"cmp %b=%0",	"and !%c",	"ror %b",		//68
		"ror %w",	"phy",		"bne --$b=%R",	"rts",			//6C
		"bvs %r",	"jst $ffd0",	"clr %b:3",	"bbc %b:3=%R",		//70
		"cmp %b,x",	"cmp %w,x",	"cmp %w,y",	"cmp (%b),y",		//74
		"cmp %b=#%0",	"cmp (x)=(y)",	"adw %w",	"ror %b,x",		//78
		"ror",		"txa",		"cpy %b",	"rti",			//7C
		"sec",		"jst $ffce",	"set %b:4",	"bbs %b:4=$R",		//80
		"adc %b",	"adc %w",	"adc (x)",	"adc ($b,x)",		//84
		"adc #%b",	"cmp %b=%0",	"eor %c",	"dec %b",		//88
		"dec %w",	"ldy #%b",	"plp",		"str %b=#%0",		//8C
		"bcc %r",	"jst $ffcc",	"clr %b:4",	"bbc %b:4=%R",		//90
		"adc %b,x",	"adc %w,x",	"adc %w,y",	"adc (%b),y",		//94
		"adc %b=#%0",	"adc (x)=(y)",	"sbw %w",	"dec %b,x",		//98
		"dec",		"tsx",		"div",		"xcn",			//9C
		"sei",		"jst $ffca",	"set %b:5",	"bbs %b:5=$R",		//A0
		"sbc %b",	"sbc %w",	"sbc (x)",	"sbc ($b,x)",		//A4
		"sbc #%b",	"sbc %b=%0",	"ldc %c",	"inc %b",		//A8
		"inc %w",	"cpy #%b",	"pla",		"sta (x++)",		//AC
		"bcs %r",	"jst $ffc8",	"clr %b:5",	"bbc %b:5=%R",		//B0
		"sbc %b,x",	"sbc %w,x",	"sbc %w,y",	"sbc (%b),y",		//B4
		"sbc %b=#%0",	"sbc (x)=(y)",	"ldw %b",	"inc %b,x",		//B8
		"inc",		"txs",		"das",		"lda (x++)",		//BC
		"cli",		"jst $ffc6",	"set %b:6",	"bbs %b:6=$R",		//C0
		"sta %b",	"sta %w",	"sta (x)",	"sta ($b,x)",		//C4
		"cpx #%b",	"stx %w",	"stc %c",	"sty %b",		//C8
		"sty %w",	"ldx #%b",	"plx",		"mul",			//CC
		"bne %r",	"jst $ffc4",	"clr %b:6",	"bbc %b:6=%R",		//D0
		"sta %b,x",	"sta %w,x",	"sta %w,y",	"sta (%b),y",		//D4
		"stx %b",	"stx %b,y",	"stw %b",	"stw %b,x",		//D8
		"dey",		"tya",		"bne %b,x=%R",	"daa",			//DC
		"clv",		"jst $ffc2",	"set %b:7",	"bbs %b:7=$R",		//E0
		"lda %b",	"lda %w",	"lda (x)",	"lda ($b,x)",		//E4
		"lda #%b",	"ldx %w",	"not %c",	"ldy %b",		//E8
		"ldy %w",	"cmc",		"ply",		"wai",			//EC
		"beq %r",	"jst $ffc0",	"clr %b:7",	"bbc %b:7=%R",		//F0
		"lda %b,x",	"lda %w,x",	"lda %w,y",	"lda (%b),y",		//F4
		"ldx %b",	"ldx %b,y",	"str %b=%0",	"ldy %b,x",		//F8
		"iny",		"tay",		"bne --y=%r",	"stp"			//FC
	};

	template<bool laddr, bool lacc>
	std::string scpu_disassembler<laddr, lacc>::disassemble(uint64_t base, std::function<unsigned char()> fetchpc)
	{
		std::ostringstream o;
		const char* ins = scpu_instructions[fetchpc()];
		uint8_t x = 0;
		//Handle %0 specially.
		for(size_t i = 0; ins[i]; i++)
			if(ins[i] == '%' && ins[i + 1] == '0')
				x = fetchpc();

		for(size_t i = 0; ins[i]; i++) {
			if(ins[i] != '%')
				o << ins[i];
			else {
				switch(ins[i + 1]) {
				case '0':
					o << "$" << hex::to(x);
					break;
				case 'a':
					o << "$" << hex::to(fetch_le<typename varsize<lacc>::type_t>(fetchpc));
					break;
				case 'b':
					o << "$" << hex::to(fetch_le<uint8_t>(fetchpc));
					break;
				case 'l': {
					uint16_t offset = fetch_le<uint16_t>(fetchpc);
					o << "$" << hex::to(fetch_le<uint8_t>(fetchpc));
					o << hex::to(offset);
					break;
				}
				case 'r':
					o << "$" << hex::to(static_cast<uint16_t>(base + 2 +
						fetch_le<int8_t>(fetchpc)));
					break;
				case 'R':
					o << "$" << hex::to(static_cast<uint16_t>(base + 3 +
						fetch_le<int16_t>(fetchpc)));
					break;
				case 'w':
					o << "$" << hex::to(fetch_le<uint16_t>(fetchpc));
					break;
				case 'x':
					o << "$" << hex::to(fetch_le<typename varsize<laddr>::type_t>(fetchpc));
					break;
				}
				i++;
			}
		}
		return o.str();
	}

	std::string ssmp_disassembler::disassemble(uint64_t base, std::function<unsigned char()> fetchpc)
	{
		std::ostringstream o;
		const char* ins = ssmp_instructions[fetchpc()];
		uint8_t x = 0;
		uint16_t tmp = 0;
		//Handle %0 specially.
		for(size_t i = 0; ins[i]; i++)
			if(ins[i] == '%' && ins[i + 1] == '0')
				x = fetchpc();

		for(size_t i = 0; ins[i]; i++) {
			if(ins[i] != '%')
				o << ins[i];
			else {
				switch(ins[i + 1]) {
				case '0':
					o << "$" << hex::to(x);
					break;
				case 'b':
					o << "$" << hex::to(fetch_le<uint8_t>(fetchpc));
					break;
				case 'c':
					tmp = fetch_le<uint16_t>(fetchpc);
					o << "$" << hex::to(static_cast<uint16_t>(tmp & 0x1FFF)) << ":"
						<< (tmp >> 13);
					break;
				case 'r':
					o << "$" << hex::to(static_cast<uint16_t>(base + 2 +
						fetch_le<int8_t>(fetchpc)));
					break;
				case 'R':
					o << "$" << hex::to(static_cast<uint16_t>(base + 3 +
						fetch_le<int8_t>(fetchpc)));
				case 'w':
					o << "$" << hex::to(fetch_le<uint16_t>(fetchpc));
					break;
				}
				i++;
			}
		}
		return o.str();
	}
}
