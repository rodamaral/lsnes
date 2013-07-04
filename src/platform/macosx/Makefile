ifeq ($(JOYSTICK), MACOSX)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) $(patsubst %.c,%.$(OBJECT_SUFFIX),$(wildcard *.c))
else
OBJECTS=
endif

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	touch __all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS)

%.$(OBJECT_SUFFIX): %.c
	$(REALCC) -DSDL_JOYSTICK_IOKIT -x c --std=gnu99 -c -o $@ $< $(CSTDFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
