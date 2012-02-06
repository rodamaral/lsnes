OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) 

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	touch __all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
