ifdef BUILD_GAMBATTE
OBJECTS=core.$(OBJECT_SUFFIX) disassemble.$(OBJECT_SUFFIX)
GAMBATTE_CFLAGS=
GAMBATTE_LDFLAGS=

GAMBATTE_LIBRARY=gambatte/libgambatte/libgambatte.$(ARCHIVE_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS) ../../../$(GAMBATTE_LIBRARY)
	lua ../../genfilelist.lua $^ >$@
	echo $(GAMBATTE_LDFLAGS) ../$(GAMBATTE_LIBRARY) >__all__.ldflags

../../../$(GAMBATTE_LIBRARY): forcelook
	$(MAKE) -C ../../../gambatte
	$(REALRANLIB) $@

ports.inc: ports.json ../make-ports$(DOT_EXECUTABLE_SUFFIX)
	../make-ports$(DOT_EXECUTABLE_SUFFIX) <$< >$@

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep ports.inc
	$(REALCC) -c -o $@ $< -I../../../include -I../../../gambatte $(CFLAGS) $(GAMBATTE_CFLAGS) -Wreturn-type

else

OBJECTS=
__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo >__all__.ldflags

endif

forcelook:
	@true

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include ../../../gambatte -- *.cpp
	@true

clean:
	-make -C ../../../gambatte clean
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
