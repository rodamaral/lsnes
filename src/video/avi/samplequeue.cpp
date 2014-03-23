#include "video/avi/samplequeue.hpp"

#define BLOCKSIZE 4096

sample_queue::sample_queue()
{
	rptr = wptr = 0;
	size = 0;
	blank = true;
}

void sample_queue::push(const int16_t* samples, size_t count)
{
	threads::alock h(mlock);
	size_t dsize = _available();
	if(dsize + count > size) {
		//Expand the buffer.
		std::vector<int16_t> newbuffer;
		newbuffer.resize((dsize + count + BLOCKSIZE - 1) / BLOCKSIZE * BLOCKSIZE);
		size_t trptr = rptr;
		for(size_t i = 0; i < dsize; i++) {
			newbuffer[i] = data[trptr++];
			if(trptr == size)
				trptr = 0;
		}
		data.swap(newbuffer);
		size = data.size();
		rptr = 0;
		wptr = dsize;
	}

	while(count) {
		data[wptr++] = *samples;
		if(wptr == size)
			wptr = 0;
		blank = false;
		samples++;
		count--;
	}
}

void sample_queue::pull(int16_t* samples, size_t count)
{
	threads::alock h(mlock);
	while(count) {
		if(!blank) {
			*samples = data[rptr++];
			if(rptr == size)
				rptr = 0;
			if(rptr == wptr)
				blank = true;
		} else
			*samples = 0;
		samples++;
		count--;
	}
}

size_t sample_queue::available()
{
	threads::alock h(mlock);
	return _available();
}

size_t sample_queue::_available()
{
	if(blank)
		return 0;
	else if(rptr < wptr)
		return wptr - rptr;
	else
		return size - (rptr - wptr);
}
