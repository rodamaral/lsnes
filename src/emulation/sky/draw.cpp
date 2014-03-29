#include "draw.hpp"
#include <cmath>
#include <cstdio>
#include "romimage.hpp"
#include "framebuffer.hpp"
#include "messages.hpp"
#include "instance.hpp"
#include "library/minmax.hpp"

/*
	Point of escape is approximately at normalized y=0.16.
	Baseline is approximately at normalized y=0.52
	1st tile after starts approximately at y=0.38




*/

namespace sky
{
	//Projection parameters.
	//Inverse of normalized y0 at baseline.
	const double z0 = 1.9;
	//Depth projection scale.
	const double zscale = 0.6;
	//Point of escape on screen.
	const double yescape = 0.16;
	//Horizontal draw scale.
	const double hscale = 0.15;
	//Vertical draw scale.
	const double vscale = 0.1;
	//Normalized floor thickness.
	const double fthickness = 0.33;

	//Type of point representation.
	struct point_t
	{
		double x;
		double y;
	};

	const unsigned top_palette[] = {61, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	const unsigned front_palette[] = {62, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
	const unsigned right_palette[] = {63, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45};
	const unsigned left_palette[] = {64, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60};
	const double slopepoints[] = {-3.45, -2.45, -1.45, 0, 1.45, 2.45, 3.45};
	const double angles[] = {1.35, 1.07, 0.88, 0.745};

	double calc_slope(double x)
	{
		return 1.6 * (hscale * x) / (1 / z0 - yescape);
	}

	std::pair<signed, signed> point_project(double x, double y, double z)
	{
		double c = (z0 + zscale * z);
		if(c < 0.001)
			c = 0.001;
		double y0 = 1 / c;
		double s = (y0 - yescape) / (1 / z0 - yescape);
		if(s >= 0)
			return std::make_pair(FB_WIDTH * (hscale * x * s + 0.5), FB_HEIGHT * (y0 - vscale * y * s));
		else
			return std::make_pair(FB_WIDTH / 2, FB_HEIGHT * yescape);
	}

	point_t point_project_b(double x, double y, double z)
	{
		double c = (z0 + zscale * z);
		if(c < 0.001)
			c = 0.001;
		double y0 = 1 / c;
		double s = (y0 - yescape) / (1 / z0 - yescape);
		point_t p;
		if(s >= 0) {
			p.x = FB_WIDTH * (hscale * x * s + 0.5);
			p.y = FB_HEIGHT * (y0 - vscale * y * s);
		} else {
			p.x = FB_WIDTH / 2;
			p.y = FB_HEIGHT * yescape;
		}
		return p;
	}

	int32_t ship_sprite_normal(uint16_t hpos, int vdir, uint8_t thruster)
	{
		int hdir;
		if(hpos < 18048) hdir = 0;
		else if(hpos < 23936) hdir = 1;
		else if(hpos < 29824) hdir = 2;
		else if(hpos < 35712) hdir = 3;
		else if(hpos < 41600) hdir = 4;
		else if(hpos < 47488) hdir = 5;
		else hdir = 6;
		if(vdir == -1) vdir = 2;
		return 9 * hdir + 3 * vdir + thruster + 14;
	}

	int32_t ship_sprite_explode(int32_t frame)
	{
		return (frame < 42) ? (frame / 3) : -1;
	}

	int32_t ship_sprite(gstate& s)
	{
		if(s.p.expframe)
			return ship_sprite_explode(s.p.expframe);
		if(s.p.death == physics::death_finished)
			return -1;
		int vdir;
		uint16_t thruster;
		if(s.p.vspeed > 300) vdir = 1;
		else if(s.p.vspeed < -300) vdir = -1;
		else vdir = 0;
		if(s.p.death == physics::death_fuel)
			thruster = 0;
		else
			thruster = s.rng.pull() % 3;
		return ship_sprite_normal(s.p.hpos, vdir, thruster % 3);
	}

	void draw_sprite(struct instance& inst, double h, double v, int32_t num)
	{
		if(num < 0)
			return;
		signed x = FB_WIDTH * hscale * h + FB_WIDTH / 2 - FB_SCALE * 15;
		signed y = FB_HEIGHT / z0 - FB_HEIGHT * vscale * v - FB_SCALE * inst.ship.width + FB_SCALE;
		uint32_t offset = inst.ship.width * 30 * num;
		//For some darn reason, CARS.LZS has the image rotated 90 degrees counterclockwise.
		for(signed j = 0; j < FB_SCALE * (int)inst.ship.width; j++) {
			if(y + j < 0 || y + j > inst.overlap_end)
				continue;
			uint32_t offset2 = offset + j / FB_SCALE;
			size_t base = FB_WIDTH * (y + j) + x;
			for(signed i = 0; i < 30 * FB_SCALE; i++) {
				if(x + i < 0)
					continue;
				if(x + i >= FB_WIDTH)
					break;
				uint8_t c = inst.ship.decode[offset2 + inst.ship.width * (i / FB_SCALE)];
				if(!c)
					continue;
				uint32_t pix = inst.ship.palette[c] & 0xFFFFFF;
				if((inst.framebuffer[base + i] >> 24) == 0)
					inst.framebuffer[base + i] = pix;
			}
		}
	}

	void draw_grav_g_meter(struct instance& inst)
	{
		uint16_t realgrav = 100 * (inst.state.p.gravity - 3);
		uint16_t x = 116;
		uint16_t y = 156;
		static const size_t sep = strlen(_numbers_g) / 10;
		while(realgrav) {
			uint8_t digit = realgrav % 10;
			draw_block2(inst, _numbers_g + sep * digit, y * 320 + (x - 5), inst.dashpalette[6],
				inst.dashpalette[5], true);
			x -= 5;
			realgrav /= 10;
		}
	}

	void draw_indicator(struct instance& inst, uint8_t& curval, uint8_t newval, gauge& g, uint8_t on1,
		uint8_t on2, uint8_t off1, uint8_t off2)
	{
		unsigned tmp = newval;
		if(tmp > g.maxlimit()) tmp = g.maxlimit();
		if(curval < tmp)
			for(unsigned i = curval; i < tmp; i++) {
				draw_block(inst, g.get_data(i), g.get_position(i), inst.dashpalette[on1],
					inst.dashpalette[on2]);
			}
		else
			for(unsigned i = tmp; i < curval; i++) {
				draw_block(inst, g.get_data(i), g.get_position(i), inst.dashpalette[off1],
					inst.dashpalette[off2]);
			}
		curval = tmp;
	}

	void draw_gauges(struct instance& inst)
	{
		//draw_grav_g_meter(s);
		uint8_t timermod = ((inst.state.p.framecounter % 9) > 4) ? 1 : 0;
		draw_indicator(inst, inst.state.speedind, (inst.state.p.lspeed - inst.state.p.speedbias) / 0x141,
			inst.speed_dat, 2, 3, 0, 1);
		draw_indicator(inst, inst.state.o2ind, (inst.state.p.o2_left + 0xbb7) / 0xbb8, inst.oxydisp_dat, 2, 3,
			0, 1);
		draw_indicator(inst, inst.state.fuelind, (inst.state.p.fuel_left + 0xbb7) / 0xbb8, inst.fueldisp_dat,
			2, 3, 0, 1);
		//Lock indicator.
		bool lck = inst.state.p.is_set(physics::flag_locked);
		if(lck != inst.state.lockind)
			draw_block2(inst, _lockind_g + (lck ? strlen(_lockind_g) / 2 : 0), 0x9c * 320 + 0xcb,
				inst.dashpalette[6], inst.dashpalette[5], true);
		inst.state.lockind = lck;
		//Out of oxygen blink&beep.
		if(inst.state.p.death == physics::death_o2 && inst.state.beep_phase != timermod) {
			blink_between(inst, 0xa0, 0xa1, 7, 7, inst.dashpalette[7], inst.dashpalette[8]);
			if(timermod)
				inst.gsfx(sound_beep);
		}
		//Out of fuel blink&beep.
		if(inst.state.p.death == physics::death_fuel && inst.state.beep_phase != timermod) {
			blink_between(inst, 0x9b, 0xa9, 16, 5, inst.dashpalette[7], inst.dashpalette[8]);
			if(timermod)
				inst.gsfx(sound_beep);
		}
		//Distance gauge.
		uint32_t res = inst.state.curlevel.apparent_length() / (29 * FB_SCALE + 1);
		size_t tmp = (inst.state.p.lpos - 3 * 65536) / res;
		for(unsigned i = inst.state.distind; i < tmp && i < (29 * FB_SCALE + 1); i++)
			draw_distance_column(inst, i, inst.dashpalette[4]);
		inst.state.distind = tmp;
		inst.state.beep_phase = timermod;
	}

	inline double horizon_distance() { return (1 / yescape - z0) / zscale; }
	inline double near_distance(struct instance& inst) { return (600 / inst.overlap_end - z0) / zscale; }


	void draw_quad_x(struct instance& inst, point_t p1, point_t p2, point_t p3, point_t p4, uint32_t color)
	{
		if(fabs(p1.x - p2.x) < 0.1)
			return;
		double qp1 = min(p1.x, p2.x);
		double qp2 = max(p1.x, p2.x);
		int x1 = floor(qp1);
		int x2 = ceil(qp2);
		double utmax = max(p1.y, p2.y);
		double utmin = min(p1.y, p2.y);
		double ltmax = max(p3.y, p4.y);
		double ltmin = min(p3.y, p4.y);
		signed y1 = max((int)floor(utmin), 0);
		signed y2 = min((int)ceil(ltmax), (int)inst.overlap_end);
		for(signed j = y1; j < y2; j++) {
			signed xstart = x1, xend = x2;
			if(j < utmax) {
				if(p1.y > p2.y)
					xstart = floor((qp1 - qp2) * (j - p2.y) / (p1.y - p2.y) + qp2);
				else
					xend = ceil((qp2 - qp1) * (j - p1.y) / (p2.y - p1.y) + qp1);
			}
			if(j >= ltmin) {
				if(p3.y > p4.y)
					xend = ceil((qp1 - qp2) * (j - p4.y) / (p3.y - p4.y) + qp2);
				else
					xstart = floor((qp2 - qp1) * (j - p3.y) / (p4.y - p3.y) + qp1);
			}
			xstart = max(xstart, 0);
			xend = min(xend, FB_WIDTH);
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = xstart; i < xend; i++)
					inst.framebuffer[base + i] = color;
			else
				for(signed i = xstart; i < xend; i++)
					framebuffer_blend2(inst.framebuffer[base + i], color);
		}
	}

	void draw_quad_y(struct instance& inst, point_t p1, point_t p2, point_t p3, point_t p4, uint32_t color)
	{
		if(fabs(p1.y - p3.y) < 0.1)
			return;
		signed y1 = max((int)floor(p1.y), 0);
		signed y2 = min((int)ceil(p3.y), (int)inst.overlap_end);
		for(signed j = y1; j < y2; j++) {
			signed xstart, xend;
			xstart = floor((p3.x - p1.x) * (j - p1.y) / (p3.y - p1.y) + p1.x);
			xend = ceil((p4.x - p2.x) * (j - p1.y) / (p3.y - p1.y) + p2.x);
			xstart = max(xstart, 0);
			xend = min(xend, FB_WIDTH);
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = xstart; i < xend; i++)
					inst.framebuffer[base + i] = color;
			else
				for(signed i = xstart; i < xend; i++)
					framebuffer_blend2(inst.framebuffer[base + i], color);
		}
	}

	void draw_quad_z(struct instance& inst, point_t p1, point_t p2, point_t p3, point_t p4, uint32_t color)
	{
		if(fabs(p1.x - p2.x) < 0.1 || fabs(p1.y - p3.y) < 0.1)
			return;
		signed x1 = max((int)floor(p1.x), 0);
		signed x2 = min((int)ceil(p2.x), FB_WIDTH);
		signed y1 = max((int)floor(p1.y), 0);
		signed y2 = min((int)ceil(p3.y), (int)inst.overlap_end);
		for(signed j = y1; j < y2; j++) {
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = x1; i < x2; i++)
					inst.framebuffer[base + i] = color;
			else
				for(signed i = x1; i < x2; i++)
					framebuffer_blend2(inst.framebuffer[base + i], color);
		}
	}

	void draw_quad_z_tunshadow(struct instance& inst, point_t p1, point_t p2, point_t p3, point_t p4,
		uint32_t color, int x)
	{
		if(fabs(p1.x - p2.x) < 0.1 || fabs(p1.y - p3.y) < 0.1)
			return;
		if(!x)
			return;		//Center pipe has no shadow.
		signed x1 = max((int)floor(p1.x), 0);
		signed x2 = min((int)ceil(p2.x), FB_WIDTH);
		signed y1 = max((int)floor(p1.y), 0);
		signed y2 = min((int)ceil(p3.y), (int)inst.overlap_end);
		double slope = calc_slope(slopepoints[x + 3]);
		const double width = p2.x - p1.x;
		const double hwidth = 0.45 * width;
		const double center = (p1.x + p2.x) / 2;
		for(signed j = y1; j < y2; j++) {
			signed c1 = x2, c2 = x1;
			size_t base = FB_WIDTH * j;
			double ry = (p3.y - j) / (0.625 * vscale / hscale * 2 * hwidth);
			if(ry < 1) {
				c1 = center - hwidth * sqrt(1 - ry * ry);
				c2 = center + hwidth * sqrt(1 - ry * ry);
			}
			if(x < 0) {
				//The shadow is on left.
				c2 = center - hwidth - slope * (p3.y - j);
			} else {
				//The shadow is on right.
				c1 = center + hwidth - slope * (p3.y - j);
			}
			c1 = max(c1, 0);
			c2 = min(c2, FB_WIDTH);
			if(j < inst.overlap_start)
				for(signed i = c1; i < c2; i++)
					inst.framebuffer[base + i] = color;
			else
				for(signed i = c1; i < c2; i++)
					framebuffer_blend2(inst.framebuffer[base + i], color);
		}
	}

	void draw_quad_z_tunnel(struct instance& inst, point_t p1, point_t p2, point_t p3, point_t p4, uint32_t color)
	{
		if(fabs(p1.x - p2.x) < 0.1 || fabs(p1.y - p3.y) < 0.1)
			return;
		signed x1 = max((int)floor(p1.x), 0);
		signed x2 = min((int)ceil(p2.x), FB_WIDTH);
		signed y1 = max((int)floor(p1.y), 0);
		signed y2 = min((int)ceil(p3.y), (int)inst.overlap_end);
		const double width = p2.x - p1.x;
		const double hwidth = 0.45 * width;
		const double center = (p1.x + p2.x) / 2;
		for(signed j = y1; j < y2; j++) {
			signed c1 = x2, c2 = x2;
			size_t base = FB_WIDTH * j;
			double ry = (p3.y - j) / (0.625 * vscale / hscale * 2 * hwidth);
			if(ry < 1) {
				c1 = center - hwidth * sqrt(1 - ry * ry);
				c2 = center + hwidth * sqrt(1 - ry * ry);
			}
			x1 = max(x1, 0);
			c2 = max(c2, 0);
			x2 = min(x2, FB_WIDTH);
			c1 = min(c1, FB_WIDTH);
			if(j < inst.overlap_start) {
				for(signed i = x1; i < c1; i++)
					inst.framebuffer[base + i] = color;
				for(signed i = c2; i < x2; i++)
					inst.framebuffer[base + i] = color;
			} else {
				for(signed i = x1; i < c1; i++)
					framebuffer_blend2(inst.framebuffer[base + i], color);
				for(signed i = c2; i < x2; i++)
					framebuffer_blend2(inst.framebuffer[base + i], color);
			}
		}
	}

	void draw_quad_z_pipefront(struct instance& inst, double z, int x, uint32_t color)
	{
		auto p5 = point_project_b(x - 0.5, 0, z);
		auto p6 = point_project_b(x, 1, z);
		auto p7 = point_project_b(x + 0.5, 0, z);
		double ymind = p6.y;
		double ymaxd = p5.y;
		int16_t ymax = ceil(ymaxd);
		if(ymaxd - ymind < 0.1)
			return;
		int16_t cheight = floor(p5.y - p6.y);
		int16_t ciheight = floor(0.9 * (p5.y - p6.y));
		int16_t cwidth = floor((p7.x - p5.x) / 2);
		int16_t ciwidth = floor(0.9 * (p7.x - p5.x) / 2);
		int16_t ccenter = floor((p7.x + p5.x) / 2);
		int16_t nxs = ccenter - cwidth;
		int16_t nxe = ccenter + cwidth;
		for(signed j = ymax - cheight; j < ymax; j++) {
			if(j < 0 || j > inst.overlap_end)
				continue;
			int16_t dstart = nxs;
			int16_t dend = nxe;
			int16_t cstart = ccenter;
			int16_t cend = ccenter;
			int16_t cistart = ccenter;
			int16_t ciend = ccenter;
			if(true) {
				int16_t o = ymax - j;
				double o2 = 1.0 * o / cheight;
				int16_t w = cwidth * sqrt(1 - o2 * o2);
				cstart -= w;
				cend += w;
			}
			if(j >= ymax - ciheight) {
				int16_t o = ymax - j;
				double o2 = 1.0 * o / ciheight;
				int16_t w = ciwidth * sqrt(1 - o2 * o2);
				cistart -= w;
				ciend += w;
			}
			dstart = max(cstart, (int16_t)0);
			dend = min(cend, (int16_t)FB_WIDTH);
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = dstart; i < dend; i++) {
					if(i < cistart || i >= ciend)
						inst.framebuffer[base + i] = color;
				}
			else
				for(signed i = dstart; i < dend; i++) {
					if(i < cistart || i >= ciend)
						framebuffer_blend2(inst.framebuffer[base + i], color);
				}
		}
	}

// x = (k*y0+b)*c*(0.5*sin(alpha)+X0)
// y = -(k*y0+b)*d*cos(alpha)+y0
//
// dx/dy0 = k*c*0.5*sin(alpha)+k*c*X0
// dy/dy0 = k*d*cos(alpha)+1
// dx/dy = (k*c*0.5*sin(alpha)+k*c*X0)/(k*d*cos(alpha)+1)
// d/dalpha[(k*c*0.5*sin(alpha)+k*c*X0)/(k*d*cos(alpha)+1)]
//=(k*c*0.5*cos(alpha))/(k*d*cos(alpha)+1) +(k*c*0.5*sin(alpha)+k*c*X0)*(k*d*sin(alpha))/(k*d*cos(alpha)+1)^2
//
// 0.5*k*d+0.5*cos(alpha)+X0*(k*d*sin(alpha)) = 0

	uint32_t mix_color(uint32_t* c, uint32_t c2)
	{
		uint32_t low = c[c2 / 256];
		uint32_t high = c[c2 / 256 + 1];
		uint16_t mod = c2 % 256;
		uint16_t imod = 256 - mod;
		const uint32_t Lm = 0x00FF00FF;
		const uint32_t Hm = 0xFF00FF00;
		uint32_t L = (((low & Lm) * imod + (high & Lm) * mod) >> 8) & Lm;
		uint32_t H = (((low & Hm) >> 8) * imod + ((high & Hm) >> 8) * mod) & Hm;
		return L | H;
	}

	void rebuild_pipe_quad_cache(pipe_cache& p, int x, uint32_t* c)
	{
		double k = 1 / (1 / z0 - yescape);
		double b = -yescape * k;
		double gmin = 999999;
		double gmax = -999999;
		double amin = 999999;
		double amax = -999999;
		double y00 = (1-b)/k;
		for(unsigned i = 0; i < 256; i++)
			p.colors[i] = 0xFFFFFFFF;
		//Compute span range.
		for(double a = -M_PI / 2; a < M_PI / 2; a += 0.01) {
			double A = vscale * cos(a);
			double y0 = (y00 + b * A) / (1 - k * A);
			double xp = (k * y0 + b) * (x + 0.5 * sin(a));
			if(xp < gmin) {
				p.min_h = x + 0.5 * sin(a);
				p.min_v = cos(a);
				gmin = xp;
				amin = a;
			}
			if(xp > gmax) {
				p.max_h = x + 0.5 * sin(a);
				p.max_v = cos(a);
				gmax = xp;
				amax = a;
			}
		}
		for(double a = amin; a < amax; a += 0.01) {
			double A = vscale * cos(a);
			double y0 = (y00 + b * A) / (1 - k * A);
			double xp = (k * y0 + b) * (x + 0.5 * sin(a));
			signed c2 = (a + M_PI / 2) * 1280 / M_PI;
			signed x = 255 * (xp - gmin) / (gmax - gmin);
			c2 = max(min(c2, 1280), 0);
			x = max(min(x, 255), 0);
			p.colors[x] = mix_color(c, c2);
		}
		for(unsigned i = 0; i < 256; i++) {
			if(p.colors[i] == 0xFFFFFFFF) {
				if(i == 0) {
					for(unsigned j = 0; j < 255; j++)
						if(p.colors[j] != 0xFFFFFFFF) {
							p.colors[i] = p.colors[j];
							break;
						}
				} else
					p.colors[i] = p.colors[i - 1];
			}
		}
		//for(unsigned i = 0; i < 256; i++)
		//	p.colors[i] = 0xFF8000 + i;
	}

	void rebuild_pipe_quad_caches(struct instance& i, uint32_t color1, uint32_t color2, uint32_t color3,
		uint32_t color4)
	{
		uint32_t c[6];
		c[0] = color4;
		c[1] = color3;
		c[2] = color2;
		c[3] = color1;
		c[4] = color2;
		c[5] = color3;
		for(int x = 0; x < 7; x++)
			rebuild_pipe_quad_cache(i.pipecache[x], x - 3, c);
	}

	void draw_quad_y_pipe_last(struct instance& inst, double z, int x, bool top)
	{
		struct pipe_cache& p = inst.pipecache[x + 3];
		uint32_t fcolor = (!top && z < 0.2) ? 0x1000000 : 0;
		//We need these for slope projection.
		auto p1 = point_project_b(p.min_h, p.min_v, z);
		auto p2 = point_project_b(p.max_h, p.max_v, z);
		auto p3 = point_project_b(p.min_h, p.min_v, z - 1);
		auto p4 = point_project_b(p.max_h, p.max_v, z - 1);
		auto p5 = point_project_b(x - 0.5, 0, z);
		auto p6 = point_project_b(x, 1, z);
		auto p7 = point_project_b(x + 0.5, 0, z);
		if(p3.y - p1.y < 0.1)
			return;
		if(p4.y - p2.y < 0.1)
			return;
		double ymaxd = p5.y;
		int16_t ymax = ceil(ymaxd);
		double sl1 = (p3.x - p1.x) / (p3.y - p1.y);
		double sl2 = (p4.x - p2.x) / (p4.y - p2.y);
		double c1 = p1.x  - sl1 * p1.y;
		double c2 = p2.x - sl2 * p2.y;
		int16_t cheight = floor(p5.y - p6.y);
		int16_t cwidth = floor((p7.x - p5.x) / 2);
		int16_t ccenter = floor((p7.x + p5.x) / 2);
		for(signed j = ymax - cheight; j < ymax; j++) {
			if(j < 0 || j > inst.overlap_end)
				continue;
			int16_t nxs = floor(sl1 * j + c1);
			int16_t nxe = ceil(sl2 * j + c2);
			int16_t dstart = nxs;
			int16_t dend = nxe;
			if(nxs >= nxe)
				continue;
			uint32_t cstep = 255 * 65536 / (nxe - nxs + 1);
			uint32_t color = 0;
			if(true) {
				int16_t o = ymax - j;
				double o2 = 1.0 * o / cheight;
				int16_t w = cwidth * sqrt(1 - o2 * o2);
				dstart = ccenter - w;
				dend = ccenter + w;
			}
			dstart = max(max(dstart, nxs), (int16_t)0);
			dend = min(min(dend, nxe), (int16_t)FB_WIDTH);
			if(dstart > nxs)
				color += (dstart - nxs) * cstep;
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = dstart; i < dend; i++) {
					inst.framebuffer[base + i] = fcolor | p.colors[color >> 16];
					color += cstep;
				}
			else
				for(signed i = dstart; i < dend; i++) {
					framebuffer_blend2(inst.framebuffer[base + i], fcolor |
						p.colors[color >> 16]);
					color += cstep;
				}
		}
	}

	void draw_quad_y_pipe_first(struct instance& inst, double z1, double z2, int x, bool top)
	{
		struct pipe_cache& p = inst.pipecache[x + 3];
		uint32_t fcolor = (!top && z1 < 0) ? 0x1000000 : 0;
		auto p1 = point_project_b(p.min_h, p.min_v, z2);
		auto p2 = point_project_b(p.max_h, p.max_v, z2);
		auto p3 = point_project_b(p.min_h, p.min_v, z1);
		auto p4 = point_project_b(p.max_h, p.max_v, z1);
		auto p5 = point_project_b(x - 0.5, 0, z1);
		auto p6 = point_project_b(x, 1, z1);
		auto p7 = point_project_b(x + 0.5, 0, z1);
		double ymind = min(p1.y, p2.y);
		double ymaxd = p5.y;
		int16_t ymin = floor(ymind);
		int16_t ymax = ceil(ymaxd);
		int16_t ymin2 = ceil(max(p1.y, p2.y));
		if(p3.y - p1.y < 0.1)
			return;
		if(p4.y - p2.y < 0.1)
			return;
		double sl1 = (p3.x - p1.x) / (p3.y - p1.y);
		double sl2 = (p4.x - p2.x) / (p4.y - p2.y);
		double c1 = p1.x  - sl1 * p1.y;
		double c2 = p2.x - sl2 * p2.y;
		int16_t cheight = floor(p5.y - p6.y);
		int16_t cwidth = floor((p7.x - p5.x) / 2);
		int16_t ccenter = floor((p7.x + p5.x) / 2);
		bool hclip = false;
		uint16_t mindist = 65535;
		for(signed j = ymin; j < ymax; j++) {
			if(j < 0 || j > inst.overlap_end)
				continue;
			int16_t nxs = floor(sl1 * j + c1);
			int16_t nxe = ceil(sl2 * j + c2);
			int16_t dstart = nxs;
			int16_t dend = nxe;
			if(nxe <= nxs)
				continue;
			uint32_t cstep = 255 * 65536 / (nxe - nxs + 1);
			uint32_t color = 0;
			int16_t cstart = ccenter;
			int16_t cend = ccenter;
			if(j < ymin2 && p1.y != p2.y) {
				//The upper triangular region.
				if(p1.y < p2.y)
					dend = ceil((p2.x - p1.x) * (j - p1.y) / (p2.y - p1.y) + p1.x);
				else
					dstart = floor((p2.x - p1.x) * (j - p1.y) / (p2.y - p1.y) + p1.x);
			}
			if(j >= ymax - cheight) {
				int16_t o = ymax - j;
				double o2 = 1.0 * o / cheight;
				int16_t w = cwidth * sqrt(1 - o2 * o2);
				cstart -= w;
				cend += w;
				if(hclip) {
					if(x < 0)
						dstart = max((int16_t)dstart, cstart);
					if(x > 0)
						dend = min((int16_t)dend, cend);
				} else {
					uint16_t dist = 0;
					if(x < 0)
						dist = cstart - dstart;
					if(x > 0)
						dist = dend - cend;

					if(dist > mindist)
						hclip = true;
					else
						mindist = dist;
				}
			}
			dstart = max(max(dstart, nxs), (int16_t)0);
			dend = min(min(dend, nxe), (int16_t)FB_WIDTH);
			if(dstart > nxs)
				color += (dstart - nxs) * cstep;
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = dstart; i < dend; i++) {
					if(i < cstart || i >= cend)
						inst.framebuffer[base + i] = fcolor | p.colors[color >> 16];
					color += cstep;
				}
			else
				for(signed i = dstart; i < dend; i++) {
					if(i < cstart || i >= cend)
						framebuffer_blend2(inst.framebuffer[base + i], fcolor |
							p.colors[color >> 16]);
					color += cstep;
				}
		}
	}

	void draw_quad_y_pipe(struct instance& inst, double z1, double z2, int x, bool top)
	{
		struct pipe_cache& p = inst.pipecache[x + 3];
		uint32_t fcolor = (!top && z1 < 0.2) ? 0x1000000 : 0;
		auto p1 = point_project_b(p.min_h, p.min_v, z2);
		auto p2 = point_project_b(p.max_h, p.max_v, z2);
		auto p3 = point_project_b(p.min_h, p.min_v, z1);
		auto p4 = point_project_b(p.max_h, p.max_v, z1);
		double ymind = min(p1.y, p2.y);
		double ymaxd = max(p3.y, p4.y);
		int16_t ymin = floor(ymind);
		int16_t ymax = ceil(ymaxd) + 1;
		int16_t ymin2 = ceil(max(p1.y, p2.y));
		int16_t ymax2 = floor(min(p3.y, p4.y));
		if(p3.y - p1.y < 0.1)
			return;
		if(p4.y - p2.y < 0.1)
			return;
		double sl1 = (p3.x - p1.x) / (p3.y - p1.y);
		double sl2 = (p4.x - p2.x) / (p4.y - p2.y);
		double c1 = p1.x  - sl1 * p1.y;
		double c2 = p2.x - sl2 * p2.y;
		//FIXME: This leaves some graphical glitching in seams.
		for(signed j = ymin; j < ymax; j++) {
			if(j < 0 || j > inst.overlap_end)
				continue;
			int16_t nxs = floor(sl1 * j + c1);
			int16_t nxe = ceil(sl2 * j + c2);
			int16_t dstart = nxs;
			int16_t dend = nxe;
			if(nxe <= nxs)
				continue;
			uint32_t cstep = 255 * 65536 / (nxe - nxs + 1);
			uint32_t color = 0;
			if(j < ymin2 && p1.y != p2.y) {
				//The upper triangular region.
				if(p1.y < p2.y)
					dend = ceil((p2.x - p1.x) * (j - p1.y) / (p2.y - p1.y) + p1.x);
				else
					dstart = floor((p2.x - p1.x) * (j - p1.y) / (p2.y - p1.y) + p1.x);
			}
			if(j >= ymax2 && p3.y != p4.y) {
				//The lower triangular region.
				if(p3.y < p4.y)
					dstart = floor((p4.x - p3.x) * (j - p3.y - 1) / (p4.y - p3.y) + p3.x);
				else
					dend = ceil((p4.x - p3.x) * (j - p3.y - 1) / (p4.y - p3.y) + p3.x);
			}
			dstart = max(max(dstart, nxs), (int16_t)0);
			dend = min(min(dend, nxe), (int16_t)FB_WIDTH);
			if(dstart > nxs)
				color += (dstart - nxs) * cstep;
			size_t base = FB_WIDTH * j;
			if(j < inst.overlap_start)
				for(signed i = dstart; i < dend; i++) {
					inst.framebuffer[base + i] = fcolor | p.colors[color >> 16];
					color += cstep;
				}
			else
				for(signed i = dstart; i < dend; i++) {
					framebuffer_blend2(inst.framebuffer[base + i], fcolor | p.colors[color >> 16]);
					color += cstep;
				}
		}
	}

	void draw_level(struct instance& inst)
	{
		for(unsigned i = 0; i < inst.overlap_start; i++)
			for(unsigned j = 0; j < FB_WIDTH; j++)
				inst.framebuffer[i * FB_WIDTH + j] = inst.origbuffer[(i / FB_SCALE) * 320 +
					(j / FB_SCALE)];
		for(unsigned i = inst.overlap_start; i < inst.overlap_end; i++)
			for(unsigned j = 0; j < FB_WIDTH; j++)
				framebuffer_blend2(inst.framebuffer[i * FB_WIDTH + j], inst.origbuffer[
				(i / FB_SCALE) * 320 + (j / FB_SCALE)]);
		static const signed dorder[] = {-3, 3, -2, 2, -1, 1, 0};
		level& l = inst.state.curlevel;
		double zship = inst.state.p.lpos / 65536.0;
		double horizon = horizon_distance();
		double near = near_distance(inst);
		signed maxtile = ceil(zship + horizon);
		signed mintile = floor(zship + near - 1);
		for(signed zt = maxtile; zt >= mintile; zt--) {
			for(signed dxt = 0; dxt < 7; dxt++) {
				signed xt = dorder[dxt];
				tile t = l.at_tile(zt, xt);
				tile tleft = l.at_tile(zt, xt - 1);
				tile tright = l.at_tile(zt, xt + 1);
				tile tfront = l.at_tile(zt - 1, xt);
				tile tback = l.at_tile(zt + 1, xt);
				//First, draw the floor level.
				if(t.lower_floor() && (!t.is_rblock() || t.is_tunnel())) {
					bool ifront = (inst.state.p.vpos < 10240);
					ifront &= (xt >= 0 || inst.state.p.hpos < 5888 * xt + 32768 - 2944);
					ifront &= (xt <= 0 || inst.state.p.hpos > 5888 * xt + 32768 + 2944);
					draw_quad_y(inst, point_project_b(xt - 0.5, 0, zt + 1 - zship),
						point_project_b(xt + 0.5, 0, zt + 1 - zship),
						point_project_b(xt - 0.5, 0, zt - zship),
						point_project_b(xt + 0.5, 0, zt - zship),
						l.get_palette_color(top_palette[t.lower_floor()], ifront));
				}
				//Draw pipe shadow.
				if(t.is_tunnel() && !tfront.is_block()) {
					//Pipe shadow never obscures the ship.
					draw_quad_z_tunshadow(inst, point_project_b(xt - 0.5, 1, zt - zship),
						point_project_b(xt + 0.5, 1, zt - zship),
						point_project_b(xt - 0.5, 0, zt - zship),
						point_project_b(xt + 0.5, 0, zt - zship),
						l.get_palette_color(t.is_rblock() ? 65 : 67), xt);
				}
				//Draw the top surface.
				if(t.is_rblock()) {
					signed q = t.apparent_height();
					bool ifront = (inst.state.p.vpos < 10240 + q * 2560);
					ifront &= (xt >= 0 || inst.state.p.hpos < 5888 * xt + 32768 - 2944);
					ifront &= (xt <= 0 || inst.state.p.hpos > 5888 * xt + 32768 + 2944);
					ifront |= (l.in_pipe(zt * 65536, inst.state.p.hpos, inst.state.p.vpos) &&
						zt < zship);
					draw_quad_y(inst, point_project_b(xt - 0.5, q, zt + 1 - zship),
						point_project_b(xt + 0.5, q, zt + 1 - zship),
						point_project_b(xt - 0.5, q, zt - zship),
						point_project_b(xt + 0.5, q, zt - zship),
						l.get_palette_color(top_palette[t.upper_floor()], ifront));
				}
				//Left/Right block surface.
				if(t.is_rblock() && xt < 0 && tright.apparent_height() < t.apparent_height()) {
					signed q1 = tright.apparent_height();
					signed q2 = t.apparent_height();
					bool ifront = (inst.state.p.vpos < 10240 + q2 * 2560);
					ifront &= (inst.state.p.hpos < 5888 * xt + 32768 + 2944);
					draw_quad_x(inst, point_project_b(xt + 0.5, q2, zt - zship),
						point_project_b(xt + 0.5, q2, zt + 1 - zship),
						point_project_b(xt + 0.5, q1, zt - zship),
						point_project_b(xt + 0.5, q1, zt + 1 - zship),
						l.get_palette_color(63, ifront));
				}
				if(t.is_rblock() && xt > 0 && tleft.apparent_height() < t.apparent_height()) {
					signed q1 = tleft.apparent_height();
					signed q2 = t.apparent_height();
					bool ifront = (inst.state.p.vpos < 10240 + q2 * 2560);
					ifront &= (inst.state.p.hpos > 5888 * xt + 32768 - 2944);
					draw_quad_x(inst, point_project_b(xt - 0.5, q2, zt + 1 - zship),
						point_project_b(xt - 0.5, q2, zt - zship),
						point_project_b(xt - 0.5, q1, zt + 1- zship),
						point_project_b(xt - 0.5, q1, zt - zship),
						l.get_palette_color(64, ifront));
				}
				//Front block surface.
				if(t.is_rblock() && tfront.apparent_height() < t.apparent_height()) {
					signed q1 = tfront.apparent_height();
					signed q2 = t.apparent_height();
					bool ifront = (zt < zship);
					if(t.is_tunnel() && !tfront.is_block())
						draw_quad_z_tunnel(inst, point_project_b(xt - 0.5, q2, zt - zship),
							point_project_b(xt + 0.5, q2, zt - zship),
							point_project_b(xt - 0.5, q1, zt - zship),
							point_project_b(xt + 0.5, q1, zt - zship),
							l.get_palette_color(62, ifront));
					else {
						draw_quad_z(inst, point_project_b(xt - 0.5, q2, zt - zship),
							point_project_b(xt + 0.5, q2, zt - zship),
							point_project_b(xt - 0.5, q1, zt - zship),
							point_project_b(xt + 0.5, q1, zt - zship),
							l.get_palette_color(62, ifront));
					}
				}
				//Last tunnel block.
				if(t.is_tunnel() && !t.is_rblock()) {
					bool top = (inst.state.p.vpos > 10240);
					top &= !l.in_pipe(zt * 65536, inst.state.p.hpos, inst.state.p.vpos);
					if(tback.is_rblock() || !tback.is_block())
						draw_quad_y_pipe_last(inst, zt + 1 - zship, xt, top);
					//Standalone tunnel top.
					if(tfront.is_block())
						draw_quad_y_pipe(inst, zt - zship, zt + 1 - zship, xt, top);
					else
						draw_quad_y_pipe_first(inst, zt - zship, zt + 1 - zship, xt, top);
					//Standalone tunnel front.
					if(!tfront.is_block()) {
						bool ifront = (zt < zship);
						draw_quad_z_pipefront(inst, zt - zship, xt, l.get_palette_color(66,
							ifront));
					}
				}
				//Left/Right floor surface.
				if(xt < 0 && t.lower_floor() && !tright.lower_floor()) {
					bool ifront = (inst.state.p.vpos < 10240);
					ifront &= (inst.state.p.hpos > 5888 * xt + 32768);
					draw_quad_x(inst, point_project_b(xt + 0.5, 0, zt - zship),
						point_project_b(xt + 0.5, 0, zt + 1 - zship),
						point_project_b(xt + 0.5, -fthickness, zt - zship),
						point_project_b(xt + 0.5, -fthickness, zt + 1 - zship),
						l.get_palette_color(right_palette[t.lower_floor()]));
				}
				if(xt > 0 && t.lower_floor() && !tleft.lower_floor()) {
					bool ifront = (inst.state.p.vpos < 10240);
					ifront &= (inst.state.p.hpos > 5888 * xt + 32768);
					draw_quad_x(inst, point_project_b(xt - 0.5, 0, zt + 1 - zship),
						point_project_b(xt - 0.5, 0, zt - zship),
						point_project_b(xt - 0.5, -fthickness, zt + 1- zship),
						point_project_b(xt - 0.5, -fthickness, zt - zship),
						l.get_palette_color(left_palette[t.lower_floor()], ifront));
				}
				//Front floor surface.
				if(t.lower_floor() && !tfront.lower_floor()) {
					bool ifront = (inst.state.p.vpos < 10240);
					ifront &= (inst.state.p.hpos < 5888 * xt + 32768);
					draw_quad_z(inst, point_project_b(xt - 0.5, 0, zt - zship),
						point_project_b(xt + 0.5, 0, zt - zship),
						point_project_b(xt - 0.5, -fthickness, zt - zship),
						point_project_b(xt + 0.5, -fthickness, zt - zship),
						l.get_palette_color(front_palette[t.lower_floor()], ifront));
				}
			}
		}
		draw_sprite(inst, (inst.state.p.hpos - 32768.0) / 5888.0, (inst.state.p.vpos - 10240.0) / 2560.0,
			ship_sprite(inst.state));
	}

	const char* const period = "%&(ccK";
	const char* const dash = "%'(cScS";
	const char* const vline = "%$(b";
	const char* const tback = "%F*ccccccccccccccccccccccccccccccccccccccccc";

	void draw_timeattack_time(struct instance& inst, const char* msg)
	{
		uint16_t w = 321;
		uint16_t nst = strlen(_numbers_g) / 10;
		draw_block2(inst, tback, 0, 0xFFFFFF, 0xFFFFFF, false);
		while(*msg) {
			if(*msg >= '0' && *msg <= '9') {
				draw_block2(inst, _numbers_g + (*msg - '0') * nst, w, 0xFFFFFF, 0xFFFFFF, true);
				draw_block2(inst, vline, w + 4, 0xFFFFFF, 0xFFFFFF, true);
				w += 5;
			} else if(*msg == ':') {
				draw_block2(inst, period, w, 0xFFFFFF, 0xFFFFFF, true);
				w += 3;
			} else if(*msg == '-') {
				draw_block2(inst, dash, w, 0xFFFFFF, 0xFFFFFF, true);
				draw_block2(inst, vline, w + 4, 0xFFFFFF, 0xFFFFFF, true);
				w += 5;
			}
			msg++;
		}
		for(unsigned i = 0; i < 7; i++)
			for(unsigned j = 0; j < 35; j++)
				inst.origbuffer[320 * i + j] ^= 0xFFFFFF;
		render_framebuffer_update(inst, 0, 0, 35, 7);
	}

	void draw_timeattack_time(struct instance& inst, uint16_t frames)
	{
		char msg[8];
		if(frames > 64807) {
			strcpy(msg, "----:--");
		} else {
			unsigned seconds = 18227 * frames / 656250;
			unsigned subseconds = 36454U * frames / 13125 - 100 * seconds;
			sprintf(msg, "%u:%02u", seconds, subseconds);
		}
		draw_timeattack_time(inst, msg);
	}
}
