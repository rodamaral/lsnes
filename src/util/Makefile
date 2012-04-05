OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all_files__: $(OBJECTS)
	@true

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS)
	touch $(patsubst %.$(OBJECT_SUFFIX),%.ldflags,$^)

precheck:
	@true

clean:
	rm -f $(OBJECTS) *.ldflags
