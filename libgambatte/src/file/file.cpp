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
#include "stdfile.h"
#include <cstring>

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

transfer_ptr<gambatte::File> gambatte::newFileInstance(std::string const &filepath) {
	return transfer_ptr<File>(new StdFile(filepath.c_str()));
}

namespace
{
	struct MemoryFile : public gambatte::File
	{
		MemoryFile(const unsigned char* image, size_t isize) : buf(image), bufsize(isize),
			ptr(0), xfail(false) {}
		~MemoryFile() {}
		void rewind() { ptr = 0; xfail = false; }
		std::size_t size() const { return bufsize; }
		void read(char *buffer, std::size_t amount) {
			if(amount > bufsize - ptr) {
				memcpy(buffer, buf, bufsize - ptr);
				xfail = true;
			} else
				memcpy(buffer, buf, amount);
		}
		bool fail() const { return xfail; }
	private:
		const unsigned char* buf;
		size_t bufsize;
		size_t ptr;
		bool xfail;
	};
}

transfer_ptr<gambatte::File> gambatte::newFileInstance(const unsigned char* image, size_t isize) {
	return transfer_ptr<File>(new MemoryFile(image, isize));
}
