OBJECTS=

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: audio/__all__.files video/__all__.files $(OBJECTS)
	lua ../../../genfilelist.lua $^ >$@

audio/__all__.files: forcelook
	$(MAKE) -C audio

video/__all__.files: forcelook
	$(MAKE) -C video

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include

forcelook:
	@true

precheck:
	$(MAKE) -C audio precheck
	$(MAKE) -C video precheck

clean:
	$(MAKE) -C audio clean
	$(MAKE) -C video clean
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
