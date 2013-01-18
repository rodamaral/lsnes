/***************************************************************************
 *   Copyright (C) 2010 by Sindre Aamås                                    *
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
#ifndef PPU_H
#define PPU_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include <cstring>
#include "video/ly_counter.h"
#include "video/sprite_mapper.h"
#include "gbint.h"
#include "../loadsave.h"

namespace gambatte {

class PPUFrameBuf {
	mutable uint_least32_t buf_[160*144];
	uint_least32_t *fbline_;
	
	static uint_least32_t * nullfbline() { static uint_least32_t nullfbline_[160]; return nullfbline_; }
	
public:
	PPUFrameBuf() : fbline_(nullfbline()) { memset(buf_, 0, sizeof(buf_)); }
	uint_least32_t * fb() const { return buf_; }
	uint_least32_t * fbline() const { return fbline_; }
	int pitch() const { return 160; }
	void setFbline(const unsigned ly) { fbline_ = buf_ ? buf_ + ly * 160ULL : nullfbline(); }
	void blit(uint_least32_t *const buf, const int pitch) const {
		for(unsigned i = 0; i < 144; i++)
			memcpy(buf + i * static_cast<signed>(pitch), buf_ + i * 160, 160 * sizeof(buf[0]));
	}
	void loadOrSave(loadsave& state) {
		state(buf_, 160*144);
		bool var = (fbline_ != nullfbline());
		state(var);
		if(var) {
			unsigned x = fbline_ - buf_;
			state(x);
			fbline_ = buf_ + x;
		} else
			fbline_ = nullfbline();
	}
};

struct PPUState {
	void (*f)(struct PPUPriv &v);
	unsigned (*predictCyclesUntilXpos_f)(const struct PPUPriv &v, int targetxpos, unsigned cycles);
	unsigned char id;
};

// The PPU loop accesses a lot of state at once, so it's difficult to split this up much beyond grouping stuff into smaller structs.
struct PPUPriv {
	uint_least32_t bgPalette[8 * 4];
	uint_least32_t spPalette[8 * 4];

	const unsigned char *vram;
	const PPUState *nextCallPtr;

	unsigned now;
	unsigned lastM0Time;
	signed cycles;

	unsigned tileword;
	unsigned ntileword;

	LyCounter lyCounter;
	SpriteMapper spriteMapper;
	PPUFrameBuf framebuf;
	
	struct Sprite { 
		unsigned char spx, oampos, line, attrib;
		void loadOrSave(loadsave& state) {
			state(spx);
			state(oampos);
			state(line);
			state(attrib);
		}
	} spriteList[11];
	unsigned short spwordList[11];

	unsigned char lcdc;
	unsigned char scy;
	unsigned char scx;
	unsigned char wy;
	unsigned char wy2;
	unsigned char wx;
	unsigned char winDrawState;
	unsigned char wscx;
	unsigned char winYPos;
	unsigned char reg0;
	unsigned char reg1;
	unsigned char attrib;
	unsigned char nattrib;
	unsigned char nextSprite;
	unsigned char currentSprite;
	unsigned char xpos;
	unsigned char endx;

	bool cgb;
	bool weMaster;
	
	PPUPriv(NextM0Time &nextM0Time, const unsigned char *oamram, const unsigned char *vram);
	void loadOrSave(loadsave& state);
};

class PPU {
	PPUPriv p_;
public:
	PPU(NextM0Time &nextM0Time, const unsigned char *oamram, const unsigned char *vram)
	: p_(nextM0Time, oamram, vram)
	{
	}
	
	uint_least32_t * bgPalette() { return p_.bgPalette; }
	bool cgb() const { return p_.cgb; }
	void doLyCountEvent() { p_.lyCounter.doEvent(); }
	unsigned doSpriteMapEvent(unsigned time) { return p_.spriteMapper.doEvent(time); }
	const PPUFrameBuf & frameBuf() const { return p_.framebuf; }
	bool inactivePeriodAfterDisplayEnable(unsigned cc) const { return p_.spriteMapper.inactivePeriodAfterDisplayEnable(cc); }
	unsigned lastM0Time() const { return p_.lastM0Time; }
	unsigned lcdc() const { return p_.lcdc; }
	void loadState(const SaveState &state, const unsigned char *oamram);
	const LyCounter & lyCounter() const { return p_.lyCounter; }
	unsigned now() const { return p_.now; }
	void oamChange(unsigned cc) { p_.spriteMapper.oamChange(cc); }
	void oamChange(const unsigned char *oamram, unsigned cc) { p_.spriteMapper.oamChange(oamram, cc); }
	unsigned predictedNextXposTime(unsigned xpos) const;
	void reset(const unsigned char *oamram, const unsigned char *vram, bool cgb);
	void resetCc(unsigned oldCc, unsigned newCc);
	void saveState(SaveState &ss) const;
	void flipDisplay(uint_least32_t *buf, unsigned pitch) { p_.framebuf.blit(buf, pitch); }
	void setLcdc(unsigned lcdc, unsigned cc);
	void setScx(const unsigned scx) { p_.scx = scx; }
	void setScy(const unsigned scy) { p_.scy = scy; }
	void setStatePtrs(SaveState &ss) { p_.spriteMapper.setStatePtrs(ss); }
	void setWx(const unsigned wx) { p_.wx = wx; }
	void setWy(const unsigned wy) { p_.wy = wy; }
	void updateWy2() { p_.wy2 = p_.wy; }
	void speedChange(unsigned cycleCounter);
	uint_least32_t * spPalette() { return p_.spPalette; }
	void update(unsigned cc);

	void loadOrSave(loadsave& state) {
		p_.loadOrSave(state);
	}
};

}

#endif
