OBJECTS=test.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo  >__all__.ldflags

ports.inc: ports.json ../make-ports$(DOT_EXECUTABLE_SUFFIX)
	../make-ports$(DOT_EXECUTABLE_SUFFIX) <$< >$@

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep ports.inc
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS)

forcelook:
	@true

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
