#include "video/avi/writer.hpp"
#include "core/framerate.hpp"
#include "core/messages.hpp"
#include <sstream>
#include <iomanip>

avi_writer::~avi_writer()
{
	try {
		if(!closed)
			close();
	} catch(...) {
	}
}

std::deque<frame_object>& avi_writer::video_queue()
{
	return vqueue;
}

sample_queue& avi_writer::audio_queue()
{
	return aqueue;
}

void avi_writer::flush()
{
	flush(false);
}

void avi_writer::close()
{
	flush(true);
	aviout.end();
	avifile.close();
	closed = true;
	curwidth = curheight = curfps_n = curfps_d = 0;
}

void avi_writer::flush(bool force)
{
do_again:
	if(vqueue.empty())
		return;
	bool sbreak = false;
	if(closed)
		sbreak = true;		//Start first segment.
	if(aviout.get_size_estimate() > 2100000000)
		sbreak = true;		//Break due to size.
	struct frame_object& f = vqueue.front();
	if(f.force_break)
		sbreak = true;		//Manual force break.
	if(f.width != curwidth || f.height != curheight || f.fps_n != curfps_n || f.fps_d != curfps_d)
		sbreak = true;		//Break due resolution / rate change.
	if(sbreak) {
		if(!closed) {
			aviout.end();
			avifile.close();
			closed = true;
		}
		curwidth = f.width;
		curheight = f.height;
		curfps_n = f.fps_n;
		curfps_d = f.fps_d;
		std::string aviname;
		{
			std::ostringstream x;
			x << prefix << "_" << std::setw(5) << std::setfill('0') << next_segment << ".avi";
			aviname = x.str();
		}
		avifile.open(aviname, std::ios::out | std::ios::binary);
		if(!avifile)
			throw std::runtime_error("Can't open '" + aviname + "'");
		next_segment++;
		aviout.start(avifile, vcodec, acodec, curwidth, curheight, curfps_n, curfps_d, samplerate,
			channels);
		closed = false;
		messages << "Start AVI: " << curwidth << "x" << curheight << "@" << curfps_n << "/" << curfps_d
			<< " to '" << aviname << "'" << std::endl;
	}
	uint64_t t = framerate_regulator::get_utime();
	if(aviout.readqueue(f.data, f.odata, f.stride, aqueue, force)) {
		t = framerate_regulator::get_utime() - t;
		if(t > 20000)
			std::cerr << "aviout.readqueue took " << t << std::endl;
		vqueue.pop_front();
		goto do_again;
	}
}

avi_writer::avi_writer(const std::string& _prefix, struct avi_video_codec& _vcodec, struct avi_audio_codec& _acodec,
	uint32_t _samplerate, uint16_t _audiochannels)
	: vcodec(_vcodec), acodec(_acodec)
{
	prefix = _prefix;
	closed = true;
	next_segment = 0;
	samplerate = _samplerate;
	channels = _audiochannels;
	curwidth = curheight = curfps_n = curfps_d = 0;
}
