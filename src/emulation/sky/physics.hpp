#ifndef _skycore__physics__hpp__included__
#define _skycore__physics__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include "level.hpp"

namespace sky
{
	struct noise_maker
	{
		virtual ~noise_maker();
		virtual void operator()(int sound, bool hipri = true) = 0;
	};

	struct physics
	{
		const static uint8_t death_none = 0;		//Playing.
		const static uint8_t death_burning = 1;		//Hit burning floor.
		const static uint8_t death_drifting = 2;	//Drifting forever in space.
		const static uint8_t death_fuel = 3;		//Ran out of fuel.
		const static uint8_t death_o2 = 4;		//Suffocated.
		const static uint8_t death_collided = 5;	//Ram wall too fast.
		const static uint8_t death_escaped = 6;		//Escaped level.
		const static uint8_t death_finished = 255;	//Exited level (finished).
		const static uint8_t flag_locked = 1;		//In locked jump.
		const static uint8_t flag_tried_lock = 2;	//Tried to lock this jump.
		const static uint8_t flag_jumping = 4;		//Currently in jump.
		const static uint8_t flag_landed = 8;		//Currently landed.
		const static uint8_t flag_slippery = 16;	//Slippery effect.
		const static uint8_t flag_sticky = 32;		//Sticky effect.
		const static uint8_t flag_blank = 64;		//Tile blank.
		const static uint8_t flag_no_jump = 128;	//Disable jumping.
		//This class MUST NOT contain pointers and MUST be prepared to deal with any values!
		uint32_t framecounter;		//Frame counter.
		uint32_t deathframe;		//Frame of death.
		uint32_t lpos;			//Longitudial position.
		uint32_t lprojected;		//Longitudial projected position.
		int32_t lspeed;			//Longitudial speed.
		int32_t speedbias;		//Temporary jump speed boost
		uint16_t expframe;		//Explosion frame (0 if not exploded).
		uint16_t postdeath;		//Number of frames after death.
		int16_t gravity_accel;		//Acceleration of gravity (negative)
		uint16_t hpos;			//Horizontal position.
		int16_t vpos;			//Vertical position.
		uint16_t hprojected;		//Horizontal projected position.
		int16_t vprojected;		//Vertical projected position.
		int16_t hspeed;			//Horizontal speed.
		int16_t vspeed;			//Vertical speed.
		int16_t hdrift;			//Speed of horizontal drift.
		int16_t jump_ground;		//Jumping ground level.
		int16_t nosupport;		//Amount of lack of support.
		int16_t nosupport_d;		//Balance of lack of support.
		uint16_t fuel_left;		//Amount of fuel left.
		uint16_t o2_left;		//Amount of O2 left.
		int16_t gravity;		//Level gravity.
		uint16_t o2_factor;		//O2 use factor.
		uint16_t fuel_factor;		//Fuel use factor.
		uint16_t bounce_limit;		//Minimum speed ship bounces on.
		int16_t o2_amount;		//Level amount of O2.
		uint16_t fuel_amount;		//Level amount of fuel
		uint8_t death;			//Cause of death (0 => Not dead)
		uint8_t flags;			//Flags.
		//Padding to multiple of 8 bytes.
		uint8_t padA;
		uint8_t padB;
		uint8_t padC;
		uint8_t padD;

		void level_init(level& stage);
		uint8_t simulate_frame(level& stage, noise_maker& noise, int lr, int ad, bool jump);
		bool is_set(uint8_t b);
	private:
		void adjust_speed(level& stage, int adjust) throw();
		void die(level& stage, uint8_t cause);
		void explode(level& stage, noise_maker& noise);
		void apply_floor_effects(level& stage, noise_maker& noise, unsigned floor);
		void check_exit(level& stage) throw();
		void use_suppiles(level& stage) throw();
		void check_death(level& stage) throw();
		void apply_steering(level& stage, int lr, int ad, bool jump) throw();
		void apply_gravity(level& stage) throw();
		void project_position(level& stage) throw();
		uint8_t get_death(level& stage) throw();
		void check_scratching(level& stage) throw();
		void check_collisions(level& stage, noise_maker& noise) throw();
		void try_locking(level& stage, int lr, int ad, bool jump) throw();
		void check_horizontal_eject(level& stage, noise_maker& noise) throw();
		void apply_bounce(level& stage, noise_maker& noise) throw();
		void check_landing(level& stage) throw();
		void move_ship(level& stage) throw();
		bool dangerous_jump(level& stage, int ad, uint32_t lp, uint16_t hp, int16_t vp, int32_t lv,
			int16_t hv, int16_t vv, int16_t hd);
		void force_flag(uint8_t b, bool s);
		void set_flag(uint8_t b);
		void clear_flag(uint8_t b);
		bool is_masked(uint8_t b, uint8_t c);
		bool is_any_of(uint8_t b);
		bool is_clear(uint8_t b);
	};
}
#endif
