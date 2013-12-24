#include "framebuffer.hpp"

namespace
{
	struct h_rotate : public framebuffer::color_mod
	{
		h_rotate(const std::string& name, int steps)
			: framebuffer::color_mod(name, [steps](int64_t& v) {
				v = framebuffer::color_rotate_hue(v, steps, 24);
			})
		{
		}
	};
	struct s_adjust : public framebuffer::color_mod
	{
		s_adjust(const std::string& name, int steps)
			: framebuffer::color_mod(name, [steps](int64_t& v) {
				v = framebuffer::color_adjust_saturation(v, steps / 16.0);
			})
		{
		}
	};
	struct l_adjust : public framebuffer::color_mod
	{
		l_adjust(const std::string& name, int steps)
			: framebuffer::color_mod(name, [steps](int64_t& v) {
				v = framebuffer::color_adjust_lightness(v, steps / 16.0);
			})
		{
		}
	};
	struct hsl : public framebuffer::basecolor
	{
		hsl(const std::string& name, int16_t h, int16_t s, uint16_t l)
			: framebuffer::basecolor(name, getcolor(h, s, l))
		{
		}
		int64_t getcolor(int16_t h, int16_t s, uint16_t l)
		{
			int16_t m2;
			if(l <= 128) m2 = l * (s + 256) / 256;
			else m2 = l + s - l * s / 256;
			int16_t m1 = l * 2 - m2;
			m1 -= (m1 >> 7);
			m2 -= (m2 >> 7);
			int64_t r = huev(h + 510, m1, m2);
			int64_t g = huev(h      , m1, m2);
			int64_t b = huev(h - 510, m1, m2);
			return (r << 16) | (g << 8) | (b << 0);
		}
		uint8_t huev(int16_t h, int16_t m1, int16_t m2)
		{
			if(h < 0) h += 1530;
			if(h > 1536) h -= 1530;
			if(h < 256) return m1 + (m2 - m1) * h / 255;
			if(h < 765) return m2;
			if(h < 1020) return m1 + (m2 - m1) * (1020 - h) / 255;
			return m1;
		}
	};
	struct hsl_luminance
	{
		hsl_luminance(const std::string& name, int16_t h, int16_t s)
			: a0(name + "0", h, s, 0)
			, a1(name + "1", h, s, 32)
			, a2(name + "2", h, s, 64)
			, a3(name + "3", h, s, 96)
			, a4(name + "4", h, s, 128)
			, a5(name + "5", h, s, 160)
			, a6(name + "6", h, s, 192)
			, a7(name + "7", h, s, 224)
			, a8(name + "8", h, s, 256)
		{
		}
	private:
		hsl a0;
		hsl a1;
		hsl a2;
		hsl a3;
		hsl a4;
		hsl a5;
		hsl a6;
		hsl a7;
		hsl a8;
	};
	struct hsl_saturation
	{
		hsl_saturation(const std::string& name, int16_t h)
			: a0(name + "0", h, 0)
			, a1(name + "1", h, 32)
			, a2(name + "2", h, 64)
			, a3(name + "3", h, 96)
			, a4(name + "4", h, 128)
			, a5(name + "5", h, 160)
			, a6(name + "6", h, 192)
			, a7(name + "7", h, 224)
			, a8(name + "8", h, 256)
		{
		}
	private:
		hsl_luminance a0;
		hsl_luminance a1;
		hsl_luminance a2;
		hsl_luminance a3;
		hsl_luminance a4;
		hsl_luminance a5;
		hsl_luminance a6;
		hsl_luminance a7;
		hsl_luminance a8;
	};
	struct hsl_hue
	{
		hsl_hue(const std::string& name)
			: a0(name + "r", 0)
			, a1(name + "ry", 128)
			, a1b(name + "o", 128)
			, a2(name + "y", 255)
			, a3(name + "yg", 383)
			, a4(name + "g", 510)
			, a5(name + "gc", 638)
			, a6(name + "c", 765)
			, a7(name + "cb", 893)
			, a8(name + "b", 1020)
			, a9(name + "bm", 1148)
			, a10(name + "m", 1275)
			, a11(name + "mr", 1403)
		{
		}
	private:
		hsl_saturation a0;
		hsl_saturation a1;
		hsl_saturation a1b;
		hsl_saturation a2;
		hsl_saturation a3;
		hsl_saturation a4;
		hsl_saturation a5;
		hsl_saturation a6;
		hsl_saturation a7;
		hsl_saturation a8;
		hsl_saturation a9;
		hsl_saturation a10;
		hsl_saturation a11;
	};

	hsl_hue hsls("hsl-");
	h_rotate hrotatep1("hue+1", 1);
	h_rotate hrotatep2("hue+2", 2);
	h_rotate hrotatep3("hue+3", 3);
	h_rotate hrotatep4("hue+4", 4);
	h_rotate hrotatep5("hue+5", 5);
	h_rotate hrotatep6("hue+6", 6);
	h_rotate hrotatep7("hue+7", 7);
	h_rotate hrotatep8("hue+8", 8);
	h_rotate hrotatep9("hue+9", 9);
	h_rotate hrotatep10("hue+10", 10);
	h_rotate hrotatep11("hue+11", 11);
	h_rotate hrotatep12("hue+12", 12);
	h_rotate hrotatep13("hue+13", 13);
	h_rotate hrotatep14("hue+14", 14);
	h_rotate hrotatep15("hue+15", 15);
	h_rotate hrotatep16("hue+16", 16);
	h_rotate hrotatep17("hue+17", 17);
	h_rotate hrotatep18("hue+18", 18);
	h_rotate hrotatep19("hue+19", 19);
	h_rotate hrotatep20("hue+20", 20);
	h_rotate hrotatep21("hue+21", 21);
	h_rotate hrotatep22("hue+22", 22);
	h_rotate hrotatep23("hue+23", 23);
	h_rotate hrotatem1("hue-1", -1);
	h_rotate hrotatem2("hue-2", -2);
	h_rotate hrotatem3("hue-3", -3);
	h_rotate hrotatem4("hue-4", -4);
	h_rotate hrotatem5("hue-5", -5);
	h_rotate hrotatem6("hue-6", -6);
	h_rotate hrotatem7("hue-7", -7);
	h_rotate hrotatem8("hue-8", -8);
	h_rotate hrotatem9("hue-9", -9);
	h_rotate hrotatem10("hue-10", -10);
	h_rotate hrotatem11("hue-11", -11);
	h_rotate hrotatem12("hue-12", -12);
	h_rotate hrotatem13("hue-13", -13);
	h_rotate hrotatem14("hue-14", -14);
	h_rotate hrotatem15("hue-15", -15);
	h_rotate hrotatem16("hue-16", -16);
	h_rotate hrotatem17("hue-17", -17);
	h_rotate hrotatem18("hue-18", -18);
	h_rotate hrotatem19("hue-19", -19);
	h_rotate hrotatem20("hue-20", -20);
	h_rotate hrotatem21("hue-21", -21);
	h_rotate hrotatem22("hue-22", -22);
	h_rotate hrotatem23("hue-23", -23);

	s_adjust sadjustp1("saturation+1", 1);
	s_adjust sadjustp2("saturation+2", 2);
	s_adjust sadjustp3("saturation+3", 3);
	s_adjust sadjustp4("saturation+4", 4);
	s_adjust sadjustp5("saturation+5", 5);
	s_adjust sadjustp6("saturation+6", 6);
	s_adjust sadjustp7("saturation+7", 7);
	s_adjust sadjustp8("saturation+8", 8);
	s_adjust sadjustp9("saturation+9", 9);
	s_adjust sadjustp10("saturation+10", 10);
	s_adjust sadjustp11("saturation+11", 11);
	s_adjust sadjustp12("saturation+12", 12);
	s_adjust sadjustp13("saturation+13", 13);
	s_adjust sadjustp14("saturation+14", 14);
	s_adjust sadjustp15("saturation+15", 15);
	s_adjust sadjustp16("saturation+16", 16);
	s_adjust sadjustm1("saturation-1", -1);
	s_adjust sadjustm2("saturation-2", -2);
	s_adjust sadjustm3("saturation-3", -3);
	s_adjust sadjustm4("saturation-4", -4);
	s_adjust sadjustm5("saturation-5", -5);
	s_adjust sadjustm6("saturation-6", -6);
	s_adjust sadjustm7("saturation-7", -7);
	s_adjust sadjustm8("saturation-8", -8);
	s_adjust sadjustm9("saturation-9", -9);
	s_adjust sadjustm10("saturation-10", -10);
	s_adjust sadjustm11("saturation-11", -11);
	s_adjust sadjustm12("saturation-12", -12);
	s_adjust sadjustm13("saturation-13", -13);
	s_adjust sadjustm14("saturation-14", -14);
	s_adjust sadjustm15("saturation-15", -15);
	s_adjust sadjustm16("saturation-16", -16);

	l_adjust ladjustp1("lightness+1", 1);
	l_adjust ladjustp2("lightness+2", 2);
	l_adjust ladjustp3("lightness+3", 3);
	l_adjust ladjustp4("lightness+4", 4);
	l_adjust ladjustp5("lightness+5", 5);
	l_adjust ladjustp6("lightness+6", 6);
	l_adjust ladjustp7("lightness+7", 7);
	l_adjust ladjustp8("lightness+8", 8);
	l_adjust ladjustp9("lightness+9", 9);
	l_adjust ladjustp10("lightness+10", 10);
	l_adjust ladjustp11("lightness+11", 11);
	l_adjust ladjustp12("lightness+12", 12);
	l_adjust ladjustp13("lightness+13", 13);
	l_adjust ladjustp14("lightness+14", 14);
	l_adjust ladjustp15("lightness+15", 15);
	l_adjust ladjustp16("lightness+16", 16);
	l_adjust ladjustm1("lightness-1", -1);
	l_adjust ladjustm2("lightness-2", -2);
	l_adjust ladjustm3("lightness-3", -3);
	l_adjust ladjustm4("lightness-4", -4);
	l_adjust ladjustm5("lightness-5", -5);
	l_adjust ladjustm6("lightness-6", -6);
	l_adjust ladjustm7("lightness-7", -7);
	l_adjust ladjustm8("lightness-8", -8);
	l_adjust ladjustm9("lightness-9", -9);
	l_adjust ladjustm10("lightness-10", -10);
	l_adjust ladjustm11("lightness-11", -11);
	l_adjust ladjustm12("lightness-12", -12);
	l_adjust ladjustm13("lightness-13", -13);
	l_adjust ladjustm14("lightness-14", -14);
	l_adjust ladjustm15("lightness-15", -15);
	l_adjust ladjustm16("lightness-16", -16);
}
