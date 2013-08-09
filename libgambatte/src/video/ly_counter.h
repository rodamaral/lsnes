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
#ifndef LY_COUNTER_H
#define LY_COUNTER_H
#include "../loadsave.h"

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

namespace gambatte {

struct SaveState;

class LyCounter {
public:
	LyCounter();
	void doEvent();
	bool isDoubleSpeed() const { return ds_; }

	unsigned frameCycles(unsigned cc) const {
		return ly_ * 456ul + lineCycles(cc);
	}

	unsigned lineCycles(unsigned cc) const {
		return 456u - ((time_ - cc) >> isDoubleSpeed());
	}

	unsigned lineTime() const { return lineTime_; }
	unsigned ly() const { return ly_; }

	void loadOrSave(loadsave& state) {
		state(time_);
		state(lineTime_);
		state(ly_);
		state(ds_);
	}
	unsigned nextLineCycle(unsigned lineCycle, unsigned cycleCounter) const;
	unsigned nextFrameCycle(unsigned frameCycle, unsigned cycleCounter) const;
	void reset(unsigned videoCycles, unsigned lastUpdate);
	void setDoubleSpeed(bool ds);
	unsigned time() const { return time_; }

private:
	unsigned time_;
	unsigned short lineTime_;
	unsigned char ly_;
	bool ds_;
};

}

#endif
