CORES=bsnes-legacy gambatte
ALLOBJECT=__all__.$(OBJECT_SUFFIX)
ALLFLAGS=__all__.ldflags
CORES_OBJS=$(patsubst %,%/$(ALLOBJECT),$(CORES))
CORES_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(CORES))

$(ALLOBJECT): $(CORES_OBJS)
	$(REALLD) -r -o $@ $^
	cat $(CORES_FLAGS) >$(ALLFLAGS)

bsnes-legacy/$(ALLOBJECT): forcelook
	$(MAKE) -C bsnes-legacy

gambatte/$(ALLOBJECT): forcelook
	$(MAKE) -C gambatte

.PRECIOUS: %.$(OBJECT_SUFFIX)

precheck:
	$(MAKE) -C bsnes-legacy precheck
	$(MAKE) -C gambatte precheck

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
	$(MAKE) -C bsnes-legacy clean
	$(MAKE) -C gambatte clean

forcelook:
	@true
