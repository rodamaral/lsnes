#include "movie.hpp"
#include <iostream>

void truncate_past_complete_frame()
{
	short in1, in2;
	std::cerr << "Test #1: Truncate past complete frame" << std::endl;
	movie m;
	std::vector<uint8_t> state;
	controls_t c;
	m.readonly_mode(false);
	m.next_frame();
	c(4) = 0x1;
	c(5) = 0x2;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x1) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (1)" << std::endl;
		std::cerr << "Expected 1, got " << in1 << "." << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x2) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (1)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x3;
	c(5) = 0x4;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x3) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (2)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x4) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (2)" << std::endl;
		return;
	}
	c(4) = 0x5;
	c(5) = 0x6;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x5) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (3)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x6) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (3)" << std::endl;
		return;
	}
	m.next_frame();
	state = m.save_state();
	c(4) = 0x7;
	c(5) = 0x8;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x7) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (4)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x8) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (4)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x9;
	c(5) = 0xa;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x9) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (5)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0xa) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (5)" << std::endl;
		return;
	}
	m.restore_state(state, false);
	std::vector<controls_t> v = m.save();
	if(v.size() != 3) {
		std::cerr << "FAIL: Unexpected size for movie" << std::endl;
		return;
	}
	if(v[0](0) != 1 || v[0](4) != 0x1 || v[0](5) != 0x2) {
		std::cerr << "FAIL: Wrong input for first frame" << std::endl;
		return;
	}
	if(v[1](0) != 1 || v[1](4) != 0x3 || v[1](5) != 0x4) {
		std::cerr << "FAIL: Wrong input for second frame first subframe" << std::endl;
		return;
	}
	if(v[2](0) != 0 || v[2](4) != 0x5 || v[2](5) != 0x6) {
		std::cerr << "FAIL: Wrong input for second frame second subframe" << std::endl;
		return;
	}
	std::cerr << "PASS!" << std::endl;
}

void truncate_past_incomplete_frame()
{
	short in1, in2;
	std::cerr << "Test #2: Truncate past incomplete frame" << std::endl;
	movie m;
	std::vector<uint8_t> state;
	controls_t c;
	m.readonly_mode(false);
	m.next_frame();
	c(4) = 0x1;
	c(5) = 0x2;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x1) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (1)" << std::endl;
		std::cerr << "Expected 1, got " << in1 << "." << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x2) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (1)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x3;
	c(5) = 0x4;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x3) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (2)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x4) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (2)" << std::endl;
		return;
	}
	c(4) = 0x5;
	c(5) = 0x6;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x5) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (3)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x6) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (3)" << std::endl;
		return;
	}
	c(5) = 0x7;
	c(6) = 0x8;
	m.set_controls(c);
	in1 = m.next_input(5);
	if(in1 != 0x7) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (4)" << std::endl;
		return;
	}
	in2 = m.next_input(6);
	if(in2 != 0x8) {
		std::cerr << "FAIL: Unexpected return for m.next_input(6) (4)" << std::endl;
		return;
	}
	state = m.save_state();
	//Now we have 2 subframes on 4, 3 on 5 and 1 on 6. Add 1 subframe for 4, 5 and 7.
	c(4) = 0x9;
	c(5) = 0xa;
	c(7) = 0xb;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x9) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (5)" << std::endl;
		return;
	}
	in1 = m.next_input(5);
	if(in1 != 0xa) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (5)" << std::endl;
		return;
	}
	in2 = m.next_input(7);
	if(in2 != 0xb) {
		std::cerr << "FAIL: Unexpected return for m.next_input(7) (5)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0xc;
	c(5) = 0xd;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0xc) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (6)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0xd) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (6)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0xe;
	c(5) = 0xf;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0xe) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (7)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0xf) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (7)" << std::endl;
		return;
	}
	m.restore_state(state, false);
	std::vector<controls_t> v = m.save();
	if(v.size() != 4) {
		std::cerr << "FAIL: Unexpected size for movie" << std::endl;
		return;
	}
	if(v[0](0) != 1 || v[0](4) != 0x1 || v[0](5) != 0x2) {
		std::cerr << "FAIL: Wrong input for first frame" << std::endl;
		return;
	}
	if(v[1](0) != 1 || v[1](4) != 0x3 || v[1](5) != 0x4 || v[1](6) != 0x8 || v[1](7) != 0xb) {
		std::cerr << "FAIL: Wrong input for second frame first subframe" << std::endl;
		return;
	}
	if(v[2](0) != 0 || v[2](4) != 0x5 || v[2](5) != 0x6 || v[2](6) != 0x8 || v[2](7) != 0xb) {
		std::cerr << "FAIL: Wrong input for second frame second subframe" << std::endl;
		std::cerr << v[2](0) << " " << v[2](4) << " " << v[2](5) << " " << v[2](6) << " " << v[2](7) << std::endl;
		return;
	}
	if(v[3](0) != 0 || v[3](4) != 0x5 || v[3](5) != 0x7 || v[3](6) != 0x8 || v[3](7) != 0xb) {
		std::cerr << "FAIL: Wrong input for second frame third subframe" << std::endl;
		return;
	}
	std::cerr << "PASS!" << std::endl;
}

void truncate_current_complete_frame()
{
	short in1, in2;
	std::cerr << "Test #3: Truncate current complete frame" << std::endl;
	movie m;
	std::vector<uint8_t> state;
	controls_t c;
	m.readonly_mode(false);
	m.next_frame();
	c(4) = 0x1;
	c(5) = 0x2;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x1) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (1)" << std::endl;
		std::cerr << "Expected 1, got " << in1 << "." << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x2) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (1)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x3;
	c(5) = 0x4;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x3) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (2)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x4) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (2)" << std::endl;
		return;
	}
	c(4) = 0x5;
	c(5) = 0x6;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x5) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (3)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x6) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (3)" << std::endl;
		return;
	}
	m.next_frame();
	state = m.save_state();
	c(4) = 0x7;
	c(5) = 0x8;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x7) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (4)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x8) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (4)" << std::endl;
		return;
	}
	m.restore_state(state, false);
	std::vector<controls_t> v = m.save();
	if(v.size() != 3) {
		std::cerr << "FAIL: Unexpected size for movie" << std::endl;
		return;
	}
	if(v[0](0) != 1 || v[0](4) != 0x1 || v[0](5) != 0x2) {
		std::cerr << "FAIL: Wrong input for first frame" << std::endl;
		return;
	}
	if(v[1](0) != 1 || v[1](4) != 0x3 || v[1](5) != 0x4) {
		std::cerr << "FAIL: Wrong input for second frame first subframe" << std::endl;
		return;
	}
	if(v[2](0) != 0 || v[2](4) != 0x5 || v[2](5) != 0x6) {
		std::cerr << "FAIL: Wrong input for second frame second subframe" << std::endl;
		return;
	}
	std::cerr << "PASS!" << std::endl;
}

void truncate_current_incomplete_frame()
{
	short in1, in2;
	std::cerr << "Test #4: Truncate current incomplete frame" << std::endl;
	movie m;
	std::vector<uint8_t> state;
	controls_t c;
	m.readonly_mode(false);
	m.next_frame();
	c(4) = 0x1;
	c(5) = 0x2;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x1) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (1)" << std::endl;
		std::cerr << "Expected 1, got " << in1 << "." << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x2) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (1)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x3;
	c(5) = 0x4;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x3) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (2)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x4) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (2)" << std::endl;
		return;
	}
	c(4) = 0x5;
	c(5) = 0x6;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x5) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (3)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x6) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (3)" << std::endl;
		return;
	}
	c(5) = 0x7;
	c(6) = 0x8;
	m.set_controls(c);
	in1 = m.next_input(5);
	if(in1 != 0x7) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (4)" << std::endl;
		return;
	}
	in2 = m.next_input(6);
	if(in2 != 0x8) {
		std::cerr << "FAIL: Unexpected return for m.next_input(6) (4)" << std::endl;
		return;
	}
	state = m.save_state();
	//Now we have 2 subframes on 4, 3 on 5 and 1 on 6. Add 1 subframe for 4, 5 and 7.
	c(4) = 0x9;
	c(5) = 0xa;
	c(7) = 0xb;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x9) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (5)" << std::endl;
		return;
	}
	in1 = m.next_input(5);
	if(in1 != 0xa) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (5)" << std::endl;
		return;
	}
	in2 = m.next_input(7);
	if(in2 != 0xb) {
		std::cerr << "FAIL: Unexpected return for m.next_input(7) (5)" << std::endl;
		return;
	}
	m.restore_state(state, false);
	std::vector<controls_t> v = m.save();
	if(v.size() != 4) {
		std::cerr << "FAIL: Unexpected size for movie" << std::endl;
		return;
	}
	if(v[0](0) != 1 || v[0](4) != 0x1 || v[0](5) != 0x2) {
		std::cerr << "FAIL: Wrong input for first frame" << std::endl;
		return;
	}
	if(v[1](0) != 1 || v[1](4) != 0x3 || v[1](5) != 0x4 || v[1](6) != 0x8 || v[1](7) != 0xb) {
		std::cerr << "FAIL: Wrong input for second frame first subframe" << std::endl;
		return;
	}
	if(v[2](0) != 0 || v[2](4) != 0x5 || v[2](5) != 0x6 || v[2](6) != 0x8 || v[2](7) != 0xb) {
		std::cerr << "FAIL: Wrong input for second frame second subframe" << std::endl;
		std::cerr << v[2](0) << " " << v[2](4) << " " << v[2](5) << " " << v[2](6) << " " << v[2](7) << std::endl;
		return;
	}
	if(v[3](0) != 0 || v[3](4) != 0x5 || v[3](5) != 0x7 || v[3](6) != 0x8 || v[3](7) != 0xb) {
		std::cerr << "FAIL: Wrong input for second frame third subframe" << std::endl;
		return;
	}
	std::cerr << "PASS!" << std::endl;
}

void truncate_future_complete_frame()
{
	short in1, in2;
	std::cerr << "Test #5: Truncate future complete frame" << std::endl;
	movie m;
	std::vector<uint8_t> state;
	controls_t c;
	m.readonly_mode(false);
	m.next_frame();
	c(4) = 0x1;
	c(5) = 0x2;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x1) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (1)" << std::endl;
		std::cerr << "Expected 1, got " << in1 << "." << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x2) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (1)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x3;
	c(5) = 0x4;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x3) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (2)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x4) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (2)" << std::endl;
		return;
	}
	c(4) = 0x5;
	c(5) = 0x6;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x5) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (3)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x6) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (3)" << std::endl;
		return;
	}
	m.next_frame();
	m.readonly_mode(true);
	m.next_frame();
	m.next_frame();
	m.next_frame();
	m.readonly_mode(false);
	std::vector<controls_t> v = m.save();
	if(v.size() != 6) {
		std::cerr << "FAIL: Unexpected size for movie" << std::endl;
		return;
	}
	if(v[0](0) != 1 || v[0](4) != 0x1 || v[0](5) != 0x2) {
		std::cerr << "FAIL: Wrong input for first frame" << std::endl;
		return;
	}
	if(v[1](0) != 1 || v[1](4) != 0x3 || v[1](5) != 0x4) {
		std::cerr << "FAIL: Wrong input for second frame first subframe" << std::endl;
		return;
	}
	if(v[2](0) != 0 || v[2](4) != 0x5 || v[2](5) != 0x6) {
		std::cerr << "FAIL: Wrong input for second frame second subframe" << std::endl;
		return;
	}
	if(v[3](0) != 1 || v[3](4) != 0 || v[3](5) != 0) {
		std::cerr << "FAIL: Wrong input for third frame" << std::endl;
		return;
	}
	if(v[4](0) != 1 || v[4](4) != 0 || v[4](5) != 0) {
		std::cerr << "FAIL: Wrong input for fourth frame" << std::endl;
		return;
	}
	if(v[5](0) != 1 || v[5](4) != 0 || v[5](5) != 0) {
		std::cerr << "FAIL: Wrong input for fifth frame" << std::endl;
		return;
	}
	std::cerr << "PASS!" << std::endl;
}

void truncate_future_incomplete_frame()
{
	short in1, in2;
	std::cerr << "Test #6: Truncate future incomplete frame" << std::endl;
	movie m;
	std::vector<uint8_t> state;
	controls_t c;
	m.readonly_mode(false);
	m.next_frame();
	c(4) = 0x1;
	c(5) = 0x2;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x1) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (1)" << std::endl;
		std::cerr << "Expected 1, got " << in1 << "." << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x2) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (1)" << std::endl;
		return;
	}
	m.next_frame();
	c(4) = 0x3;
	c(5) = 0x4;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x3) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (2)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x4) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (2)" << std::endl;
		return;
	}
	c(4) = 0x5;
	c(5) = 0x6;
	m.set_controls(c);
	in1 = m.next_input(4);
	if(in1 != 0x5) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (3)" << std::endl;
		return;
	}
	in2 = m.next_input(5);
	if(in2 != 0x6) {
		std::cerr << "FAIL: Unexpected return for m.next_input(5) (3)" << std::endl;
		return;
	}
	m.next_frame();
	m.readonly_mode(true);
	m.next_frame();
	m.next_frame();
	m.next_frame();
	in1 = m.next_input(4);
	if(in1 != 0x0) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (4)" << std::endl;
		return;
	}
	in1 = m.next_input(4);
	if(in1 != 0x0) {
		std::cerr << "FAIL: Unexpected return for m.next_input(4) (5)" << std::endl;
		return;
	}
	m.readonly_mode(false);
	std::vector<controls_t> v = m.save();
	if(v.size() != 7) {
		std::cerr << "FAIL: Unexpected size for movie (got " << v.size() << ")" << std::endl;
		return;
	}
	if(v[0](0) != 1 || v[0](4) != 0x1 || v[0](5) != 0x2) {
		std::cerr << "FAIL: Wrong input for first frame" << std::endl;
		return;
	}
	if(v[1](0) != 1 || v[1](4) != 0x3 || v[1](5) != 0x4) {
		std::cerr << "FAIL: Wrong input for second frame first subframe" << std::endl;
		return;
	}
	if(v[2](0) != 0 || v[2](4) != 0x5 || v[2](5) != 0x6) {
		std::cerr << "FAIL: Wrong input for second frame second subframe" << std::endl;
		return;
	}
	if(v[3](0) != 1 || v[3](4) != 0 || v[3](5) != 0) {
		std::cerr << "FAIL: Wrong input for third frame" << std::endl;
		return;
	}
	if(v[4](0) != 1 || v[4](4) != 0 || v[4](5) != 0) {
		std::cerr << "FAIL: Wrong input for fourth frame" << std::endl;
		return;
	}
	if(v[5](0) != 1 || v[5](4) != 0 || v[5](5) != 0) {
		std::cerr << "FAIL: Wrong input for fifth frame" << std::endl;
		return;
	}
	if(v[6](0) != 1 || v[6](4) != 0 || v[6](5) != 0) {
		std::cerr << "FAIL: Wrong input for sixth frame" << std::endl;
		return;
	}
	std::cerr << "PASS!" << std::endl;
}

int main()
{
	truncate_past_complete_frame();
	truncate_past_incomplete_frame();
	truncate_current_complete_frame();
	truncate_current_incomplete_frame();
	truncate_future_complete_frame();
	truncate_future_incomplete_frame();
	return 0;
}