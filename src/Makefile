__all_files__: util/__all_files__ \
	lsnes$(DOT_EXECUTABLE_SUFFIX) \
	$(patsubst %.cpp,%.util$(DOT_EXECUTABLE_SUFFIX),$(wildcard util/*.cpp))

ifndef NO_RDYNAMIC
LDFLAGS += -rdynamic
endif
ifdef NO_DLFCN
CFLAGS += -DNO_DLFCN
else
LDFLAGS += -ldl
endif

ifdef BOOST_FILESYSTEM3
CFLAGS += -DBOOST_FILESYSTEM3
endif

ifdef OPUS_CODEC
CFLAGS += -DWITH_OPUS_CODEC
LDFLAGS += -lopus
endif

DUMMY_LIBRARY=core lua fonts library interface video emulation dummy
PLATFORM_LIBRARY=core lua fonts library interface video emulation platform
ALLOBJECT=__all__.$(OBJECT_SUFFIX)
ALLFLAGS=__all__.ldflags
DUMMY_LIBRARY_OBJS=$(patsubst %,%/$(ALLOBJECT),$(DUMMY_LIBRARY))
PLATFORM_LIBRARY_OBJS=$(patsubst %,%/$(ALLOBJECT),$(PLATFORM_LIBRARY))
DUMMY_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(DUMMY_LIBRARY))
PLATFORM_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(PLATFORM_LIBRARY))


core/$(ALLOBJECT): forcelook
	$(MAKE) -C core

dummy/$(ALLOBJECT): forcelook
	$(MAKE) -C dummy

emulation/$(ALLOBJECT): forcelook
	$(MAKE) -C emulation

fonts/$(ALLOBJECT): forcelook
	$(MAKE) -C fonts

library/$(ALLOBJECT): forcelook
	$(MAKE) -C library

interface/$(ALLOBJECT): forcelook
	$(MAKE) -C interface

lua/$(ALLOBJECT): forcelook
	$(MAKE) -C lua

platform/$(ALLOBJECT): forcelook
	$(MAKE) -C platform

util/__all_files__: forcelook
	$(MAKE) -C util

util/%.$(OBJECT_SUFFIX): util/__all_files__
	@true;

video/$(ALLOBJECT): forcelook
	$(MAKE) -C video

.PRECIOUS: %.$(OBJECT_SUFFIX) util/%.$(OBJECT_SUFFIX)

%.util$(DOT_EXECUTABLE_SUFFIX): %.$(OBJECT_SUFFIX) $(DUMMY_LIBRARY_OBJS)
	$(REALCC) -o $@ $^ $(LDFLAGS) $(CORE_OBJECT) `cat $(DUMMY_LIBRARY_FLAGS)`

lsnes$(DOT_EXECUTABLE_SUFFIX): $(PLATFORM_LIBRARY_OBJS)
	$(REALCC) -o $@ $^ $(LDFLAGS) $(CORE_OBJECT) `cat $(PLATFORM_LIBRARY_FLAGS)`

precheck:
	$(MAKE) -C core precheck
	$(MAKE) -C dummy precheck
	$(MAKE) -C emulation precheck
	$(MAKE) -C interface precheck
	$(MAKE) -C library precheck
	$(MAKE) -C lua precheck
	$(MAKE) -C platform precheck
	$(MAKE) -C util precheck
	$(MAKE) -C video precheck

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
	$(MAKE) -C core clean
	$(MAKE) -C dummy clean
	$(MAKE) -C emulation clean
	$(MAKE) -C fonts clean
	$(MAKE) -C interface clean
	$(MAKE) -C library clean
	$(MAKE) -C lua clean
	$(MAKE) -C platform clean
	$(MAKE) -C util clean
	$(MAKE) -C video clean

forcelook:
	@true
