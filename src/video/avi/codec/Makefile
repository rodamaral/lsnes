OBJECTS=video/__all__.$(OBJECT_SUFFIX) audio/__all__.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^

audio/__all__.$(OBJECT_SUFFIX): forcelook
	$(MAKE) -C audio

video/__all__.$(OBJECT_SUFFIX): forcelook
	$(MAKE) -C video

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include $(VIDEO_CLFAGS)

forcelook:
	@true

precheck:
	$(MAKE) -C audio precheck
	$(MAKE) -C video precheck

clean:
	$(MAKE) -C audio clean
	$(MAKE) -C video clean
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
