ifeq ($(JOYSTICK), MACOSX)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) $(patsubst %.c,%.$(OBJECT_SUFFIX),$(wildcard *.c))
else
OBJECTS=
endif

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	touch __all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS)

%.$(OBJECT_SUFFIX): %.c %.c.dep
	$(REALCC) -DSDL_JOYSTICK_IOKIT -x c --std=gnu99 -c -o $@ $< $(CSTDFLAGS)

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp *.c
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
