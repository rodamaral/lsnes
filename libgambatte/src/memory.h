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
	Cartridge cart;
	unsigned char ioamhram[0x200];
	
	InputGetter *getInput;
	unsigned divLastUpdate;
	unsigned lastOamDmaUpdate;
	
	InterruptRequester intreq;
	Tima tima;
	LCD display;
	PSG sound;
	Interrupter interrupter;
	
	unsigned short dmaSource;
	unsigned short dmaDestination;
	unsigned char oamDmaPos;
	unsigned char serialCnt;
	bool blanklcd;

	uint_least32_t* videoBuf_;
	unsigned pitch_;

	void updateInput();
	void decEventCycles(MemEventId eventId, unsigned dec);

	void oamDmaInitSetup();
	void updateOamDma(unsigned cycleCounter);
	void startOamDma(unsigned cycleCounter);
	void endOamDma(unsigned cycleCounter);
	const unsigned char * oamDmaSrcPtr() const;
	
	unsigned nontrivial_ff_read(unsigned P, unsigned cycleCounter);
	unsigned nontrivial_read(unsigned P, unsigned cycleCounter);
	void nontrivial_ff_write(unsigned P, unsigned data, unsigned cycleCounter);
	void nontrivial_write(unsigned P, unsigned data, unsigned cycleCounter);
	
	void updateSerial(unsigned cc);
	void updateTimaIrq(unsigned cc);
	void updateIrqs(unsigned cc);
	
	bool isDoubleSpeed() const { return display.isDoubleSpeed(); }

	void postLoadRom();
public:
	explicit Memory(const Interrupter &interrupter, time_t (**_getCurrentTime)());
	
	bool loaded() const { return cart.loaded(); }
	char const * romTitle() const { return cart.romTitle(); }
	PakInfo const pakInfo(bool multicartCompat) const { return cart.pakInfo(multicartCompat); }

	void loadOrSave(loadsave& state);

	void setStatePtrs(SaveState &state);
	unsigned saveState(SaveState &state, unsigned cc);
	void loadState(const SaveState &state/*, unsigned long oldCc*/);
	void loadSavedata() { cart.loadSavedata(); }
	void saveSavedata() { cart.saveSavedata(); }
	const std::string saveBasePath() const { return cart.saveBasePath(); }
	
	void setOsdElement(std::auto_ptr<OsdElement> osdElement) {
		display.setOsdElement(osdElement);
	}

	unsigned stop(unsigned cycleCounter);
	bool isCgb() const { return display.isCgb(); }
	bool ime() const { return intreq.ime(); }
	bool halted() const { return intreq.halted(); }
	unsigned nextEventTime() const { return intreq.minEventTime(); }
	
	bool isActive() const { return intreq.eventTime(END) != DISABLED_TIME; }
	
	signed cyclesSinceBlit(const unsigned cc) const {
		return cc < intreq.eventTime(BLIT) ? -1 : static_cast<signed>((cc - intreq.eventTime(BLIT)) >> isDoubleSpeed());
	}

	void halt() { intreq.halt(); }
	void ei(unsigned cycleCounter) { if (!ime()) { intreq.ei(cycleCounter); } }

	void di() { intreq.di(); }

	unsigned ff_read(const unsigned P, const unsigned cycleCounter) {
		return P < 0xFF80 ? nontrivial_ff_read(P, cycleCounter) : ioamhram[P - 0xFE00];
	}

	unsigned read(const unsigned P, const unsigned cycleCounter) {
		return cart.rmem(P >> 12) ? cart.rmem(P >> 12)[P] : nontrivial_read(P, cycleCounter);
	}

	void write(const unsigned P, const unsigned data, const unsigned cycleCounter) {
		if (cart.wmem(P >> 12)) {
			cart.wmem(P >> 12)[P] = data;
		} else
			nontrivial_write(P, data, cycleCounter);
	}
	
	void ff_write(const unsigned P, const unsigned data, const unsigned cycleCounter) {
		if (P - 0xFF80u < 0x7Fu) {
			ioamhram[P - 0xFE00] = data;
		} else
			nontrivial_ff_write(P, data, cycleCounter);
	}

	unsigned event(unsigned cycleCounter);
	unsigned resetCounters(unsigned cycleCounter);

	LoadRes loadROM(const std::string &romfile, bool forceDmg, bool multicartCompat);
	LoadRes loadROM(const unsigned char* image, size_t isize, bool forceDmg, bool multicartCompat);
	void setSaveDir(const std::string &dir) { cart.setSaveDir(dir); }

	void setInputGetter(InputGetter *getInput) {
		this->getInput = getInput;
	}

	void setEndtime(unsigned cc, unsigned inc);
	
	void setSoundBuffer(uint_least32_t *const buf) { sound.setBuffer(buf); }
	unsigned fillSoundBuffer(unsigned cc);
	
	void setVideoBuffer(uint_least32_t *const videoBuf, const int pitch) {
		videoBuf_ = videoBuf;
		pitch_ = pitch;
	}
	
	void setDmgPaletteColor(unsigned palNum, unsigned colorNum, uint_least32_t rgb32);
	void setGameGenie(const std::string &codes) { cart.setGameGenie(codes); }
	void setGameShark(const std::string &codes) { interrupter.setGameShark(codes); }

	void setRtcBase(time_t time) { cart.setRtcBase(time); }
	time_t getRtcBase() { return cart.getRtcBase(); }
	std::pair<unsigned char*, size_t> getWorkRam() { return cart.getWorkRam(); }
	std::pair<unsigned char*, size_t> getSaveRam() { return cart.getSaveRam(); }
	std::pair<unsigned char*, size_t> getIoRam() { return std::make_pair(ioamhram, sizeof(ioamhram)); }
	std::pair<unsigned char*, size_t> getVideoRam() { return cart.getVideoRam(); };

};

}

#endif
