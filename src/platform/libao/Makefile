ifeq ($(SOUND), LIBAO)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))
PA_CFLAGS += $(shell $(CROSS_PREFIX)pkg-config ao --cflags)
PA_LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config ao --libs)
else
OBJECTS=
endif

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo $(PA_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS) $(PA_CFLAGS)

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
