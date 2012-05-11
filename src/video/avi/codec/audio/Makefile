OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.o: $(OBJECTS)
	$(REALLD) -r -o $@ $^

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../../../../include

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
