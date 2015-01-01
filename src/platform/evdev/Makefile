ifeq ($(JOYSTICK), EVDEV)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))
else
OBJECTS=
endif

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	touch __all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS) -Wall

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
