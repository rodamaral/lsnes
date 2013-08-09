all: libgambatte.$(ARCHIVE_SUFFIX)

REALAR=$(CROSS_PREFIX)ar

OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/*.cpp src/video/*.cpp src/mem/*.cpp src/sound/*.cpp src/file/file.cpp))

%.o: %.cpp
	$(gambatte_compiler) $(CFLAGS) -Wno-deprecated-declarations -DHAVE_CSTDINT -I../common -Iinclude -Isrc -c -o $@ $<

libgambatte.$(ARCHIVE_SUFFIX): $(OBJECTS)
	$(REALAR) crvs $@ $^
	$(REALRANLIB) $@

clean: forcelook
	rm -f $(OBJECTS) libgambatte.$(ARCHIVE_SUFFIX)

forcelook:
	@true