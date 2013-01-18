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
#include "channel4.h"
#include "../savestate.h"
#include <algorithm>

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

static unsigned toPeriod(const unsigned nr3) {
	unsigned s = (nr3 >> 4) + 3;
	unsigned r = nr3 & 7;
	
	if (!r) {
		r = 1;
		--s;
	}
	
	return r << s;
}

namespace gambatte {

Channel4::Lfsr::Lfsr() :
backupCounter(COUNTER_DISABLED),
reg(0xFF),
nr3(0),
master(false)
{}

void Channel4::Lfsr::updateBackupCounter(const unsigned cc) {
	/*if (backupCounter <= cc) {
		const unsigned long period = toPeriod(nr3);
		backupCounter = cc - (cc - backupCounter) % period + period;
	}*/
	
	if (backupCounter <= cc) {
		const unsigned period = toPeriod(nr3);
		unsigned periods = (cc - backupCounter) / period + 1;
		
		backupCounter += periods * period;
		
		if (master && nr3 < 0xE0) {
			if (nr3 & 8) {
				while (periods > 6) {
					const unsigned xored = (reg << 1 ^ reg) & 0x7E;
					reg = (reg >> 6 & ~0x7E) | xored | xored << 8;
					periods -= 6;
				}
				
				const unsigned xored = ((reg ^ reg >> 1) << (7 - periods)) & 0x7F;
				reg = (reg >> periods & ~(0x80 - (0x80 >> periods))) | xored | xored << 8;
			} else {
				while (periods > 15) {
					reg = reg ^ reg >> 1;
					periods -= 15;
				}
				
				reg = reg >> periods | (((reg ^ reg >> 1) << (15 - periods)) & 0x7FFF);
			}
		}
	}
}

void Channel4::Lfsr::reviveCounter(const unsigned cc) {
	updateBackupCounter(cc);
	counter = backupCounter;
}

/*static const unsigned char nextStateDistance[0x40] = {
	6, 1, 1, 2, 2, 1, 1, 3,
	3, 1, 1, 2, 2, 1, 1, 4,
	4, 1, 1, 2, 2, 1, 1, 3,
	3, 1, 1, 2, 2, 1, 1, 5,
	5, 1, 1, 2, 2, 1, 1, 3,
	3, 1, 1, 2, 2, 1, 1, 4,
	4, 1, 1, 2, 2, 1, 1, 3,
	3, 1, 1, 2, 2, 1, 1, 6,
};*/

inline void Channel4::Lfsr::event() {
	if (nr3 < 0xE0) {
		const unsigned shifted = reg >> 1;
		const unsigned xored = (reg ^ shifted) & 1;
		
		reg = shifted | xored << 14;
		
		if (nr3 & 8)
			reg = (reg & ~0x40) | xored << 6;
	}
	
	counter += toPeriod(nr3);
	backupCounter = counter;
	
	
	/*if (nr3 < 0xE0) {
		const unsigned periods = nextStateDistance[reg & 0x3F];
		const unsigned xored = ((reg ^ reg >> 1) << (7 - periods)) & 0x7F;
		
		reg = reg >> periods | xored << 8;
		
		if (nr3 & 8)
			reg = reg & ~(0x80 - (0x80 >> periods)) | xored;
	}
	
	const unsigned long period = toPeriod(nr3);
	backupCounter = counter + period;
	counter += period * nextStateDistance[reg & 0x3F];*/
}

void Channel4::Lfsr::nr3Change(const unsigned newNr3, const unsigned cc) {
	updateBackupCounter(cc);
	nr3 = newNr3;
	
// 	if (counter != COUNTER_DISABLED)
// 		counter = backupCounter + toPeriod(nr3) * (nextStateDistance[reg & 0x3F] - 1);
}

void Channel4::Lfsr::nr4Init(unsigned cc) {
	disableMaster();
	updateBackupCounter(cc);
	master = true;
	backupCounter += 4;
	counter = backupCounter;
// 	counter = backupCounter + toPeriod(nr3) * (nextStateDistance[reg & 0x3F] - 1);
}

void Channel4::Lfsr::reset(const unsigned cc) {
	nr3 = 0;
	disableMaster();
	backupCounter = cc + toPeriod(nr3);
}

void Channel4::Lfsr::resetCounters(const unsigned oldCc) {
	updateBackupCounter(oldCc);
	backupCounter -= COUNTER_MAX;
	SoundUnit::resetCounters(oldCc);
}

void Channel4::Lfsr::saveState(SaveState &state, const unsigned cc) {
	updateBackupCounter(cc);
	state.spu.ch4.lfsr.counter = backupCounter;
	state.spu.ch4.lfsr.reg = reg;
}

void Channel4::Lfsr::loadState(const SaveState &state) {
	counter = backupCounter = std::max(state.spu.ch4.lfsr.counter, state.spu.cycleCounter);
	reg = state.spu.ch4.lfsr.reg;
	master = state.spu.ch4.master;
	nr3 = state.mem.ioamhram.get()[0x122];
}

Channel4::Channel4() :
	staticOutputTest(*this, lfsr),
	disableMaster(master, lfsr),
	lengthCounter(disableMaster, 0x3F),
	envelopeUnit(staticOutputTest),
	cycleCounter(0),
	soMask(0),
	prevOut(0),
	nr4(0),
	master(false)
{
	setEvent();
}

void Channel4::setEvent() {
// 	nextEventUnit = &lfsr;
// 	if (envelopeUnit.getCounter() < nextEventUnit->getCounter())
		nextEventUnit = &envelopeUnit;
	if (lengthCounter.getCounter() < nextEventUnit->getCounter())
		nextEventUnit = &lengthCounter;
}

void Channel4::setNr1(const unsigned data) {
	lengthCounter.nr1Change(data, nr4, cycleCounter);
	
	setEvent();
}

void Channel4::setNr2(const unsigned data) {
	if (envelopeUnit.nr2Change(data))
		disableMaster();
	else
		staticOutputTest(cycleCounter);
	
	setEvent();
}

void Channel4::setNr4(const unsigned data) {
	lengthCounter.nr4Change(nr4, data, cycleCounter);
		
	nr4 = data;
	
	if (data & 0x80) { //init-bit
		nr4 &= 0x7F;
		
		master = !envelopeUnit.nr4Init(cycleCounter);
		
		if (master)
			lfsr.nr4Init(cycleCounter);
		
		staticOutputTest(cycleCounter);
	}
	
	setEvent();
}

void Channel4::setSo(const unsigned soMask) {
	this->soMask = soMask;
	staticOutputTest(cycleCounter);
	setEvent();
}

void Channel4::reset() {
	cycleCounter = 0x1000 | (cycleCounter & 0xFFF); // cycleCounter >> 12 & 7 represents the frame sequencer position.

// 	lengthCounter.reset();
	lfsr.reset(cycleCounter);
	envelopeUnit.reset();
	
	setEvent();
}

void Channel4::init(const bool cgb) {
	lengthCounter.init(cgb);
}

void Channel4::saveState(SaveState &state) {
	lfsr.saveState(state, cycleCounter);
	envelopeUnit.saveState(state.spu.ch4.env);
	lengthCounter.saveState(state.spu.ch4.lcounter);
	
	state.spu.ch4.nr4 = nr4;
	state.spu.ch4.master = master;
}

void Channel4::loadState(const SaveState &state) {
	lfsr.loadState(state);
	envelopeUnit.loadState(state.spu.ch4.env, state.mem.ioamhram.get()[0x121], state.spu.cycleCounter);
	lengthCounter.loadState(state.spu.ch4.lcounter, state.spu.cycleCounter);
	
	cycleCounter = state.spu.cycleCounter;
	nr4 = state.spu.ch4.nr4;
	master = state.spu.ch4.master;
}

void Channel4::update(uint_least32_t *buf, const unsigned soBaseVol, unsigned cycles) {
	const unsigned outBase = envelopeUnit.dacIsOn() ? soBaseVol & soMask : 0;
	const unsigned outLow = outBase * (0 - 15ul);
	const unsigned endCycles = cycleCounter + cycles;
	
	for (;;) {
		const unsigned outHigh = /*master ? */outBase * (envelopeUnit.getVolume() * 2 - 15ul)/* : outLow*/;
		const unsigned nextMajorEvent = nextEventUnit->getCounter() < endCycles ? nextEventUnit->getCounter() : endCycles;
		unsigned out = lfsr.isHighState() ? outHigh : outLow;
		
		while (lfsr.getCounter() <= nextMajorEvent) {
			*buf += out - prevOut;
			prevOut = out;
			buf += lfsr.getCounter() - cycleCounter;
			cycleCounter = lfsr.getCounter();
			
			lfsr.event();
			out = lfsr.isHighState() ? outHigh : outLow;
		}
		
		if (cycleCounter < nextMajorEvent) {
			*buf += out - prevOut;
			prevOut = out;
			buf += nextMajorEvent - cycleCounter;
			cycleCounter = nextMajorEvent;
		}
		
		if (nextEventUnit->getCounter() == nextMajorEvent) {
			nextEventUnit->event();
			setEvent();
		} else
			break;
	}
	
	if (cycleCounter & SoundUnit::COUNTER_MAX) {
		lengthCounter.resetCounters(cycleCounter);
		lfsr.resetCounters(cycleCounter);
		envelopeUnit.resetCounters(cycleCounter);
		
		cycleCounter -= SoundUnit::COUNTER_MAX;
	}
}

void Channel4::loadOrSave(loadsave& state) {
	//DisableMaster has no state.
	lengthCounter.loadOrSave(state);
	envelopeUnit.loadOrSave(state);
	lfsr.loadOrSave(state);

	state.startEnumeration();
	state.enumerate<SoundUnit*>(nextEventUnit, NULL, 0);
	state.enumerate<SoundUnit*>(nextEventUnit, &lengthCounter, 1);
	state.enumerate<SoundUnit*>(nextEventUnit, &envelopeUnit, 2);
	state.endEnumeration();

	state(cycleCounter);
	state(soMask);
	state(prevOut);
	state(nr4);
	state(master);
}

}
