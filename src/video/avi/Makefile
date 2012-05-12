OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) codec/__all__.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^

codec/__all__.$(OBJECT_SUFFIX): forcelook
	$(MAKE) -C codec

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../../include

forcelook:
	@true

precheck:
	$(MAKE) -C codec precheck

clean:
	$(MAKE) -C codec clean
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
