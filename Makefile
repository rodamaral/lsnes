CROSS_PREFIX=
DOT_EXECUTABLE_SUFFIX=
OBJECT_SUFFIX = o
ARCHIVE_SUFFIX = a
FONT_SRC := unifontfull-5.1.20080820.hex

USER_CFLAGS=
USER_LDFLAGS=

#Compilers.
CC := g++
LD := ld
REALCC = $(CROSS_PREFIX)$(CC)
REALLD = $(CROSS_PREFIX)$(LD)
HOSTCC = $(CC)

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS = -I$(BSNES_PATH) -std=gnu++0x $(USER_CFLAGS)
LDFLAGS = -lboost_iostreams -lboost_filesystem -lboost_system -lz $(USER_LDFLAGS)

#Platform
GRAPHICS=SDL
SOUND=SDL
JOYSTICK=SDL
THREADS=BOOST

ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
endif

export DOT_EXECUTABLE_SUFFIX OBJECT_SUFFIX ARCHIVE_SUFFIX FONT_SRC REALCC HOSTCC REALLD HOSTCCFLAGS CFLAGS LDFLAGS GRAPHICS SOUND JOYSTICK THREADS

all: src/__all_files__

src/__all_files__: src/core/version.cpp forcelook
	$(MAKE) -C src precheck
	$(MAKE) -C src
	cp src/lsnes$(DOT_EXECUTABLE_SUFFIX) .

buildaux/version.exe: buildaux/version.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
src/core/version.cpp: buildaux/version.exe forcelook
	buildaux/version.exe >$@


clean:
	$(MAKE) -C src clean

forcelook:
	@true
