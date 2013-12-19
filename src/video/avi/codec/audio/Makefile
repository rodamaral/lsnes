OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../../../genfilelist.lua $^ >$@

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../../../../include

precheck:
	../../../../../buildaux/mkdeps.exe ../../../../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
