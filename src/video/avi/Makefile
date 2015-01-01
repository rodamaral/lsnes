OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: codec/__all__.files $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@

codec/__all__.files: forcelook
	$(MAKE) -C codec

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../../include

forcelook:
	@true

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	$(MAKE) -C codec precheck

clean:
	$(MAKE) -C codec clean
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
