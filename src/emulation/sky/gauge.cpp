#include "gauge.hpp"
#include "util.hpp"
#include "library/string.hpp"

namespace sky
{
	gauge::gauge()
	{
		dummyimage[0] = dummyimage[1] = 0;
	}

	gauge::gauge(const std::vector<char>& dispdat, size_t cells)
	{
		dummyimage[0] = dummyimage[1] = 0;
		if(dispdat.size() < 2 * cells)
			(stringfmt() << "Entry pointer table incomplete").throwex();
		data.resize(dispdat.size() - 2 * cells);
		memcpy(&data[0], &dispdat[2 * cells], data.size());
		for(unsigned i = 0; i < cells; i++)
			unpack_image(dispdat, i, cells);
	}

	void gauge::unpack_image(const std::vector<char>& dispdat, size_t i, size_t total)
	{
		size_t offset = combine(dispdat[2 * i + 0], dispdat[2 * i + 1]);
		//Check that the offset is in-range.
		if(data.size() < offset)
			(stringfmt() << "Entry " << i << " points outside file").throwex();
		if(data.size() < offset + 4)
			(stringfmt() << "Entry " << i << " header incomplete").throwex();
		size_t imagesize = (uint16_t)data[offset + 2] * data[offset + 3];
		if(data.size() < offset + 4 + imagesize)
			(stringfmt() << "Entry " << i << " bitmap incomplete").throwex();
		ptr.push_back(offset);
	}
}
