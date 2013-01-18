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
#ifndef TIMA_H
#define TIMA_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "interruptrequester.h"

namespace gambatte {

class TimaInterruptRequester {
	InterruptRequester &intreq;
	
public:
	explicit TimaInterruptRequester(InterruptRequester &intreq) : intreq(intreq) {}
	void flagIrq() const { intreq.flagIrq(4); }
	unsigned nextIrqEventTime() const { return intreq.eventTime(TIMA); }
	void setNextIrqEventTime(const unsigned time) const { intreq.setEventTime<TIMA>(time); }
};

class Tima {
	unsigned lastUpdate_;
	unsigned tmatime_;
	
	unsigned char tima_;
	unsigned char tma_;
	unsigned char tac_;
	
	void updateIrq(const unsigned cc, const TimaInterruptRequester timaIrq) {
		while (cc >= timaIrq.nextIrqEventTime())
			doIrqEvent(timaIrq);
	}
	
	void updateTima(unsigned cc);
	
public:
	Tima();
	void saveState(SaveState &) const;
	void loadState(const SaveState &, TimaInterruptRequester timaIrq);
	void resetCc(unsigned oldCc, unsigned newCc, TimaInterruptRequester timaIrq);
	
	void setTima(unsigned tima, unsigned cc, TimaInterruptRequester timaIrq);
	void setTma(unsigned tma, unsigned cc, TimaInterruptRequester timaIrq);
	void setTac(unsigned tac, unsigned cc, TimaInterruptRequester timaIrq);
	unsigned tima(unsigned cc);
	
	void doIrqEvent(TimaInterruptRequester timaIrq);

	void loadOrSave(loadsave& state);
};

}

#endif
