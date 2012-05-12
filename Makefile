OPTIONS=options.build
include $(OPTIONS)


#Compilers.
REALCC = $(CROSS_PREFIX)$(CC)
REALLD = $(CROSS_PREFIX)$(LD)
REALRANLIB = $(CROSS_PREFIX)$(RANLIB)

BSNES_PATH=$(shell pwd)/bsnes

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS = -I$(BSNES_PATH) -std=gnu++0x $(USER_CFLAGS)
ifdef BOOST_NEEDS_MT
BOOST_LIB_POSTFIX=-mt
else
BOOST_LIB_POSTFIX=
endif

LDFLAGS = -lboost_iostreams$(BOOST_LIB_POSTFIX) -lboost_filesystem$(BOOST_LIB_POSTFIX) -lboost_system$(BOOST_LIB_POSTFIX) -lboost_regex$(BOOST_LIB_POSTFIX) -lz $(USER_LDFLAGS)

ifeq ($(THREADS), NATIVE)
CFLAGS += -DNATIVE_THREADS
else
ifeq ($(THREADS), BOOST)
CFLAGS += -DBOOST_THREADS
LDFLAGS += -lboost_thread$(BOOST_LIB_POSTFIX)
else
$(error "Bad value for THREADS (expected NATIVE or BOOST)")
endif
endif

ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
endif

export

all: src/__all_files__

ifeq ($(BSNES_VERSION), 087)
BSNES_TARGET_STRING=target=libsnes
else
BSNES_TARGET_STRING=ui=ui-libsnes
endif

ifdef BSNES_IS_COMPAT
BSNES_PROFILE_STRING=profile=compatibility
else
BSNES_PROFILE_STRING=profile=accuracy
endif

bsnes_compiler=$(subst ++,cc,$(REALCC))

bsnes/out/libsnes.$(ARCHIVE_SUFFIX): bsnes/snes/snes.hpp forcelook
	$(MAKE) -C bsnes OPTIONS=debugger $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING) compiler=$(bsnes_compiler)
	$(REALRANLIB) $@

src/__all_files__: src/core/version.cpp forcelook bsnes/out/libsnes.$(ARCHIVE_SUFFIX)
	$(MAKE) -C src precheck
	$(MAKE) -C src
	cp src/lsnes$(DOT_EXECUTABLE_SUFFIX) .

buildaux/version.exe: buildaux/version.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
src/core/version.cpp: buildaux/version.exe forcelook
	buildaux/version.exe >$@


clean:
	$(MAKE) -C bsnes clean
	$(MAKE) -C src clean

forcelook:
	@true
