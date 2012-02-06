ifeq ($(GRAPHICS), WXWIDGETS)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) 
WXW_CFLAGS += $(shell $(CROSS_PREFIX)wx-config --cxxflags) $(shell $(CROSS_PREFIX)pkg-config libswscale --cflags)
WXW_LDFLAGS += $(shell $(CROSS_PREFIX)wx-config --libs) $(shell $(CROSS_PREFIX)pkg-config libswscale --libs)
else
OBJECTS = dummy.$(OBJECT_SUFFIX)
endif


.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	echo $(WXW_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS) $(WXW_CFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
