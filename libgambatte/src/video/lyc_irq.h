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
#ifndef VIDEO_LYC_IRQ_H
#define VIDEO_LYC_IRQ_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "../loadsave.h"

namespace gambatte {

struct SaveState;
class LyCounter;

class LycIrq {
public:
	LycIrq();
	void doEvent(unsigned char *ifreg, LyCounter const &lyCounter);
	unsigned lycReg() const { return lycRegSrc_; }
	void loadState(SaveState const &state);
	void saveState(SaveState &state) const;
	void loadOrSave(loadsave& state) {
		state(time_);
		state(lycRegSrc_);
		state(statRegSrc_);
		state(lycReg_);
		state(statReg_);
		state(cgb_);
	}

	unsigned time() const { return time_; }
	void setCgb(bool cgb) { cgb_ = cgb; }
	void lcdReset();
	void reschedule(LyCounter const &lyCounter, unsigned cc);

	void statRegChange(unsigned statReg, LyCounter const &lyCounter, unsigned cc) {
		regChange(statReg, lycRegSrc_, lyCounter, cc);
	}

	void lycRegChange(unsigned lycReg, LyCounter const &lyCounter, unsigned cc) {
		regChange(statRegSrc_, lycReg, lyCounter, cc);
	}

private:
	unsigned time_;
 	unsigned char lycRegSrc_;
 	unsigned char statRegSrc_;
	unsigned char lycReg_;
	unsigned char statReg_;
	bool cgb_;

	void regChange(unsigned statReg, unsigned lycReg,
	               LyCounter const &lyCounter, unsigned cc);
};

}

#endif
