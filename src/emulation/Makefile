CORES=bsnes-legacy gambatte sky test
ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags
CORES_FILES=$(patsubst %,%/$(ALLFILES),$(CORES))
CORES_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(CORES))

__all__.files: $(CORES_FILES)
	lua ../genfilelist.lua $^ >$@
	cat $(CORES_FLAGS) >$(ALLFLAGS)

make-ports.exe: make-ports.cpp
	$(HOSTCC) -g -std=gnu++0x -I../../include/library -o $@ $< ../library/json.cpp ../library/utf8.cpp ../library/string.cpp ../library/controller-parse.cpp ../library/controller-data.cpp ../library/sha256.cpp ../library/assembler.cpp -lboost_regex$(HOST_BOOST_POSTFIX) -lboost_system$(HOST_BOOST_POSTFIX)

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

forcelook:
	@true
