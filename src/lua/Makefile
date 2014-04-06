OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) sysrc.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../genfilelist.lua $^ >$@
	echo >__all__.ldflags

sysrc.cpp: sysrc.lua
	luac -p $<
	../../buildaux/txt2cstr.exe lua_sysrc_script $< >$@

sysrc.cpp.dep: sysrc.cpp
	touch sysrc.cpp.dep

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS) -Wall

precheck:
	../../buildaux/mkdeps.exe ../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files sysrc.cpp
