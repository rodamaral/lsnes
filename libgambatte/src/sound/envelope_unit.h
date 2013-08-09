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
#ifndef ENVELOPE_UNIT_H
#define ENVELOPE_UNIT_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "sound_unit.h"
#include "../savestate.h"
#include "../loadsave.h"

namespace gambatte {

class EnvelopeUnit : public SoundUnit {
public:
	struct VolOnOffEvent {
		virtual ~VolOnOffEvent() {}
		virtual void operator()(unsigned /*cc*/) {}
	};

	explicit EnvelopeUnit(VolOnOffEvent &volOnOffEvent = nullEvent_);
	void event();
	bool dacIsOn() const { return nr2_ & 0xF8; }
	unsigned getVolume() const { return volume_; }
	bool nr2Change(unsigned newNr2);
	bool nr4Init(unsigned cycleCounter);
	void reset();
	void saveState(SaveState::SPU::Env &estate) const;
	void loadOrSave(loadsave& state);
	void loadState(SaveState::SPU::Env const &estate, unsigned nr2, unsigned cc);

private:
	static VolOnOffEvent nullEvent_;
	VolOnOffEvent &volOnOffEvent_;
	unsigned char nr2_;
	unsigned char volume_;
};

}

#endif
