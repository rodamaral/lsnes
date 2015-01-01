OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../../include -I. $(CFLAGS) -Wall

forcelook:
	@true

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
