ifdef BUILD_METEOR
OBJECTS=core.$(OBJECT_SUFFIX)
METEOR_CFLAGS=
METEOR_LDFLAGS=
REALAR=$(CROSS_PREFIX)ar
export REALAR
METEOR_LIBRARY=meteor/libmeteor.$(ARCHIVE_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS) ../../../$(METEOR_LIBRARY)
	lua ../../genfilelist.lua $^ >$@
	echo $(METEOR_LDFLAGS) ../$(METEOR_LIBRARY) >__all__.ldflags

../../../$(METEOR_LIBRARY): forcelook
	$(MAKE) -C ../../../meteor
	$(REALRANLIB) $@

ports.inc: ports.json ../make-ports.exe
	../make-ports.exe <$< >$@

%.$(OBJECT_SUFFIX): %.cpp ports.inc
	$(REALCC) -c -o $@ $< -I../../../include -I../../../meteor/ameteor/include $(CFLAGS) $(METEOR_CFLAGS)

else

OBJECTS=
__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo >__all__.ldflags

endif

forcelook:
	@true

precheck:
	@true

clean:
	-make -C ../../../gambatte clean
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
