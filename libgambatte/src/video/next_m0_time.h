#ifndef NEXT_M0_TIME_H_
#define NEXT_M0_TIME_H_

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

#include "../loadsave.h"

namespace gambatte {

class NextM0Time {
public:
	NextM0Time() : predictedNextM0Time_(0) {}
	void predictNextM0Time(class PPU const &v);
	void invalidatePredictedNextM0Time() { predictedNextM0Time_ = 0; }
	unsigned predictedNextM0Time() const { return predictedNextM0Time_; }

	void loadOrSave(loadsave& state) {
		state(predictedNextM0Time_);
	}
private:
	unsigned predictedNextM0Time_;
};

}

#endif
