CORES=bsnes-legacy gambatte sky
ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags
CORES_FILES=$(patsubst %,%/$(ALLFILES),$(CORES))
CORES_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(CORES))

__all__.files: $(CORES_FILES)
	lua ../genfilelist.lua $^ >$@
	cat $(CORES_FLAGS) >$(ALLFLAGS)

bsnes-legacy/$(ALLFILES): forcelook
	$(MAKE) -C bsnes-legacy

gambatte/$(ALLFILES): forcelook
	$(MAKE) -C gambatte

sky/$(ALLFILES): forcelook
	$(MAKE) -C sky

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

precheck:
	$(MAKE) -C bsnes-legacy precheck
	$(MAKE) -C gambatte precheck
	$(MAKE) -C sky precheck

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
	$(MAKE) -C bsnes-legacy clean
	$(MAKE) -C gambatte clean
	$(MAKE) -C sky clean

forcelook:
	@true
