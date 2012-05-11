OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) avi/__all__.o

ifdef SECRET_RABBIT_CODE
AVI_CFLAGS=-DWITH_SECRET_RABBIT_CODE
AVI_LDFLAGS=-lsamplerate
endif

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	echo $(AVI_LDFLAGS) >__all__.ldflags

avi/__all__.$(OBJECT_SUFFIX): forcelook
	$(MAKE) -C avi

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include $(AVI_CFLAGS)

forcelook:
	@true

precheck:
	$(MAKE) -C avi precheck

clean:
	$(MAKE) -C avi clean
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
