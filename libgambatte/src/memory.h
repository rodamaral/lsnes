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

	unsigned ff_read(unsigned p, unsigned cc) {
		return p < 0x80 ? nontrivial_ff_read(p, cc) : ioamhram_[p + 0x100];
	}

	unsigned read(unsigned p, unsigned cc) {
		return cart_.rmem(p >> 12) ? cart_.rmem(p >> 12)[p] : nontrivial_read(p, cc);
	}

	void write(unsigned p, unsigned data, unsigned cc) {
		if (cart_.wmem(p >> 12)) {
			cart_.wmem(p >> 12)[p] = data;
		} else
			nontrivial_write(p, data, cc);
	}

	void ff_write(unsigned p, unsigned data, unsigned cc) {
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

private:
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
	unsigned nontrivial_read(unsigned p, unsigned cycleCounter);
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
