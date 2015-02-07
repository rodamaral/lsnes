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
CFLAGS += -std=gnu++0x -pthread $(USER_CFLAGS)
ifdef BOOST_NEEDS_MT
BOOST_LIB_POSTFIX=-mt
endif
ifdef HOST_BOOST_NEEDS_MT
HOST_BOOST_POSTFIX=-mt
endif

LDFLAGS = -pthread -lboost_iostreams$(BOOST_LIB_POSTFIX) -lboost_filesystem$(BOOST_LIB_POSTFIX) -lboost_system$(BOOST_LIB_POSTFIX) -lz $(USER_LDFLAGS)
HOSTHELPER_LDFLAGS =

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

ifeq ($(REGEX), BOOST)
CFLAGS += -DUSE_BOOST_REGEX
LDFLAGS += -lboost_regex$(BOOST_LIB_POSTFIX)
HOSTHELPER_LDFLAGS += -lboost_regex$(HOST_BOOST_POSTFIX)
endif
HOSTHELPER_LDFLAGS += -lboost_system$(HOST_BOOST_POSTFIX)


ifdef NEED_LIBICONV
LDFLAGS += -liconv
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
CFLAGS += $(shell $(CROSS_PREFIX)curl-config --cflags)
LDFLAGS += $(shell $(CROSS_PREFIX)curl-config --libs)

compiler=$(subst ++,cc,$(REALCC))
gambatte_compiler=$(REALCC)

bsnes/out/libsnes.$(ARCHIVE_SUFFIX): forcelook
	$(MAKE) -C bsnes $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING)
	$(REALRANLIB) bsnes/out/libsnes.$(ARCHIVE_SUFFIX)


src/__all_files__: src/core/version.cpp buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) buildaux/txt2cstr$(DOT_EXECUTABLE_SUFFIX) forcelook
	$(MAKE) -C src precheck
	$(MAKE) -C src
	cp src/lsnes$(DOT_EXECUTABLE_SUFFIX) .

buildaux/txt2cstr$(DOT_EXECUTABLE_SUFFIX): buildaux/txt2cstr.cpp
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
buildaux/version$(DOT_EXECUTABLE_SUFFIX): buildaux/version.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX): buildaux/mkdeps.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $< -lboost_filesystem$(HOST_BOOST_POSTFIX) -lboost_system$(HOST_BOOST_POSTFIX)
src/core/version.cpp: buildaux/version$(DOT_EXECUTABLE_SUFFIX) forcelook
	buildaux/version$(DOT_EXECUTABLE_SUFFIX) >$@

platclean:
	$(MAKE) -C src platclean

clean:
	$(MAKE) -C src clean
	rm -f buildaux/version$(DOT_EXECUTABLE_SUFFIX)
	rm -f buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX)
	rm -f buildaux/txt2cstr$(DOT_EXECUTABLE_SUFFIX)

forcelook:
	@true
