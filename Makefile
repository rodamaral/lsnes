OPTIONS=options.build
include $(OPTIONS)


#Compilers.
REALCC = $(CROSS_PREFIX)$(CC)
REALLD = $(CROSS_PREFIX)$(LD)
REALRANLIB = $(CROSS_PREFIX)$(RANLIB)

GAMBATTE_PATH=$(shell pwd)/gambatte

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS = -I$(GAMBATTE_PATH) -std=gnu++0x $(USER_CFLAGS)
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

export

all: src/__all_files__

gambatte_compiler=$(subst ++,cc,$(REALCC))

gambatte/libgambatte/libgambatte.$(ARCHIVE_SUFFIX): forcelook
	$(MAKE) -C gambatte compiler=$(gambatte_compiler)
	$(REALRANLIB) $@

src/__all_files__: src/core/version.cpp forcelook gambatte/libgambatte/libgambatte.$(ARCHIVE_SUFFIX)
	$(MAKE) -C src precheck
	$(MAKE) -C src
	cp src/lsnes$(DOT_EXECUTABLE_SUFFIX) .

buildaux/version.exe: buildaux/version.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
src/core/version.cpp: buildaux/version.exe forcelook
	buildaux/version.exe >$@


clean:
	$(MAKE) -C gambatte clean
	$(MAKE) -C src clean

forcelook:
	@true
