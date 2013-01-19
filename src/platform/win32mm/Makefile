ifeq ($(JOYSTICK), WIN32MM)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))
else
OBJECTS=
endif

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	touch __all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
