OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../genfilelist.lua $^ >$@
	touch __all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
