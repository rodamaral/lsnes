#ifndef _library__curve25519__hpp__included__
#define _library__curve25519__hpp__included__

#include <cstdint>

extern "C"
{
void curve25519(uint8_t* out, const uint8_t* key, const uint8_t* base);
void curve25519_clamp(uint8_t* key);
extern const uint8_t curve25519_base[];
}

#endif
