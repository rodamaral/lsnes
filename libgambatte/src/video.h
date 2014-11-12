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
#ifndef VIDEO_H
#define VIDEO_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.
//
// Modified 2014-10-22 by H. Ilari Liusvaara
//	- Add extra callbacks.

#include "video/ppu.h"
#include "video/lyc_irq.h"
#include "video/next_m0_time.h"
#include "interruptrequester.h"
#include "extracallbacks.h"
#include "minkeeper.h"
#include "osd_element.h"
#include "scoped_ptr.h"
#include "video/lyc_irq.h"
#include "video/m0_irq.h"
#include "video/next_m0_time.h"
#include "video/ppu.h"

namespace gambatte {

class VideoInterruptRequester {
public:
	explicit VideoInterruptRequester(InterruptRequester &intreq)
	: intreq_(intreq)
	{
	}

	void flagHdmaReq() const { gambatte::flagHdmaReq(intreq_); }
	void flagIrq(unsigned bit) const { intreq_.flagIrq(bit); }
	void setNextEventTime(unsigned time) const { intreq_.setEventTime<intevent_video>(time); }

private:
	InterruptRequester &intreq_;
};

class LCD {
public:
	LCD(unsigned char const *oamram, unsigned char const *vram,
	    VideoInterruptRequester memEventRequester, const extra_callbacks*& callbacks);
	void reset(unsigned char const *oamram, unsigned char const *vram, bool cgb);
	void setStatePtrs(SaveState &state);
	void saveState(SaveState &state) const;
	void loadOrSave(loadsave& state);
	void loadState(SaveState const &state, unsigned char const *oamram);
	void setDmgPaletteColor(unsigned palNum, unsigned colorNum, uint_least32_t rgb32);
	void setOsdElement(transfer_ptr<OsdElement> osdElement) { osdElement_ = osdElement; }

	void dmgBgPaletteChange(unsigned data, unsigned cycleCounter) {
		update(cycleCounter);
		bgpData_[0] = data;
		setDmgPalette(ppu_.bgPalette(), dmgColorsRgb32_, data);
	}

	void dmgSpPalette1Change(unsigned data, unsigned cycleCounter) {
		update(cycleCounter);
		objpData_[0] = data;
		setDmgPalette(ppu_.spPalette(), dmgColorsRgb32_ + 4, data);
	}

	void dmgSpPalette2Change(unsigned data, unsigned cycleCounter) {
		update(cycleCounter);
		objpData_[1] = data;
		setDmgPalette(ppu_.spPalette() + 4, dmgColorsRgb32_ + 8, data);
	}

	void cgbBgColorChange(unsigned index, unsigned data, unsigned cycleCounter) {
		if (bgpData_[index] != data)
			doCgbBgColorChange(index, data, cycleCounter);
	}

	void cgbSpColorChange(unsigned index, unsigned data, unsigned cycleCounter) {
		if (objpData_[index] != data)
			doCgbSpColorChange(index, data, cycleCounter);
	}

	unsigned cgbBgColorRead(unsigned index, unsigned cycleCounter) {
		return ppu_.cgb() & cgbpAccessible(cycleCounter) ? bgpData_[index] : 0xFF;
	}

	unsigned cgbSpColorRead(unsigned index, unsigned cycleCounter) {
		return ppu_.cgb() & cgbpAccessible(cycleCounter) ? objpData_[index] : 0xFF;
	}

	void updateScreen(bool blanklcd, unsigned cc, uint_least32_t* vbuffer, unsigned vpitch);
	void resetCc(unsigned oldCC, unsigned newCc);
	void speedChange(unsigned cycleCounter);
	bool vramAccessible(unsigned cycleCounter);
	bool oamReadable(unsigned cycleCounter);
	bool oamWritable(unsigned cycleCounter);
	void wxChange(unsigned newValue, unsigned cycleCounter);
	void wyChange(unsigned newValue, unsigned cycleCounter);
	void oamChange(unsigned cycleCounter);
	void oamChange(const unsigned char *oamram, unsigned cycleCounter);
	void scxChange(unsigned newScx, unsigned cycleCounter);
	void scyChange(unsigned newValue, unsigned cycleCounter);
	void vramChange(unsigned cycleCounter) { update(cycleCounter); }
	unsigned getStat(unsigned lycReg, unsigned cycleCounter);

	unsigned getLyReg(unsigned const cc) {
		unsigned lyReg = 0;

		if (ppu_.lcdc() & lcdc_en) {
			if (cc >= ppu_.lyCounter().time())
				update(cc);

			lyReg = ppu_.lyCounter().ly();

			if (lyReg == 153) {
				if (isDoubleSpeed()) {
					if (ppu_.lyCounter().time() - cc <= 456 * 2 - 8)
						lyReg = 0;
				} else
					lyReg = 0;
			} else if (ppu_.lyCounter().time() - cc <= 4)
				++lyReg;
		}

		return lyReg;
	}

	unsigned nextMode1IrqTime() const { return eventTimes_(memevent_m1irq); }
	void lcdcChange(unsigned data, unsigned cycleCounter);
	void lcdstatChange(unsigned data, unsigned cycleCounter);
	void lycRegChange(unsigned data, unsigned cycleCounter);
	void enableHdma(unsigned cycleCounter);
	void disableHdma(unsigned cycleCounter);
	bool hdmaIsEnabled() const { return eventTimes_(memevent_hdma) != disabled_time; }
	void update(unsigned cycleCounter);
	bool isCgb() const { return ppu_.cgb(); }
	bool isDoubleSpeed() const { return ppu_.lyCounter().isDoubleSpeed(); }

private:
	enum Event { event_mem,
	             event_ly, event_last = event_ly };

	enum MemEvent { memevent_oneshot_statirq,
	                memevent_oneshot_updatewy2,
	                memevent_m1irq,
	                memevent_lycirq,
	                memevent_spritemap,
	                memevent_hdma,
	                memevent_m2irq,
	                memevent_m0irq, memevent_last = memevent_m0irq };

	enum { num_events = event_last + 1 };
	enum { num_memevents = memevent_last + 1 };

	class EventTimes {
	public:
		explicit EventTimes(VideoInterruptRequester memEventRequester)
		: memEventRequester_(memEventRequester)
		{
		}

		Event nextEvent() const { return static_cast<Event>(eventMin_.min()); }
		unsigned nextEventTime() const { return eventMin_.minValue(); }
		unsigned operator()(Event e) const { return eventMin_.value(e); }
		template<Event e> void set(unsigned time) { eventMin_.setValue<e>(time); }
		void set(Event e, unsigned time) { eventMin_.setValue(e, time); }

		MemEvent nextMemEvent() const { return static_cast<MemEvent>(memEventMin_.min()); }
		unsigned nextMemEventTime() const { return memEventMin_.minValue(); }
		unsigned operator()(MemEvent e) const { return memEventMin_.value(e); }

		template<MemEvent e>
		void setm(unsigned time) { memEventMin_.setValue<e>(time); setMemEvent(); }
		void set(MemEvent e, unsigned time) { memEventMin_.setValue(e, time); setMemEvent(); }

		void flagIrq(unsigned bit) { memEventRequester_.flagIrq(bit); }
		void flagHdmaReq() { memEventRequester_.flagHdmaReq(); }

		void loadOrSave(loadsave& state) {
			eventMin_.loadOrSave(state);
			memEventMin_.loadOrSave(state);
		}
	private:
		MinKeeper<num_events> eventMin_;
		MinKeeper<num_memevents> memEventMin_;
		VideoInterruptRequester memEventRequester_;

		void setMemEvent() {
			unsigned nmet = nextMemEventTime();
			eventMin_.setValue<event_mem>(nmet);
			memEventRequester_.setNextEventTime(nmet);
		}

	};

	PPU ppu_;
	uint_least32_t dmgColorsRgb32_[3 * 4];
	unsigned char  bgpData_[8 * 8];
	unsigned char objpData_[8 * 8];
	EventTimes eventTimes_;
	M0Irq m0Irq_;
	LycIrq lycIrq_;
	NextM0Time nextM0Time_;
	scoped_ptr<OsdElement> osdElement_;
	unsigned char statReg_;
	unsigned char m2IrqStatReg_;
	unsigned char m1IrqStatReg_;
	const extra_callbacks*& callbacks_;

	static void setDmgPalette(uint_least32_t palette[],
	                          uint_least32_t const dmgColors[],
	                          unsigned data);
	void setDmgPaletteColor(unsigned index, uint_least32_t rgb32);
	void refreshPalettes();
	void setDBuffer();
	void doMode2IrqEvent();
	void event();
	unsigned m0TimeOfCurrentLine(unsigned cc);
	bool cgbpAccessible(unsigned cycleCounter);
	bool lycRegChangeStatTriggerBlockedByM0OrM1Irq(unsigned cc);
	bool lycRegChangeTriggersStatIrq(unsigned old, unsigned data, unsigned cc);
	bool statChangeTriggersM0LycOrM1StatIrqCgb(unsigned old, unsigned data, unsigned cc);
	bool statChangeTriggersStatIrqCgb(unsigned old, unsigned data, unsigned cc);
	bool statChangeTriggersStatIrqDmg(unsigned old, unsigned cc);
	bool statChangeTriggersStatIrq(unsigned old, unsigned data, unsigned cc);
	void mode3CyclesChange();
	void doCgbBgColorChange(unsigned index, unsigned data, unsigned cycleCounter);
	void doCgbSpColorChange(unsigned index, unsigned data, unsigned cycleCounter);
};

}

#endif
