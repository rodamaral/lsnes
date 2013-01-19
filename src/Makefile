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

COMMON_LIBRARY=core lua fonts library interface video emulation
DUMMY_LIBRARY=$(COMMON_LIBRARY) dummy
PLATFORM_LIBRARY=$(COMMON_LIBRARY) platform
ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags
DUMMY_LIBRARY_FILES=$(patsubst %,%/$(ALLFILES),$(DUMMY_LIBRARY))
PLATFORM_LIBRARY_FILES=$(patsubst %,%/$(ALLFILES),$(PLATFORM_LIBRARY))
DUMMY_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(DUMMY_LIBRARY))
PLATFORM_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(PLATFORM_LIBRARY))

__all_dummy__.files: $(DUMMY_LIBRARY_FILES)
	lua genfilelist.lua $^ >$@

__all_platform__.files: $(PLATFORM_LIBRARY_FILES)
	lua genfilelist.lua $^ >$@

core/$(ALLFILES): forcelook
	$(MAKE) -C core

dummy/$(ALLFILES): forcelook
	$(MAKE) -C dummy

emulation/$(ALLFILES): forcelook
	$(MAKE) -C emulation

fonts/$(ALLFILES): forcelook
	$(MAKE) -C fonts

library/$(ALLFILES): forcelook
	$(MAKE) -C library

interface/$(ALLFILES): forcelook
	$(MAKE) -C interface

lua/$(ALLFILES): forcelook
	$(MAKE) -C lua

platform/$(ALLFILES): forcelook
	$(MAKE) -C platform

util/__all_files__: forcelook
	$(MAKE) -C util

util/%.$(OBJECT_SUFFIX): util/__all_files__
	@true;

video/$(ALLFILES): forcelook
	$(MAKE) -C video

.PRECIOUS: %.$(OBJECT_SUFFIX) util/%.$(OBJECT_SUFFIX) %.files

%.util$(DOT_EXECUTABLE_SUFFIX): %.$(OBJECT_SUFFIX) __all_dummy__.files
	$(REALCC) -o $@ $< `cat __all_dummy__.files` $(LDFLAGS) `cat $(DUMMY_LIBRARY_FLAGS)`

lsnes$(DOT_EXECUTABLE_SUFFIX): __all_platform__.files
	$(REALCC) -o $@ `cat __all_platform__.files` $(LDFLAGS) `cat $(PLATFORM_LIBRARY_FLAGS)`

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
