#include "curve25519.hpp"
#include <cstring>
#include <iostream>
#include <iomanip>

namespace
{
	void zeroize(void* ptr, size_t size)
	{
		//Whee... Do it like OpenSSL/GnuTLS.
		volatile char* vptr = (volatile char*)ptr;
		volatile size_t vidx = 0;
		do { memset(ptr, 0, size); } while(vptr[vidx]);
	}

#if defined(__x86_64__)
	//Generic (slow).
	struct element
	{
		typedef uint64_t smallval_t;
		typedef uint64_t cond_t;
		typedef uint64_t limb_t;
		typedef __uint128_t wide_t;
		const static int shift = 51;
		const static limb_t mask = (1ULL << shift) - 1;
		//a^(2^count) -> self
		inline void square(const element& a, unsigned count = 1)
		{
			wide_t s[5];
			memcpy(n, a.n, sizeof(n));
			for(unsigned i = 0; i < count; i++) {
				s[0] = (wide_t)n[0] * (wide_t)n[0] +
					(wide_t)(n[1] << 1) * (wide_t)(n[4] * 19) +
					(wide_t)(n[2] << 1) * (wide_t)(n[3] * 19);
				s[1] = (wide_t)n[0] * (wide_t)(n[1] << 1) +
					(wide_t)(n[2] << 1) * (wide_t)(n[4] * 19) +
					(wide_t)n[3] * (wide_t)(n[3] * 19);
				s[2] = ((wide_t)n[0] * (wide_t)(n[2] << 1) +
					(wide_t)n[1] * (wide_t)n[1]) +
					(wide_t)(n[3] << 1) * (wide_t)(n[4] * 19);
				s[3] = ((wide_t)n[0] * (wide_t)n[3] << 1) +
					((wide_t)n[1] * (wide_t)n[2] << 1) +
					((wide_t)n[4] * (wide_t)(n[4] * 19));
				s[4] = ((wide_t)n[0] * (wide_t)n[4] << 1) +
					((wide_t)n[1] * (wide_t)n[3] << 1) +
					((wide_t)n[2] * (wide_t)n[2]);
				s[1] += (s[0] >> shift);
				s[2] += (s[1] >> shift);
				n[2] = (limb_t)s[2] & mask;
				s[3] += (s[2] >> shift);
				n[3] = (limb_t)s[3] & mask;
				s[4] += (s[3] >> shift);
				n[4] = (limb_t)s[4] & mask;
				s[0] = ((limb_t)s[0] & mask) + 19 * (limb_t)(s[4] >> shift);
				n[0] = (limb_t)s[0] & mask;
				s[1] = ((limb_t)s[1] & mask) + (limb_t)(s[0] >> shift);
				n[1] = (limb_t)s[1];
			}
			zeroize(s, sizeof(s));
		}
		//a * b -> self
		inline void multiply(const element& a, const element& b)
		{
			wide_t s[5];
			s[0] = (wide_t)a.n[0] * (wide_t)b.n[0] + (wide_t)(a.n[4] * 19) * (wide_t)b.n[1] +
				(wide_t)(a.n[3] * 19) * (wide_t)b.n[2] + (wide_t)a.n[2] * (wide_t)(b.n[3] * 19) +
				(wide_t)a.n[1] * (wide_t)(b.n[4] * 19);
			s[1] = (wide_t)a.n[0] * (wide_t)b.n[1] + (wide_t)a.n[1] * (wide_t)b.n[0] +
				 (wide_t)(a.n[4] * 19) * (wide_t)b.n[2] + (wide_t)(a.n[3] * 19) * (wide_t)b.n[3] +
				 (wide_t)a.n[2] * (wide_t)(b.n[4] * 19);
			s[2] = (wide_t)a.n[0] * (wide_t)b.n[2] + (wide_t)a.n[1] * (wide_t)b.n[1] +
				(wide_t)a.n[2] * (wide_t)b.n[0] + (wide_t)(a.n[4] * 19) * (wide_t)b.n[3] +
				(wide_t)(a.n[3] * 19) * (wide_t)b.n[4];
			s[3] = (wide_t)a.n[0] * (wide_t)b.n[3] + (wide_t)a.n[1] * (wide_t)b.n[2] +
				(wide_t)a.n[2] * (wide_t)b.n[1] + (wide_t)a.n[3] * (wide_t)b.n[0] +
				(wide_t)(a.n[4] * 19) * (wide_t)b.n[4];
			s[4] = (wide_t)a.n[0] * (wide_t)b.n[4] + (wide_t)a.n[1] * (wide_t)b.n[3] +
				(wide_t)a.n[2] * (wide_t)b.n[2] + (wide_t)a.n[3] * (wide_t)b.n[1] +
				(wide_t)a.n[4] * (wide_t)b.n[0];
			s[1] += (s[0] >> shift);
			s[2] += (s[1] >> shift);
			n[2] = (limb_t)s[2] & mask;
			s[3] += (s[2] >> shift);
			n[3] = (limb_t)s[3] & mask;
			s[4] += (s[3] >> shift);
			n[4] = (limb_t)s[4] & mask;
			s[0] = ((limb_t)s[0] & mask) + 19 * (limb_t)(s[4] >> shift);
			n[0] = (limb_t)s[0] & mask;
			s[1] = ((limb_t)s[1] & mask) + (limb_t)(s[0] >> shift);
			n[1] = (limb_t)s[1];
			zeroize(s, sizeof(s));
		}
		//e - self -> self
		inline void diff_back(const element& e)
		{
			limb_t C1 = 2 * mask - 2 * (19 - 1);
			limb_t C2 = 2 * mask;
			n[0] = e.n[0] + C1 - n[0];
			for(unsigned i = 1; i < 5; i++)
				n[i] = e.n[i] + C2 - n[i];
			limb_t carry = 0;
			for(unsigned i = 0; i < 5; i++) {
				n[i] += carry;
				carry = n[i] >> shift;
				n[i] &= mask;
			}
			carry *= 19;
			n[0] += carry;
		}
		//a * b -> self (with constant b).
		inline void multiply(const element& a, smallval_t b)
		{
			limb_t carry = 0;
			for(unsigned i = 0; i < 5; i++) {
				wide_t x = (wide_t)a.n[i] * b + carry;
				n[i] = x & mask;
				carry = x >> shift;
			}
			carry *= 19;
			n[0] += carry;
		}
		//Reduce mod 2^255-19 and store to buffer.
		inline void store(uint8_t* buffer)
		{
			limb_t carry = 19;
			for(int i = 0; i < 5; i++) {
				n[i] = n[i] + carry;
				carry = n[i] >> shift;
				n[i] = n[i] & mask;
			}
			carry = 19 - carry * 19;
			for(int i = 0; i < 5; i++) {
				n[i] = n[i] - carry;
				carry = (n[i] >> shift) & 1;
				n[i] = n[i] & mask;
			}
			for(unsigned i = 0; i < 32; i++) {
				buffer[i] = n[8 * i / shift] >> (8 * i % shift);
				if(8 * i % shift > shift - 8 && i < 26)
					buffer[i] |= n[8 * i / shift + 1] << (shift - 8 * i % shift);
			}
		}
		//Load from buffer.
		inline explicit element(const uint8_t* buffer)
		{
			memset(n, 0, sizeof(n));
			for(unsigned i = 0; i < 32; i++) {
				n[8 * i / shift] |= (limb_t)buffer[i] << (8 * i % shift);
				n[8 * i / shift] &= mask;
				if(8 * i % shift > shift - 8 && i < 26) {
					n[8 * i / shift + 1] |= (limb_t)buffer[i] >> (shift - 8 * i % shift);
				}
			}
		}
		//Construct 0.
		inline element()
		{
			memset(n, 0, sizeof(n));
		}
		//Construct small value.
		inline element(smallval_t sval)
		{
			memset(n, 0, sizeof(n));
			n[0] = sval;
		}
		//self + e -> self.
		inline void sum(const element& e)
		{
			limb_t carry = 0;
			for(int i = 0; i < 5; i++) {
				n[i] = n[i] + e.n[i] + carry;
				carry = n[i] >> shift;
				n[i] = n[i] & mask;
			}
			n[0] += carry * 19;
		}
		//If condition=1, swap self,e.
		inline void swap_cond(element& e, cond_t condition)
		{
			condition = -condition;
			for(int i = 0; i < 5; i++) {
				limb_t t = condition & (n[i] ^ e.n[i]);
				n[i] ^= t;
				e.n[i] ^= t;
			}
		}
		inline ~element()
		{
			zeroize(n, sizeof(n));
		}
		void debug(const char* pfx) const
		{
			uint8_t buf[34];
			std::cerr << pfx << ": ";
			memset(buf, 0, 34);
			for(unsigned i = 0; i < 5*64; i++) {
				unsigned rbit = shift*(i>>6)+(i&63);
				if((n[i>>6] >> (i&63)) & 1)
					buf[rbit>>3]|=(1<<(rbit&7));
			}
			for(unsigned i = 33; i < 34; i--)
				std::cerr << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
					<< (int)buf[i];
			std::cerr << std::endl;
		}
	private:
		limb_t n[5];
	};
#else
	//Generic (slow).
	struct element
	{
		typedef uint32_t smallval_t;
		typedef uint32_t cond_t;
		//a^(2^count) -> self
		inline void square(const element& a, unsigned count = 1)
		{
			uint64_t x[20];
			uint32_t t[10];
			memcpy(t, a.n, sizeof(t));
			for(unsigned c = 0; c < count; c++) {
				memset(x, 0, sizeof(x));
				for(unsigned i = 0; i < 10; i++) {
					x[i + i] += (uint64_t)t[i] * t[i];
					for(unsigned j = 0; j < i; j++)
						x[i + j] += ((uint64_t)t[i] * t[j]) << 1;
				}
/*
				for(unsigned i = 0; i < 20; i++) {
					std::cerr << "2^" << std::hex << std::uppercase << 26*i << "*"
						<< std::hex << std::uppercase << x[i] << "+";
				}
				std::cerr << "0" << std::endl;
*/
				//Multiplication by 608 can overflow, so reduce these.
				uint64_t carry2 = 0;
				for(unsigned i = 0; i < 20; i++) {
					x[i] += carry2;
					carry2 = x[i] >> 26;
					x[i] &= 0x3FFFFFF;
				}
				x[19] += (carry2 << 26);
/*
				for(unsigned i = 0; i < 20; i++) {
					std::cerr << "2^" << std::hex << std::uppercase << 26*i << "*"
						<< std::hex << std::uppercase << x[i] << "+";
				}
				std::cerr << "0" << std::endl;
*/
				//Reduce and fold.
				uint64_t carry = 0;
				for(unsigned i = 0; i < 10; i++) {
					x[i] = x[i] + x[10 + i] * 608 + carry;
					carry = x[i] >> 26;
					x[i] &= 0x3FFFFFF;
				}
				//Final reduction.
				x[0] += carry * 608;

				for(unsigned i = 0; i < 10; i++)
					t[i] = x[i];
			}
			memcpy(n, t, sizeof(n));
			zeroize(x, sizeof(x));
			zeroize(t, sizeof(t));
		}
		//a * b -> self
		inline void multiply(const element& a, const element& b)
		{
			uint64_t x[20];
			memset(x, 0, sizeof(x));
			for(unsigned i = 0; i < 10; i++)
				for(unsigned j = 0; j < 10; j++)
					x[i + j] += (uint64_t)a.n[i] * b.n[j];

			//Multiplication by 608 can overflow, so reduce these.
			uint64_t carry2 = 0;
			for(unsigned i = 9; i < 20; i++) {
				x[i] += carry2;
				carry2 = x[i] >> 26;
				x[i] &= 0x3FFFFFF;
			}
			x[19] += (carry2 << 26);

			//Reduce and fold.
			uint64_t carry = 0;
			for(unsigned i = 0; i < 10; i++) {
				x[i] = x[i] + x[10 + i] * 608 + carry;
				carry = x[i] >> 26;
				x[i] &= 0x3FFFFFF;
			}
			//Final reduction.
			x[0] += carry * 608;
			for(unsigned i = 0; i < 10; i++)
				n[i] = x[i];
			zeroize(x, sizeof(x));
		}
		//e - self -> self
		inline void diff_back(const element& e)
		{
			uint32_t C1 = (1<<28)-2432;
			uint32_t C2 = (1<<28)-4;
			n[0] = e.n[0] + C1 - n[0];
			for(unsigned i = 1; i < 10; i++)
				n[i] = e.n[i] + C2 - n[i];
			uint32_t carry = 0;
			for(unsigned i = 0; i < 10; i++) {
				n[i] += carry;
				carry = n[i] >> 26;
				n[i] &= 0x3FFFFFF;
			}
			n[9] |= (carry << 26);
		}
		//a * b -> self (with constant b).
		inline void multiply(const element& a, smallval_t b)
		{
			uint64_t carry = 0;
			for(unsigned i = 0; i < 10; i++) {
				uint64_t x = (uint64_t)a.n[i] * b + carry;
				n[i] = x & 0x3FFFFFF;
				carry = x >> 26;
			}
			carry = ((carry << 5) | (n[9] >> 21)) * 19;
			n[9] &= 0x1FFFFF;
			n[0] += carry;
		}
		//Reduce mod 2^255-19 and store to buffer.
		inline void store(uint8_t* buffer)
		{
			uint32_t carry = (n[9] >> 21) * 19 + 19;
			n[9] &= 0x1FFFFF;
			for(int i = 0; i < 10; i++) {
				n[i] = n[i] + carry;
				carry = n[i] >> 26;
				n[i] = n[i] & 0x3FFFFFF;
			}
			carry = 19 - (n[9] >> 21) * 19;
			for(int i = 0; i < 10; i++) {
				n[i] = n[i] - carry;
				carry = (n[i] >> 26) & 1;
				n[i] = n[i] & 0x3FFFFFF;
			}
			n[9] &= 0x1FFFFF;
			for(unsigned i = 0; i < 32; i++) {
				buffer[i] = n[8 * i / 26] >> (8 * i % 26);
				if(8 * i % 26 > 18)
					buffer[i] |= n[8 * i / 26 + 1] << (26 - 8 * i % 26);
			}
		}
		//Load from buffer.
		inline explicit element(const uint8_t* buffer)
		{
			memset(n, 0, sizeof(n));
			for(unsigned i = 0; i < 32; i++) {
				n[8 * i / 26] |= (uint32_t)buffer[i] << (8 * i % 26);
				n[8 * i / 26] &= 0x3FFFFFF;
				if(8 * i % 26 > 18) {
					n[8 * i / 26 + 1] |= (uint32_t)buffer[i] >> (26 - 8 * i % 26);
				}
			}
		}
		//Construct 0.
		inline element()
		{
			memset(n, 0, sizeof(n));
		}
		//Construct small value.
		inline element(smallval_t sval)
		{
			memset(n, 0, sizeof(n));
			n[0] = sval;
		}
		//self + e -> self.
		inline void sum(const element& e)
		{
			uint32_t carry = 0;
			for(int i = 0; i < 10; i++) {
				n[i] = n[i] + e.n[i] + carry;
				carry = n[i] >> 26;
				n[i] = n[i] & 0x3FFFFFF;
			}
			n[0] += carry * 608;
		}
		//If condition=1, swap self,e.
		inline void swap_cond(element& e, cond_t condition)
		{
			condition = -condition;
			for(int i = 0; i < 10; i++) {
				uint32_t t = condition & (n[i] ^ e.n[i]);
				n[i] ^= t;
				e.n[i] ^= t;
			}
		}
		inline ~element()
		{
			zeroize(n, sizeof(n));
		}
		void debug(const char* pfx) const
		{
			uint8_t buf[34];
			std::cerr << pfx << ": ";
			memset(buf, 0, 34);
			for(unsigned i = 0; i < 10*32; i++) {
				unsigned rbit = 26*(i>>5)+(i&31);
				if((n[i>>5] >> (i&31)) & 1)
					buf[rbit>>3]|=(1<<(rbit&7));
			}
			for(unsigned i = 33; i < 34; i--)
				std::cerr << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
					<< (int)buf[i];
			std::cerr << std::endl;
		}
	private:
		uint32_t n[10];
	};
#endif
}

static void montgomery(element& dblx, element& dblz, element& sumx, element& sumz,
	element& ax, element& az, element& bx, element& bz, const element& diff)
{
	element tmp;
	element oax = ax;
	ax.sum(az);
	az.diff_back(oax);
	element obx = bx;
	bx.sum(bz);
	bz.diff_back(obx);
	oax.multiply(az, bx);
	obx.multiply(ax, bz);
	bx.square(ax);
	bz.square(az);
	dblx.multiply(bx, bz);
	bz.diff_back(bx);
	tmp.multiply(bz, 121665);
	bx.sum(tmp);
	dblz.multiply(bx, bz);
	bx = oax;
	oax.sum(obx);
	obx.diff_back(bx);
	sumx.square(oax);
	bz.square(obx);
	sumz.multiply(bz, diff);
}

static void cmultiply(element& ox, element& oz, const uint8_t* key, const element& base)
{
	element x1a(1), z1a, x2a(base), z2a(1), x1b, z1b(1), x2b, z2b(1);

	element::cond_t lbit = 0;
	for(unsigned i = 31; i < 32; i--) {
		uint8_t x = key[i];
		for(unsigned j = 0; j < 4; j++) {
			element::cond_t bit = (x >> 7);
			x1a.swap_cond(x2a, bit ^ lbit);
			z1a.swap_cond(z2a, bit ^ lbit);
			montgomery(x1b, z1b, x2b, z2b, x1a, z1a, x2a, z2a, base);
			lbit = bit;
			x <<= 1;
			bit = (x >> 7);
			x1b.swap_cond(x2b, bit ^ lbit);
			z1b.swap_cond(z2b, bit ^ lbit);
			montgomery(x1a, z1a, x2a, z2a, x1b, z1b, x2b, z2b, base);
			x <<= 1;
			lbit = bit;
		}
	}
	x1a.swap_cond(x2a, lbit);
	z1a.swap_cond(z2a, lbit);
	ox = x1a;
	oz = z1a;
};

static void invert(element& out, const element& in)
{
	element r, y, g, b, c;
	y.square(in);
	g.square(y, 2);
	b.multiply(g, in);
	r.multiply(b, y);
	y.square(r);
	g.multiply(y,b);
	y.square(g, 5);
	b.multiply(y, g);
	y.square(b, 10);
	g.multiply(y, b);
	y.square(g, 20);
	c.multiply(y, g);
	g.square(c, 10);
	y.multiply(g, b);
	b.square(y, 50);
	g.multiply(y, b);
	b.square(g, 100);
	c.multiply(g, b);
	g.square(c, 50);
	b.multiply(y, g);
	y.square(b, 5);
	out.multiply(r, y);
}

void curve25519(uint8_t* _out, const uint8_t* key, const uint8_t* _base)
{
	element base(_base), outx, outz, zinv;
	cmultiply(outx, outz, key, base);
	invert(zinv, outz);
	outz.multiply(outx, zinv);
	outz.store(_out);
}

void curve25519_clamp(uint8_t* key)
{
	key[0] &= 0xF8;
	key[31] &= 0x7F;
	key[31] |= 0x40;
}

const uint8_t curve25519_base[32] = {9};

#ifdef CURVE25519_VALGRIND_TEST

int main()
{
	uint8_t buf[128];
	curve25519(buf+64, buf, buf+32);
	std::cerr << buf[64] << std::endl;
	return 0;
}

#endif

#ifdef CURVE25519_TEST_MODE
#include <cmath>
uint64_t arch_get_tsc()
{
	uint32_t a, b;
	asm volatile("rdtsc" : "=a"(a), "=d"(b));
	return ((uint64_t)b << 32) | a;
}

//For comparision
extern "C"
{
int curve25519_donna(uint8_t *mypublic, const uint8_t *secret, const uint8_t *basepoint);
}

int main()
{
	uint8_t buf[128] = {0};
	FILE* fd = fopen("/dev/urandom", "rb");
	uint64_t ctr = 0;
	uint64_t _t;
	double tsum;
	double tsqr;
	uint64_t tmin = 999999999;

	buf[32] = 9;
	fread(buf, 1, 32, fd);
	buf[0] &= 248;
	buf[31] &= 127;
	buf[31] |= 64;

	for(unsigned i = 0; i < 32768; i++) {
		_t = arch_get_tsc();
		curve25519(buf+64, buf, buf+32);
		_t = arch_get_tsc() - _t;
		tsum += _t;
		tsqr += _t * _t;
		if(_t < tmin) tmin = _t;
	}
	tsum /= 32768;
	tsqr /= 32768;
	std::cerr << "Time: " << tsum << "+-" << sqrt(tsqr - tsum * tsum) << " >=" << tmin << std::endl;
	while(true) {
		fread(buf, 1, 32, fd);
		buf[0] &= 248;
		buf[31] &= 127;
		buf[31] |= 64;
		curve25519(buf+64, buf, buf+32);
		curve25519_donna(buf+96, buf, buf+32);
		if(memcmp(buf+64,buf+96,32)) {
			std::cerr << "Fail test: " << std::endl;
			std::cerr << "key:\t";
			for(unsigned i = 31; i < 32; i--)
				std::cerr << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
					<< (int)buf[i];
			std::cerr << std::endl;
			std::cerr << "point:\t";
			for(unsigned i = 31; i < 32; i--)
				std::cerr << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
					<< (int)buf[i+32];
			std::cerr << std::endl;
			std::cerr << "res1:\t";
			for(unsigned i = 31; i < 32; i--)
				std::cerr << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
					<< (int)buf[i+64];
			std::cerr << std::endl;
			std::cerr << "res2:\t";
			for(unsigned i = 31; i < 32; i--)
				std::cerr << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
					<< (int)buf[i+96];
			std::cerr << std::endl;
			abort();
		}
		if(++ctr % 10000 == 0)
			std::cerr << "Passed " << ctr << " tests." << std::endl;
	}
}
#endif
