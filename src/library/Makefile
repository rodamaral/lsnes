OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../genfilelist.lua $^ >$@
	touch __all__.ldflags

framebuffer-basecolors.inc: framebuffer-basecolors.def
	lua make-basecolors.lua <$< >$@

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include/library

precheck: forcelook framebuffer-basecolors.inc
	../../buildaux/mkdeps.exe ../../include/library -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
