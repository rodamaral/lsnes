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
LDFLAGS = -lboost_iostreams-mt -lboost_filesystem-mt -lboost_system-mt -lboost_regex-mt -lz $(USER_LDFLAGS)

#Platform
GRAPHICS=SDL
SOUND=SDL
JOYSTICK=SDL
THREADS=BOOST

#Threads
ifdef THREADS
ifeq ($(THREADS), NATIVE)
CFLAGS += -DNATIVE_THREADS
else
ifeq ($(THREADS), BOOST)
CFLAGS += -DBOOST_THREADS
ifdef BOOST_THREAD_LIB
LDFLAGS += -l$(BOOST_THREAD_LIB)
else
LDFLAGS += -lboost_thread-mt
endif
else
$(error "Bad value for THREADS (expected NATIVE or BOOST)")
endif
endif
endif

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
