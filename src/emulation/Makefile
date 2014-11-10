CORES=bsnes-legacy gambatte sky test
ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags
CORES_FILES=$(patsubst %,%/$(ALLFILES),$(CORES))
CORES_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(CORES))

__all__.files: $(CORES_FILES)
	lua ../genfilelist.lua $^ >$@
	cat $(CORES_FLAGS) >$(ALLFLAGS)

make-ports.exe: make-ports.cpp ../library/json.cpp ../library/utf8.cpp ../library/string.cpp ../library/portctrl-parse.cpp ../library/portctrl-data.cpp ../library/sha256.cpp ../library/assembler.cpp  ../library/hex.cpp  ../library/eatarg.cpp ../library/int24.cpp ../library/binarystream.cpp ../library/integer-pool.cpp
	$(HOSTCC) -g -std=gnu++0x -I../../include/library -o $@ $^  -lboost_regex$(HOST_BOOST_POSTFIX) -lboost_system$(HOST_BOOST_POSTFIX) -Wall -DNO_ASM_GENERATION

bsnes-legacy/$(ALLFILES): forcelook make-ports.exe
	$(MAKE) -C bsnes-legacy

gambatte/$(ALLFILES): forcelook make-ports.exe
	$(MAKE) -C gambatte

sky/$(ALLFILES): forcelook make-ports.exe
	$(MAKE) -C sky

test/$(ALLFILES): forcelook make-ports.exe
	$(MAKE) -C test

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

precheck:
	$(MAKE) -C bsnes-legacy precheck
	$(MAKE) -C gambatte precheck
	$(MAKE) -C sky precheck
	$(MAKE) -C test precheck

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
	$(MAKE) -C bsnes-legacy clean
	$(MAKE) -C gambatte clean
	$(MAKE) -C sky clean
	$(MAKE) -C test clean
	rm -f make-ports.exe

forcelook:
	@true
