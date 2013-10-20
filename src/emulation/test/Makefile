OBJECTS=test.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo  >__all__.ldflags

ports.inc: ports.json ../make-ports.exe
	../make-ports.exe <$< >$@

%.$(OBJECT_SUFFIX): %.cpp ports.inc
	$(REALCC) -c -o $@ $< -I../../../include -I../../../gambatte $(CFLAGS)

forcelook:
	@true

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
