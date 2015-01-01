ifeq ($(GRAPHICS), WXWIDGETS)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))
WXW_CFLAGS += $(shell $(CROSS_PREFIX)wx-config --cxxflags) $(shell $(CROSS_PREFIX)pkg-config libswscale --cflags) -Wall
WXW_LDFLAGS += $(shell $(CROSS_PREFIX)wx-config --libs) $(shell $(CROSS_PREFIX)pkg-config libswscale --libs)
else
OBJECTS =
endif

ifeq ($(JOYSTICK), WXWIDGETS)
ifneq ($(GRAPHICS), WXWIDGETS)
$(error "WXWIDGETS joystick requires WXWIDGETS graphics")
endif
WXW_CFLAGS += -DWXWIDGETS_JOYSTICK_SUPPORT
endif


.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo $(WXW_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../../include $(CFLAGS) $(WXW_CFLAGS)

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
