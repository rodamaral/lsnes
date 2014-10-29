ifdef WIN32
CROSS_PREFIX=i686-pc-mingw32.static-
GCC_SUFFIX=
GCC=$(CROSS_PREFIX)gcc$(GCC_SUFFIX)
GPP=$(CROSS_PREFIX)g++$(GCC_SUFFIX)
CFLAGS=-D__MSVCRT_VERSION__=0x0601 -fPIC -std=gnu++11 -g
LIBRARY_EXT=dll
EXE_EXT=.exe
else
CROSS_PREFIX=
GCC_SUFFIX=-4.9
GCC=$(CROSS_PREFIX)gcc$(GCC_SUFFIX)
GPP=$(CROSS_PREFIX)g++$(GCC_SUFFIX)
CFLAGS=-fPIC -std=gnu++11 -g
LIBRARY_EXT=so
EXE_EXT=
endif

all: core.$(LIBRARY_EXT) gbasm$(EXE_EXT)

bsnes/out/libsnes.a: FORCELOOK
	make -C bsnes ui=ui-libsnes options=debugger profile=compatibility compiler=$(GCC)

gambatte/libgambatte/libgambatte.a: FORCELOOK
	make -C gambatte OBJECT_SUFFIX=o ARCHIVE_SUFFIX=a CFLAGS="-g -std=gnu++11 -fPIC -O3" gambatte_compiler=$(GPP) REALRANLIB=$(CROSS_PREFIX)ranlib

libsnes.a: bsnes/out/libsnes.a
	cp $^ $@

libgambatte.a: gambatte/libgambatte/libgambatte.a
	cp $^ $@

core.$(LIBRARY_EXT): core-freestanding.cpp libsnes.a libgambatte.a callbacks.hpp controllerjson.hpp memregions.hpp strfmt.hpp c-interface.h debug.hpp registers.hpp tempmem.hpp c-interface-translate.hpp gbdisasm.hpp sram.hpp serialize.hpp
	$(GPP) $(CFLAGS) -shared -o $@ $< -Wl,--exclude-libs,ALL libsnes.a libgambatte.a -Ibsnes -Igambatte

gbasm$(EXE_EXT): gbasm.cpp
	$(GPP) $(CFLAGS) -o $@ $<

FORCELOOK:
	@true
