ifeq ($(SOUND), PORTAUDIO)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) 
PA_CFLAGS += $(shell $(CROSS_PREFIX)pkg-config portaudio-2.0 --cflags)
PA_LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config portaudio-2.0 --libs)
else
OBJECTS = dummy.$(OBJECT_SUFFIX)
endif


.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	echo $(PA_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS) $(PA_CFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
