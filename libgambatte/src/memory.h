/***************************************************************************
 *   Copyright (C) 2007 by Sindre Aamås                                    *
 *   sinamas@users.sourceforge.net                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License version 2 for more details.                *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   version 2 along with this program; if not, write to the               *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef MEMORY_H
#define MEMORY_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "gambatte.h"
#include "mem/cartridge.h"
#include "interrupter.h"
#include "pakinfo.h"
#include "sound.h"
#include "tima.h"
#include "video.h"

namespace gambatte {

class InputGetter;
class FilterInfo;

class Memory {
public:
	explicit Memory(Interrupter const &interrupter, time_t (**gettime)());
	bool loaded() const { return cart_.loaded(); }
	char const * romTitle() const { return cart_.romTitle(); }
	PakInfo const pakInfo(bool multicartCompat) const { return cart_.pakInfo(multicartCompat); }
	void setStatePtrs(SaveState &state);
	unsigned saveState(SaveState &state, unsigned cc);
	void loadOrSave(loadsave& state);
	void loadState(SaveState const &state);
	void loadSavedata() { cart_.loadSavedata(); }
	void saveSavedata() { cart_.saveSavedata(); }
	std::string const saveBasePath() const { return cart_.saveBasePath(); }

	void setOsdElement(transfer_ptr<OsdElement> osdElement) {
		lcd_.setOsdElement(osdElement);
	}

	unsigned stop(unsigned cycleCounter);
	bool isCgb() const { return lcd_.isCgb(); }
	bool ime() const { return intreq_.ime(); }
	bool halted() const { return intreq_.halted(); }
	unsigned nextEventTime() const { return intreq_.minEventTime(); }
	bool isActive() const { return intreq_.eventTime(intevent_end) != disabled_time; }

	signed cyclesSinceBlit(unsigned cc) const {
		if (cc < intreq_.eventTime(intevent_blit))
			return -1;

		return (cc - intreq_.eventTime(intevent_blit)) >> isDoubleSpeed();
	}

	void halt() { intreq_.halt(); }
	void ei(unsigned cycleCounter) { if (!ime()) { intreq_.ei(cycleCounter); } }
	void di() { intreq_.di(); }

	void set_debug_buffer(debugbuffer& dbgbuf)
	{
		dbg = &dbgbuf;
	}

	unsigned ff_read(unsigned p, unsigned cc) {
		uint8_t v = p < 0x80 ? nontrivial_ff_read(p, cc) : ioamhram_[p + 0x100];
		if(__builtin_expect(dbg->ioamhram[0x100 + p] & 1, 0))
			dbg->read(2, 0x100 + p, v, false);
		if(__builtin_expect(dbg->bus[0xFF00 + p] & 1, 0))
			dbg->read(0, 0xFF00 + p, v, false);
		return v;
	}

	inline uint8_t do_read_trap(const uint8_t* addr, std::pair<unsigned char*, size_t> area, unsigned clazz,
		const uint8_t* dbgflags, std::map<unsigned, uint8_t>& cheats, uint8_t v, uint8_t mask, bool exec)
	{
		if(addr >= area.first && addr < area.first + area.second) {
			if(__builtin_expect(dbgflags[addr - area.first] & mask, 0)) {
				if(dbgflags[addr - area.first] & (mask & 77))
					dbg->read(clazz, addr - area.first, v, exec);
				if(__builtin_expect(dbgflags[addr - area.first] & 8, 0)) {
					auto itr = cheats.find(addr - area.first);
					if(itr != cheats.end()) v = itr->second;
				}
			}
		}
		return v;
	}

	inline void do_write_trap(const uint8_t* addr, std::pair<unsigned char*, size_t> area, unsigned clazz,
		const uint8_t* dbgflags, uint8_t v)
	{
		if(addr >= area.first && addr < area.first + area.second)
			if(__builtin_expect(dbgflags[addr - area.first] & 0x22, 0))
				dbg->write(clazz, addr - area.first, v);
	}

	unsigned read(unsigned p, unsigned cc, bool exec) {
		uint8_t mask = exec ? 0x4C : 0x19;
		const unsigned char* memblock = cart_.rmem(p >> 12);
		uint8_t v = memblock ? memblock[p] : nontrivial_read(p, cc, exec);
		uint8_t v2 = v;
		if(memblock) {
			if(p >= 0xFE00) { //IOAMHRAM.
				if(__builtin_expect(dbg->ioamhram[p - 0xFE00] & mask, 0))
					dbg->read(2, 0x100 + p, v, exec);
			} else {
				const uint8_t* addr = memblock + p;
				static void* targets[8] = {&&cart, &&cart, &&cart, &&cart, &&out, &&sram, &&wram,
					&&wram};
				goto *targets[p >> 13];
wram:
				v2 = do_read_trap(addr, cart_.getWorkRam(), 1, dbg->wram, dbg->wramcheat, v, mask, exec);
				goto out;
sram:
				v2 = do_read_trap(addr, cart_.getSaveRam(), 4, dbg->sram, dbg->sramcheat, v, mask, exec);
				goto out;
cart:
				v2 = do_read_trap(addr, cart_.getCartRom(), 3, dbg->cart, dbg->cartcheat, v, mask, exec);
				goto out;
			}
out:			;
		}
		if(__builtin_expect(dbg->bus[p] & mask, 0))
			dbg->read(0, p, v, exec);
		return v2;
	}

	void write(unsigned p, unsigned data, unsigned cc) {
		if(__builtin_expect(dbg->bus[p] & 0x22, 0))
			dbg->write(0, p, data);
		unsigned char* memblock = cart_.wmem(p >> 12);
		if(memblock) {
			if(p >= 0xFE00)	//IOAMHRAM.
				if(__builtin_expect(dbg->ioamhram[p - 0xFE00] & 2, 0))
					dbg->write(2, 0x100 + p, data);
			uint8_t* addr = memblock + p;
			do_write_trap(addr, cart_.getWorkRam(), 1, dbg->wram, data);
			do_write_trap(addr, cart_.getSaveRam(), 4, dbg->sram, data);
			do_write_trap(addr, cart_.getCartRom(), 3, dbg->cart, data);
		}
		if (memblock) {
			memblock[p] = data;
		} else
			nontrivial_write(p, data, cc);
	}

	void ff_write(unsigned p, unsigned data, unsigned cc) {
		if(__builtin_expect(dbg->ioamhram[0x100 + p] & 2, 0))
			dbg->write(2, 0x100 + p, data);
		if(__builtin_expect(dbg->bus[0xFF00 + p] & 2, 0))
			dbg->write(0, 0xFF00 + p, data);
		if (p - 0x80u < 0x7Fu) {
			ioamhram_[p + 0x100] = data;
		} else
			nontrivial_ff_write(p, data, cc);
	}

	LoadRes loadROM(const std::string &romfile, bool forceDmg, bool multicartCompat);
	LoadRes loadROM(const unsigned char* image, size_t isize, bool forceDmg, bool multicartCompat);

	void setVideoBuffer(uint_least32_t *const videoBuf, std::ptrdiff_t pitch) {
		videoBuf_ = videoBuf;
		pitch_ = pitch;
	}

	void setRtcBase(time_t time) { cart_.setRtcBase(time); }
	time_t getRtcBase() { return cart_.getRtcBase(); }
	std::pair<unsigned char*, size_t> getWorkRam() { return cart_.getWorkRam(); }
	std::pair<unsigned char*, size_t> getSaveRam() { return cart_.getSaveRam(); }
	std::pair<unsigned char*, size_t> getIoRam() { return std::make_pair(ioamhram_, sizeof(ioamhram_)); }
	std::pair<unsigned char*, size_t> getVideoRam() { return cart_.getVideoRam(); };

	unsigned event(unsigned cycleCounter);
	unsigned resetCounters(unsigned cycleCounter);
	void setSaveDir(std::string const &dir) { cart_.setSaveDir(dir); }
	void setInputGetter(InputGetter *getInput) { getInput_ = getInput; }
	void setEndtime(unsigned cc, unsigned inc);
	void setSoundBuffer(uint_least32_t *buf) { psg_.setBuffer(buf); }
	unsigned fillSoundBuffer(unsigned cc);

	void setDmgPaletteColor(unsigned palNum, unsigned colorNum, uint_least32_t rgb32);
	void setGameGenie(std::string const &codes) { cart_.setGameGenie(codes); }
	void setGameShark(std::string const &codes) { interrupter_.setGameShark(codes); }
	debugbuffer* get_debug() { return dbg; }
private:
	debugbuffer* dbg;
	Cartridge cart_;
	unsigned char ioamhram_[0x200];
	InputGetter *getInput_;
	unsigned divLastUpdate_;
	unsigned lastOamDmaUpdate_;
	InterruptRequester intreq_;
	Tima tima_;
	LCD lcd_;
	PSG psg_;
	Interrupter interrupter_;
	unsigned short dmaSource_;
	unsigned short dmaDestination_;
	unsigned char oamDmaPos_;
	unsigned char serialCnt_;
	bool blanklcd_;
	uint_least32_t* videoBuf_;
	unsigned pitch_;

	void updateInput();
	void decEventCycles(IntEventId eventId, unsigned dec);
	void oamDmaInitSetup();
	void updateOamDma(unsigned cycleCounter);
	void startOamDma(unsigned cycleCounter);
	void endOamDma(unsigned cycleCounter);
	unsigned char const * oamDmaSrcPtr() const;
	unsigned nontrivial_ff_read(unsigned p, unsigned cycleCounter);
	unsigned nontrivial_read(unsigned p, unsigned cycleCounter, bool exec);
	void nontrivial_ff_write(unsigned p, unsigned data, unsigned cycleCounter);
	void nontrivial_write(unsigned p, unsigned data, unsigned cycleCounter);
	void updateSerial(unsigned cc);
	void updateTimaIrq(unsigned cc);
	void updateIrqs(unsigned cc);
	bool isDoubleSpeed() const { return lcd_.isDoubleSpeed(); }
	void postLoadRom();
};

}

#endif
