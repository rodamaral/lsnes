/***************************************************************************
Copyright (C) 2007 by Nach
http://nsrt.edgeemu.com

Copyright (C) 2007-2011 by Sindre Aamås
sinamas@users.sourceforge.net

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License version 2 for more details.

You should have received a copy of the GNU General Public License
version 2 along with this program; if not, write to the
Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
***************************************************************************/
#ifndef GAMBATTE_FILE_H
#define GAMBATTE_FILE_H


//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.
//
// Modified 2012-07-10 by H. Ilari Liusvaara
//	- New API methods.

#include <memory>
#include "transfer_ptr.h"
#include <string>

namespace gambatte {

class File {
public:
	virtual ~File() {}
	virtual void rewind() = 0;
	virtual std::size_t size() const = 0;
	virtual void read(char *buffer, std::size_t amount) = 0;
	virtual bool fail() const = 0;
};

transfer_ptr<File> newFileInstance(std::string const &filepath);
transfer_ptr<File> newFileInstance(const unsigned char* image, size_t isize);

}

#endif
