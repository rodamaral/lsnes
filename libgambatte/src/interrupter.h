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
#ifndef INTERRUPTER_H
#define INTERRUPTER_H

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include <string>
#include <vector>
#include "loadsave.h"


namespace gambatte {

struct GsCode {
	unsigned short address;
	unsigned char value;
	unsigned char type;

	void loadOrSave(loadsave& state) {
		state(address);
		state(value);
		state(type);
	}
};

class Memory;

class Interrupter {
public:
	Interrupter(unsigned short &sp, unsigned short &pc);
	unsigned interrupt(unsigned address, unsigned cycleCounter, Memory &memory);
	void setGameShark(std::string const &codes);
	void loadOrSave(loadsave& state);

private:
	unsigned short &sp_;
	unsigned short &pc_;
	std::vector<GsCode> gsCodes_;

	void applyVblankCheats(unsigned cc, Memory &mem);
};

}

#endif
