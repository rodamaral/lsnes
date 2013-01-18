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
#ifndef SOUND_CHANNEL1_H
#define SOUND_CHANNEL1_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "gbint.h"
#include "master_disabler.h"
#include "length_counter.h"
#include "duty_unit.h"
#include "envelope_unit.h"
#include "static_output_tester.h"
#include "loadsave.h"

namespace gambatte {

struct SaveState;

class Channel1 {
	class SweepUnit : public SoundUnit {
		MasterDisabler &disableMaster;
		DutyUnit &dutyUnit;
		unsigned short shadow;
		unsigned char nr0;
		bool negging;
		
		unsigned calcFreq();
		
	public:
		SweepUnit(MasterDisabler &disabler, DutyUnit &dutyUnit);
		void event();
		void nr0Change(unsigned newNr0);
		void nr4Init(unsigned cycleCounter);
		void reset();
		void saveState(SaveState &state) const;
		void loadState(const SaveState &state);
		void loadOrSave(loadsave& state) {
			loadOrSave2(state);
			state(shadow);
			state(nr0);
			state(negging);
		}
	};
	
	friend class StaticOutputTester<Channel1,DutyUnit>;
	
	StaticOutputTester<Channel1,DutyUnit> staticOutputTest;
	DutyMasterDisabler disableMaster;
	LengthCounter lengthCounter;
	DutyUnit dutyUnit;
	EnvelopeUnit envelopeUnit;
	SweepUnit sweepUnit;
	
	SoundUnit *nextEventUnit;
	
	unsigned cycleCounter;
	unsigned soMask;
	unsigned prevOut;
	
	unsigned char nr4;
	bool master;
	
	void setEvent();
	
public:
	Channel1();
	void setNr0(unsigned data);
	void setNr1(unsigned data);
	void setNr2(unsigned data);
	void setNr3(unsigned data);
	void setNr4(unsigned data);
	
	void setSo(unsigned soMask);
	bool isActive() const { return master; }
	
	void update(uint_least32_t *buf, unsigned soBaseVol, unsigned cycles);
	
	void reset();
	void init(bool cgb);
	void saveState(SaveState &state);
	void loadState(const SaveState &state);

	void loadOrSave(loadsave& state);
};

}

#endif
