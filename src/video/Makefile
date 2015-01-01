OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp))

ifdef SECRET_RABBIT_CODE
AVI_CFLAGS=-DWITH_SECRET_RABBIT_CODE
AVI_LDFLAGS=-lsamplerate
endif

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: avi/__all__.files $(OBJECTS)
	lua ../genfilelist.lua $^ >$@
	echo $(AVI_LDFLAGS) >__all__.ldflags

avi/__all__.files: forcelook
	$(MAKE) -C avi

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include $(AVI_CFLAGS)

forcelook:
	@true

precheck:
	../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../include -- *.cpp
	$(MAKE) -C avi precheck

clean:
	$(MAKE) -C avi clean
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
