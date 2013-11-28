/***************************************************************************
 *   Copyright (C) 2007-2010 by Sindre Aamås                               *
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
#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "loadres.h"
#include "memptrs.h"
#include "rtc.h"
#include "savestate.h"
#include "scoped_ptr.h"
#include <string>
#include <vector>
#include "../loadsave.h"

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

namespace gambatte {

class File;

class Mbc {
public:
	virtual ~Mbc() {}
	virtual void romWrite(unsigned P, unsigned data) = 0;
	virtual void saveState(SaveState::Mem &ss) const = 0;
	virtual void loadState(SaveState::Mem const &ss) = 0;
	virtual bool isAddressWithinAreaRombankCanBeMappedTo(unsigned address, unsigned rombank) const = 0;
	virtual void loadOrSave(loadsave& state) = 0;
};

class Cartridge {
public:
	Cartridge(time_t (**_getCurrentTime)());
	void setStatePtrs(SaveState &);
	void saveState(SaveState &) const;
	void loadState(SaveState const &);
	bool loaded() const { return mbc_.get(); }
	OamDmaSrc oamDmaSrc() const { return memptrs_.oamDmaSrc(); }
	void setVrambank(unsigned bank) { memptrs_.setVrambank(bank); }
	void setWrambank(unsigned bank) { memptrs_.setWrambank(bank); }
	void setOamDmaSrc(OamDmaSrc oamDmaSrc) { memptrs_.setOamDmaSrc(oamDmaSrc); }
	void mbcWrite(unsigned addr, unsigned data) { mbc_->romWrite(addr, data); }
	bool isCgb() const { return gambatte::isCgb(memptrs_); }
	void rtcWrite(unsigned data) { rtc_.write(data); }
	unsigned char rtcRead() const { return *rtc_.activeData(); }
	void loadSavedata();
	void saveSavedata();
	std::string const saveBasePath() const;
	void setSaveDir(std::string const &dir);
	char const * romTitle() const { return reinterpret_cast<char const *>(memptrs_.romdata() + 0x134); }
	class PakInfo const pakInfo(bool multicartCompat) const;
	void setGameGenie(std::string const &codes);
	const unsigned char * rmem(unsigned area) const { return memptrs_.rmem(area); }
	unsigned char * wmem(unsigned area) const { return memptrs_.wmem(area); }
	unsigned char * vramdata() const { return memptrs_.vramdata(); }
	unsigned char * romdata(unsigned area) const { return memptrs_.romdata(area); }
	unsigned char * wramdata(unsigned area) const { return memptrs_.wramdata(area); }
	const unsigned char * rdisabledRam() const { return memptrs_.rdisabledRam(); }
	const unsigned char * rsrambankptr() const { return memptrs_.rsrambankptr(); }
	unsigned char * wsrambankptr() const { return memptrs_.wsrambankptr(); }
	unsigned char * vrambankptr() const { return memptrs_.vrambankptr(); }

	void loadOrSave(loadsave& state);
	void setRtcBase(time_t time) { rtc_.setBaseTime(time); }
	time_t getRtcBase() { return rtc_.getBaseTime(); }
	std::pair<unsigned char*, size_t> getCartRom();
	std::pair<unsigned char*, size_t> getWorkRam();
	std::pair<unsigned char*, size_t> getSaveRam();
	std::pair<unsigned char*, size_t> getVideoRam();
	LoadRes loadROM(const std::string &romfile, bool forceDmg, bool multicartCompat);
	LoadRes loadROM(const unsigned char* image, size_t isize, bool forceDmg, bool multicartCompat);
	LoadRes loadROM(File* rom, const bool forceDmg, const bool multicartCompat, const std::string& filename);
	void clearMemorySavedData();

private:
	struct AddrData {
		unsigned addr;
		unsigned char data;
		AddrData(unsigned addr, unsigned data) : addr(addr), data(data) {}
		AddrData() {}
		void loadOrSave(loadsave& state) {
			state(addr);
			state(data);
		}
	};

	MemPtrs memptrs_;
	Rtc rtc_;
	scoped_ptr<Mbc> mbc_;
	std::string defaultSaveBasePath_;
	std::string saveDir_;
	std::vector<AddrData> ggUndoList_;
	bool memoryCartridge;
	time_t memoryCartridgeRtcBase;
	std::vector<unsigned char> memoryCartridgeSram;

	void applyGameGenie(std::string const &code);
};

}

#endif
