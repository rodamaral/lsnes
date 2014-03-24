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
#ifndef CPU_H
#define CPU_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "memory.h"
#include "loadsave.h"

namespace gambatte {

class CPU {
public:
	CPU(time_t (**_getCurrentTime)());
	signed runFor(unsigned cycles);
	void setStatePtrs(SaveState &state);
	void saveState(SaveState &state);
	void loadOrSave(loadsave& state);
	void loadState(SaveState const &state);
	void loadSavedata() { mem_.loadSavedata(); }
	void saveSavedata() { mem_.saveSavedata(); }

	void setVideoBuffer(uint_least32_t *videoBuf, std::ptrdiff_t pitch) {
		mem_.setVideoBuffer(videoBuf, pitch);
	}

	void setInputGetter(InputGetter *getInput) {
		mem_.setInputGetter(getInput);
	}

	void setSaveDir(std::string const &sdir) {
		mem_.setSaveDir(sdir);
	}

	void set_debug_buffer(debugbuffer& dbgbuf)
	{
		mem_.set_debug_buffer(dbgbuf);
	}

	std::string const saveBasePath() const {
		return mem_.saveBasePath();
	}

	void setOsdElement(transfer_ptr<OsdElement> osdElement) {
		mem_.setOsdElement(osdElement);
	}

	LoadRes load(std::string const &romfile, bool forceDmg, bool multicartCompat) {
		return mem_.loadROM(romfile, forceDmg, multicartCompat);
	}

	LoadRes load(const unsigned char* image, size_t isize, bool forceDmg, bool multicartCompat) {
		return mem_.loadROM(image, isize, forceDmg, multicartCompat);
	}

	bool loaded() const { return mem_.loaded(); }
	char const * romTitle() const { return mem_.romTitle(); }
	PakInfo const pakInfo(bool multicartCompat) const { return mem_.pakInfo(multicartCompat); }
	void setSoundBuffer(uint_least32_t *buf) { mem_.setSoundBuffer(buf); }
	std::size_t fillSoundBuffer() { return mem_.fillSoundBuffer(cycleCounter_); }
	bool isCgb() const { return mem_.isCgb(); }

	void setDmgPaletteColor(int palNum, int colorNum, uint_least32_t rgb32) {
		mem_.setDmgPaletteColor(palNum, colorNum, rgb32);
	}

	void setGameGenie(std::string const &codes) { mem_.setGameGenie(codes); }
	void setGameShark(std::string const &codes) { mem_.setGameShark(codes); }

	void setRtcBase(time_t time) { mem_.setRtcBase(time); }
	time_t getRtcBase() { return mem_.getRtcBase(); }
	std::pair<unsigned char*, size_t> getWorkRam() { return mem_.getWorkRam(); }
	std::pair<unsigned char*, size_t> getSaveRam() { return mem_.getSaveRam(); }
	std::pair<unsigned char*, size_t> getIoRam() { return mem_.getIoRam(); }
	std::pair<unsigned char*, size_t> getVideoRam() { return mem_.getVideoRam(); };
        uint8_t bus_read(unsigned addr) { return mem_.read(addr, cycleCounter_, false); }
        void bus_write(unsigned addr, uint8_t val) { mem_.write(addr, val, cycleCounter_); }
	void set_emuflags(unsigned flags) { emuflags = flags; }

	unsigned cycleCounter_;
	unsigned short pc_;
	unsigned short sp;
	unsigned hf1, hf2, zf, cf;
	unsigned char a_, b, c, d, e, /*f,*/ h, l;
	unsigned char* aptr;
	unsigned short* pcptr;
	unsigned* cyclecountptr;
private:
	Memory mem_;
	bool skip_;
	unsigned emuflags;

	void process(unsigned cycles);
};

}

#endif
