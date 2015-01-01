OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all_files__: $(OBJECTS)
	@true

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS)

precheck:
	../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f $(OBJECTS) *.ldflags
