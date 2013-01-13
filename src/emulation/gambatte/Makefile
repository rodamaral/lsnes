ifdef BUILD_GAMBATTE
OBJECTS=core.$(OBJECT_SUFFIX)
GAMBATTE_CFLAGS=
GAMBATTE_LDFLAGS=
else
OBJECTS=dummy.$(OBJECT_SUFFIX)
endif

GAMBATTE_LIBRARY=gambatte/libgambatte/libgambatte.$(ARCHIVE_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS) ../../../$(GAMBATTE_LIBRARY)
	$(REALLD) -r -o $@ $^
	echo $(GAMBATTE_LDFLAGS) ../$(GAMBATTE_LIBRARY) >__all__.ldflags

../../../$(GAMBATTE_LIBRARY): forcelook
	$(MAKE) -C ../../../gambatte
	$(REALRANLIB) $@

ports.inc: ports.def ../make-ports.lua
	lua ../make-ports.lua $< >$@

slots.inc: slots.def ../make-slots.lua
	lua ../make-slots.lua $< >$@

regions.inc: regions.def ../make-regions.lua
	lua ../make-regions.lua $< >$@

%.$(OBJECT_SUFFIX): %.cpp ports.inc slots.inc regions.inc
	$(REALCC) -c -o $@ $< -I../../../include -I../../../gambatte $(CFLAGS) $(GAMBATTE_CFLAGS)

forcelook:
	@true

precheck:
	@true

clean:
	make -C ../../../gambatte clean
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
