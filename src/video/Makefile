OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) avi/__all__.o



.PRECIOUS: %.$(OBJECT_SUFFIX)

export VIDEO_CFLAGS

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	touch __all__.ldflags

avi/__all__.$(OBJECT_SUFFIX): forcelook
	$(MAKE) -C avi

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include

forcelook:
	@true

precheck:
	$(MAKE) -C avi precheck

clean:
	$(MAKE) -C avi clean
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
