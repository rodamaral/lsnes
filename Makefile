OPTIONS=options.build
include $(OPTIONS)

ifndef LUA
LUA=lua
endif

#Compilers.
REALCC = $(CROSS_PREFIX)$(CC)
REALLD = $(CROSS_PREFIX)$(LD)
REALRANLIB = $(CROSS_PREFIX)$(RANLIB)

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS += -std=gnu++0x $(USER_CFLAGS)
ifdef BOOST_NEEDS_MT
BOOST_LIB_POSTFIX=-mt
else
BOOST_LIB_POSTFIX=
endif
ifdef HOST_BOOST_NEEDS_MT
HOST_BOOST_LIB_POSTFIX=-mt
else
HOST_BOOST_LIB_POSTFIX=
endif

LDFLAGS = -lboost_iostreams$(BOOST_LIB_POSTFIX) -lboost_filesystem$(BOOST_LIB_POSTFIX) -lboost_system$(BOOST_LIB_POSTFIX) -lboost_regex$(BOOST_LIB_POSTFIX) -lz -lcurl $(USER_LDFLAGS)

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

ifdef USE_LIBGCRYPT
CFLAGS += -DUSE_LIBGCRYPT_SHA256
LDFLAGS += -lgcrypt -lgpg-error
endif

ifdef USE_LIBLZMA
CFLAGS += -DLIBLZMA_AVAILABLE
LDFLAGS += -llzma
endif

ifeq ($(ARCHITECTURE), I386)
CFLAGS += -DARCH_IS_I386
else
endif


export

all: src/__all_files__

CFLAGS += $(shell $(CROSS_PREFIX)pkg-config $(LUA) --cflags)
LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config $(LUA) --libs)

compiler=$(subst ++,cc,$(REALCC))
gambatte_compiler=$(REALCC)

bsnes/out/libsnes.$(ARCHIVE_SUFFIX): forcelook
	$(MAKE) -C bsnes $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING)
	$(REALRANLIB) bsnes/out/libsnes.$(ARCHIVE_SUFFIX)


src/__all_files__: src/core/version.cpp buildaux/mkdeps.exe forcelook
	$(MAKE) -C src precheck
	$(MAKE) -C src
	cp src/lsnes$(DOT_EXECUTABLE_SUFFIX) .

buildaux/version.exe: buildaux/version.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
buildaux/mkdeps.exe: buildaux/mkdeps.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $< -lboost_filesystem$(HOST_BOOST_LIB_POSTFIX) -lboost_system$(HOST_BOOST_LIB_POSTFIX)
src/core/version.cpp: buildaux/version.exe forcelook
	buildaux/version.exe >$@


clean:
	$(MAKE) -C src clean
	rm -f buildaux/version.exe

forcelook:
	@true
