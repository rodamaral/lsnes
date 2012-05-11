OPTIONS=options.build
include $(OPTIONS)


#Compilers.
REALCC = $(CROSS_PREFIX)$(CC)
REALLD = $(CROSS_PREFIX)$(LD)

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS = -I$(BSNES_PATH) -std=gnu++0x $(USER_CFLAGS)
ifdef BOOST_NEEDS_MT
BOOST_LIB_POSTFIX=
else
BOOST_LIB_POSTFIX=-mt
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
