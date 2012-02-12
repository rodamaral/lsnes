OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) codec/__all__.o

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.o: $(OBJECTS)
	$(REALLD) -r -o $@ $^

codec/__all__.o: forcelook
	$(MAKE) -C codec

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../../include $(VIDEO_CLFAGS)

forcelook:
	@true

precheck:
	$(MAKE) -C codec precheck

clean:
	$(MAKE) -C codec clean
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
