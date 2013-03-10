#include "physics.hpp"
#include "sound.hpp"
#include "util.hpp"
#include <cstdlib>

namespace sky
{
	noise_maker::~noise_maker()
	{
	}

	void physics::force_flag(uint8_t b, bool s)
	{
		if(s)
			flags |= b;
		else
			flags &= ~b;
	}

	void physics::set_flag(uint8_t b)
	{
		flags |= b;
	}

	void physics::clear_flag(uint8_t b)
	{
		flags &= ~b;
	}

	bool physics::is_masked(uint8_t b, uint8_t c)
	{
		return ((flags & b) == c);
	}

	bool physics::is_set(uint8_t b)
	{
		return ((flags & b) == b);
	}

	bool physics::is_any_of(uint8_t b)
	{
		return ((flags & b) != 0);
	}

	bool physics::is_clear(uint8_t b)
	{
		return ((flags & b) == 0);
	}

	void physics::adjust_speed(level& stage, int adjust) throw()
	{
		lspeed += adjust;
		if(lspeed < 0)
			lspeed = 0;
		if(lspeed > 10922)
			lspeed = 10922;
	}
	void physics::die(level& stage, uint8_t cause)
	{
		if(death)
			return;
		death = cause;
		deathframe = framecounter;
	}
	void physics::explode(level& stage, noise_maker& noise)
	{
		if(expframe)
			return;
		expframe = 1;
		noise(sound_explode);
	}
	void physics::apply_floor_effects(level& stage, noise_maker& noise, unsigned floor)
	{
		if(is_clear(flag_landed)) {
			clear_flag(flag_sticky);
			return;
		}
		switch(floor) {
		case tile::sticky:
			if(!expframe)
				adjust_speed(stage, -303);
			break;
		case tile::suppiles:
			if(death)
				break;
			if(o2_left < 27000 || fuel_left < 27000)
				noise(sound_suppiles);
			o2_left = 30000;
			fuel_left = 30000;
			break;
		case tile::boost:
			if(!expframe)
				adjust_speed(stage, 303);
			break;
		case tile::burning:
			die(stage, death_burning);
			explode(stage, noise);
			break;
		}
		force_flag(flag_slippery, floor == tile::slippery);
		force_flag(flag_sticky, floor == tile::sticky);
	}
	void physics::check_exit(level& stage) throw()
	{
		if(lpos < stage.finish_line())
			return;
		if(!stage.in_pipe(lpos, hpos, vpos))
			return;
		//Die preserves death reason if one exists.
		die(stage, death_finished);
	}
	void physics::use_suppiles(level& stage) throw()
	{
		if(death)
			return;
		o2_left -= o2_factor;
		if(o2_left > 30000)
			o2_left = 0;
		fuel_left -= (fuel_factor * lspeed) / 65536;
		if(fuel_left > 30000)
			fuel_left = 0;
	}
	void physics::check_death(level& stage) throw()
	{
		if(!death) {
			if(vpos < 10240)
				die(stage, death_drifting);
			else if(fuel_left == 0)
				die(stage, death_fuel);
			else if(o2_left == 0)
				die(stage, death_o2);
		} else
			postdeath++;
		if(expframe)
			expframe++;
	}
	void physics::apply_steering(level& stage, int lr, int ad, bool jump) throw()
	{
		if(death)
			return;
		adjust_speed(stage, 75 * ad);
		if(is_clear(flag_slippery)) {
			if(is_any_of(flag_blank | flag_jumping)) {
				if(hspeed == 0 && vspeed > 0 && vpos - jump_ground < 3840)
					hspeed = 29 * lr;
			} else
				hspeed = 29 * lr;
		}
		if(jump && is_clear(flag_blank | flag_jumping | flag_no_jump)) {
			vspeed = 1152;
			set_flag(flag_jumping);
			jump_ground = vpos;
		}
	}
	void physics::apply_gravity(level& stage) throw()
	{
		if(!expframe) {
			if(vpos >= 10240)
				vspeed = vspeed + gravity_accel;
			else if(vspeed > -106)
				vspeed = -106;
		} else {
			if(vspeed < 0)
				vspeed = 0;
			else if(vspeed < 71)
				vspeed += 39;
			else
				vspeed = 71;
		}
	}
	void physics::project_position(level& stage) throw()
	{
		lprojected = lpos + lspeed;
		hprojected = hpos + (hspeed * (lspeed + (is_set(flag_sticky) ? 0 : 1560))) / 512  + hdrift;
		vprojected = vpos + vspeed;
		//Don't wrap around horizontally.
		if((hpos < 12160 && hprojected >= 53376) && (hpos >= 53376 && hprojected < 12160))
			hprojected = hpos;
	}
	uint8_t physics::get_death(level& stage) throw()
	{
		if(!death)
			return 0;	//Still alive.
		else if(death == death_finished) {
			framecounter++;
			lpos += lspeed;
			return (++postdeath > 72) ? death : 0;
		} else if(death == death_collided || death == death_burning)
			return (expframe > 42) ? death : 0;
		else if(death == death_drifting)
			return (expframe > 42 || postdeath > 108) ? death : 0;
		else
			return (postdeath > 108) ? death : 0;
	}
	void physics::check_scratching(level& stage) throw()
	{
		if(hprojected == hpos)
			return;
		hspeed = 0;
		if(hdrift < 0 && hpos > hprojected)
			hdrift = 0;
		if(hdrift > 0 && hpos < hprojected)
			hdrift = 0;
		adjust_speed(stage, -151);
	}
	void physics::check_collisions(level& stage, noise_maker& noise) throw()
	{
		if(lprojected == lpos)
			return;
		if(lspeed >= 3640) {
			die(stage, death_collided);
			explode(stage, noise);
		} else {
			if(lpos > lprojected - lspeed)
				noise(sound_blow);
		}
		lspeed = 0;
	}
	void physics::try_locking(level& stage, int lr, int ad, bool jump) throw()
	{
		if(vpos < 14080 || !is_masked(flag_tried_lock | flag_jumping, flag_jumping))
			return;
		set_flag(flag_tried_lock);
		if(!dangerous_jump(stage, ad, lpos, hpos, vpos, lspeed, hspeed, vspeed, hdrift))
			return;
		for(int32_t a = 1; a < 7; a++) {
			if(!dangerous_jump(stage, ad, lpos, hpos, vpos, lspeed, hspeed + (a * hspeed) / 10, vspeed,
				hdrift)) {
				hspeed = hspeed + (a * hspeed) / 10;
				set_flag(flag_locked);
				return;
			}
			if(!dangerous_jump(stage, ad, lpos, hpos, vpos, lspeed, hspeed - (a * hspeed) / 10, vspeed,
				hdrift)) {
				hspeed = hspeed - (a * hspeed) / 10;
				set_flag(flag_locked);
				return;
			}
			int32_t tmp = lspeed + (a * lspeed) / 10;
			if(tmp < 10922)
				if(!dangerous_jump(stage, ad, lpos, hpos, vpos, tmp, hspeed, vspeed, hdrift)) {
					speedbias = tmp - lspeed;
					lspeed = tmp;
					set_flag(flag_locked);
					return;
				}
			if(!dangerous_jump(stage, ad, lpos, hpos, vpos, lspeed - (a * lspeed) / 10, hspeed, vspeed,
				hdrift)) {
				speedbias = - (a * lspeed) / 10;
				lspeed = lspeed - (a * lspeed) / 10;
				set_flag(flag_locked);
				return;
			}
		}
	}
	void physics::check_horizontal_eject(level& stage, noise_maker& noise) throw()
	{
		if(lprojected == lpos || hpos != hprojected)
			return;
		if(!stage.collides(lprojected, hpos, vpos))
			return;
		if(!stage.collides(lprojected, hpos - 928, vpos)) {
			hpos -= 928;
			lprojected = lpos;
			noise(sound_blow);
		} else if(!stage.collides(lprojected, hpos + 928, vpos)) {
			hpos += 928;
			lprojected = lpos;
			noise(sound_blow);
		}
	}
	void physics::apply_bounce(level& stage, noise_maker& noise) throw()
	{
		if(vprojected == vpos)
			return;
		if(hdrift != 0 && nosupport < 2) {
			vspeed = 0;
			return;
		}
		if(expframe || std::abs(vspeed) < bounce_limit) {
			vspeed = 0;
			return;
		}
		if(!death && vspeed < 0)
			noise(sound_bounce, false);
		vspeed = -((5 * static_cast<int16_t>(vspeed)) / 10);
	}
	void physics::check_landing(level& stage) throw()
	{
		if(vprojected == vpos || vspeed >= 0)
			return;
		clear_flag(flag_locked | flag_tried_lock | flag_jumping);
		set_flag(flag_landed);
		adjust_speed(stage, -speedbias);
		speedbias = 0;
		nosupport_d = 0;
		for(int i = 1; i <= 14; i++)
			if(!stage.collides(lpos, hpos + 128 * i, vpos - 1)) {
				nosupport = i;
				nosupport_d++;
				break;
			}
		for(int i = 1; i <= 14; i++)
			if(!stage.collides(lpos, hpos - 128 * i, vpos - 1)) {
				nosupport = i;
				nosupport_d--;
				break;
			}
		if(nosupport_d)
			hdrift += 17 * nosupport_d;
		else
			hdrift = 0;
	}
	void physics::move_ship(level& stage) throw()
	{
		if(lprojected == lpos && hprojected == hpos && vprojected == vpos)
			return;
		uint32_t tmp_l;
		uint16_t tmp_h;
		int32_t tmp_v;
		int32_t delta_l = lprojected - lpos;
		int16_t delta_h = hprojected - hpos;
		int16_t delta_v = vprojected - vpos;
		int i;
		for(i = 1; i <= 5; i++) {
			tmp_l = lpos + i * delta_l / 5;
			tmp_h = hpos + i * delta_h / 5;
			tmp_v = vpos + i * delta_v / 5;
			if(stage.collides(tmp_l, tmp_h, tmp_v)) {
				break;
			}
		}
		lpos = lpos + (i - 1) * delta_l / 5;
		hpos = hpos + (i - 1) * delta_h / 5;
		vpos = vpos + (i - 1) * delta_v / 5;
		for(i = 16384; i > 0; i /= 2)
			if(i <= abs(lprojected - lpos))
				if(!stage.collides(lpos + sgn(delta_l) * i, hpos, vpos))
					lpos = lpos + sgn(delta_l) * i;
		for(i = 16384; i > 0; i /= 2)
			if(i <= abs(hprojected - hpos))
				if(!stage.collides(lpos, hpos + sgn(delta_h) * i, vpos))
					hpos = hpos + sgn(delta_h) * i;
		for(i = 16384; i > 0; i /= 2)
			if(i <= abs(vprojected - vpos))
				if(!stage.collides(lpos, hpos, vpos + sgn(delta_v) * i))
					vpos = vpos + sgn(delta_v) * i;
	}
	bool physics::dangerous_jump(level& stage, int ad, uint32_t lp, uint16_t hp, int16_t vp, int32_t lv,
		int16_t hv, int16_t vv, int16_t hd)
	{
		uint16_t old_hp;
		uint32_t old_lp;
		do {
			old_hp = hp;
			old_lp = lp;
			vv = vv + gravity_accel;
			lp = lp + lv;
			hp = hp + hv * (lv + 1560) / 512 + hd;
			if(hp < 12160 || hp > 53376)
				return true;
			vp = vp + vv;
			lv = lv + 75 * ad;
			if(lv < 0)
				lv = 0;
			if(lv > 10922)
				lv = 10922;
		} while(vp > 10240);
		tile A = stage.at(old_lp, old_hp);
		tile B = stage.at(lp, hp);
		return A.is_dangerous() || B.is_dangerous();
	}
	uint8_t physics::simulate_frame(level& stage, noise_maker& noise, int lr, int ad, bool jump)
	{
		uint8_t cod = get_death(stage);
		if(cod)
			return cod;
		if(death == death_finished)
			return 0;	//The animation to scroll.
		tile t = stage.at(lpos, hpos);
		force_flag(flag_blank, t.is_blank());
		apply_floor_effects(stage, noise, t.surface_type(vpos));
		check_exit(stage);
		if(death == death_finished)
			return 0;	//If check_exit changed things.
		apply_bounce(stage, noise);
		apply_steering(stage, lr, ad, jump);
		try_locking(stage, lr, ad, jump);
		apply_gravity(stage);
		project_position(stage);
		move_ship(stage);
		check_horizontal_eject(stage, noise);
		check_collisions(stage, noise);
		check_scratching(stage);
		clear_flag(flag_landed);
		check_landing(stage);
		if(vpos < 0)    vpos = 0;
		use_suppiles(stage);
		check_death(stage);
		framecounter++;
		return 0;
	}
	void physics::level_init(level& stage)
	{
		gravity = stage.get_gravity();
		o2_amount = stage.get_o2_amount();
		fuel_amount = stage.get_fuel_amount();
		gravity_accel = -((72 * static_cast<int32_t>(gravity) / 5) & 0xFFFF);
		bounce_limit = (260 * static_cast<int32_t>(gravity) / 8) & 0xFFFF;
		if(o2_amount)
			o2_factor = 30000 / ((36 * (int32_t)o2_amount) & 0xFFFF);
		else
			o2_factor = 30000;
		if(fuel_amount)
			fuel_factor = 30000 / fuel_amount;
		else
			fuel_factor = 65535;
		framecounter = 0;
		deathframe = 0;
		lpos = 3 << 16;
		lprojected = 1999185;		//Crap from memory.
		lspeed = 0;
		speedbias = 0;
		expframe = 0;
		postdeath = 0;
		hpos = 32768;
		vpos = 10240;
		hprojected = 47370;		//Crap from memory.
		vprojected = 0;			//Crap from memory.
		hspeed = 0;
		vspeed = 0;
		hdrift = 0;
		jump_ground = 10240;
		nosupport = 19422;		//Crap from memory.
		nosupport_d = 16199;		//Crap from memory.
		jump_ground = 0;
		fuel_left = 30000;
		o2_left = 30000;
		death = 0;
		flags = flag_landed | ((gravity >= 20) ? flag_no_jump : 0);
	}
}
