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
public:
	explicit TimaInterruptRequester(InterruptRequester &intreq) : intreq_(intreq) {}
	void flagIrq() const { intreq_.flagIrq(4); }
	unsigned nextIrqEventTime() const { return intreq_.eventTime(intevent_tima); }
	void setNextIrqEventTime(unsigned time) const { intreq_.setEventTime<intevent_tima>(time); }

private:
	InterruptRequester &intreq_;
};

class Tima {
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
	void updateTima(unsigned cc);
	void loadOrSave(loadsave& state);
private:
	unsigned lastUpdate_;
	unsigned tmatime_;
	unsigned char tima_;
	unsigned char tma_;
	unsigned char tac_;

	void updateIrq(unsigned const cc, TimaInterruptRequester timaIrq) {
		while (cc >= timaIrq.nextIrqEventTime())
			doIrqEvent(timaIrq);
	}

};

}

#endif
