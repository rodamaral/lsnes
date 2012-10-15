OPTIONS=options.build
include $(OPTIONS)

ifndef LUA
LUA=lua
endif

#Compilers.
REALCC = $(CROSS_PREFIX)$(CC)
REALLD = $(CROSS_PREFIX)$(LD)
REALRANLIB = $(CROSS_PREFIX)$(RANLIB)

ifeq ($(CORE_TYPE), BSNES)
CORE_PATH=$(shell pwd)/bsnes
CORE_SUBDIR=bsnes
CORE_OBJECT=bsnes/out/libsnes.$(ARCHIVE_SUFFIX)
CORE_DEFINE=-DCORETYPE_BSNES=1
ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
BSNES_PROFILE_STRING=profile=compatibility
else
BSNES_PROFILE_STRING=profile=accuracy
endif
ifeq ($(BSNES_VERSION), 084)
BSNES_PROFILE_STRING+=options=debugger
CFLAGS += -DBSNES_HAS_DEBUGGER
else
ifeq ($(BSNES_VERSION), 085)
BSNES_PROFILE_STRING+=options=debugger
CFLAGS += -DBSNES_HAS_DEBUGGER
endif
endif

ifeq ($(BSNES_VERSION), 087)
BSNES_TARGET_STRING=target=libsnes
else
BSNES_TARGET_STRING=ui=ui-libsnes
endif
CFLAGS += -DBSNES_V${BSNES_VERSION}
else
ifeq ($(CORE_TYPE), GAMBATTE)
CORE_PATH=$(shell pwd)/gambatte
CORE_SUBDIR=gambatte
CORE_OBJECT=gambatte/libgambatte/libgambatte.$(ARCHIVE_SUFFIX)
CORE_DEFINE=-DCORETYPE_GAMBATTE=1
BSNES_PROFILE_STRING=
BSNES_TARGET_STRING=
else
$(error "Bad value for CORETYPE (expected BSNES or GAMBATTE)")
endif
endif

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS += -I$(CORE_PATH) $(CORE_DEFINE) -std=gnu++0x $(USER_CFLAGS)
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



compiler=$(subst ++,cc,$(REALCC))
gambatte_compiler=$(REALCC)

$(CORE_OBJECT): forcelook
	$(MAKE) -C $(CORE_SUBDIR) $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING)
	$(REALRANLIB) $@

src/__all_files__: src/core/version.cpp forcelook $(CORE_OBJECT)
	$(MAKE) -C src precheck
	$(MAKE) -C src
	cp src/lsnes$(DOT_EXECUTABLE_SUFFIX) .

buildaux/version.exe: buildaux/version.cpp VERSION
	$(HOSTCC) $(HOSTCCFLAGS) -o $@ $<
src/core/version.cpp: buildaux/version.exe forcelook
	buildaux/version.exe >$@


clean:
	-$(MAKE) -C bsnes clean
	-$(MAKE) -C gambatte clean
	$(MAKE) -C src clean

forcelook:
	@true
