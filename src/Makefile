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
CFLAGS += -DWITH_OPUS_CODEC $(shell $(CROSS_PREFIX)pkg-config --cflags opus)
LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config --libs opus)
ifdef OPUS_CODEC_SUPPORTS_SURROUND
CFLAGS += -DOPUS_SUPPORTS_SURROUND
endif
endif

COMMON_LIBRARY=core lua fonts library interface video emulation cmdhelp
ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags
COMMON_LIBRARY_FILES=$(patsubst %,%/$(ALLFILES),$(COMMON_LIBRARY))
COMMON_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(COMMON_LIBRARY))
PLATFORM_LIBRARY_FILES=platform/$(ALLFILES)
PLATFORM_LIBRARY_FLAGS=platform/$(ALLFLAGS)

__all_common__.files: $(COMMON_LIBRARY_FILES)
	lua genfilelist.lua $^ >$@

__all_platform__.files: $(PLATFORM_LIBRARY_FILES)
	lua genfilelist.lua $^ >$@

core/$(ALLFILES): forcelook cmdhelp/$(ALLFILES)
	$(MAKE) -C core

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

cmdhelp/$(ALLFILES): forcelook
	$(MAKE) -C cmdhelp

util/__all_files__: forcelook
	$(MAKE) -C util

util/%.$(OBJECT_SUFFIX): util/__all_files__
	@true;

video/$(ALLFILES): forcelook
	$(MAKE) -C video

.PRECIOUS: %.$(OBJECT_SUFFIX) util/%.$(OBJECT_SUFFIX) %.files

%.util$(DOT_EXECUTABLE_SUFFIX): %.$(OBJECT_SUFFIX) __all_common__.files
	$(REALCC) -o $@ $< `cat __all_common__.files` $(LDFLAGS) `cat $(COMMON_LIBRARY_FLAGS)`

lsnes$(DOT_EXECUTABLE_SUFFIX): __all_common__.files __all_platform__.files
	$(REALCC) -o $@ `cat __all_common__.files __all_platform__.files` $(LDFLAGS) `cat $(COMMON_LIBRARY_FLAGS) $(PLATFORM_LIBRARY_FLAGS)`

precheck:
	$(MAKE) -C cmdhelp prebuild
	$(MAKE) -C core precheck
	$(MAKE) -C emulation precheck
	$(MAKE) -C fonts precheck
	$(MAKE) -C interface precheck
	$(MAKE) -C library precheck
	$(MAKE) -C lua precheck
	$(MAKE) -C platform precheck
	$(MAKE) -C util precheck
	$(MAKE) -C video precheck
	$(MAKE) -C cmdhelp precheck

platclean:
	$(MAKE) -C emulation clean

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
	find . -name "*.dep" -exec rm -f {} \;
	$(MAKE) -C core clean
	$(MAKE) -C emulation clean
	$(MAKE) -C fonts clean
	$(MAKE) -C interface clean
	$(MAKE) -C library clean
	$(MAKE) -C lua clean
	$(MAKE) -C platform clean
	$(MAKE) -C util clean
	$(MAKE) -C video clean
	$(MAKE) -C cmdhelp clean

forcelook:
	@true
