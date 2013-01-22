PLATFORMS=evdev portaudio wxwidgets win32mm libao
ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags
PLATFORMS_FILES=$(patsubst %,%/$(ALLFILES),$(PLATFORMS))
PLATFORMS_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(PLATFORMS))

$(ALLFILES): $(PLATFORMS_FILES) $(OBJECTS)
	lua ../genfilelist.lua $^ >$@
	cat $(PLATFORMS_FLAGS) >$(ALLFLAGS)

evdev/$(ALLFILES): forcelook
	$(MAKE) -C evdev

win32mm/$(ALLFILES): forcelook
	$(MAKE) -C win32mm

portaudio/$(ALLFILES): forcelook
	$(MAKE) -C portaudio

libao/$(ALLFILES): forcelook
	$(MAKE) -C libao

wxwidgets/$(ALLFILES): forcelook
	$(MAKE) -C wxwidgets


.PRECIOUS: %.$(OBJECT_SUFFIX) *.files

precheck:
	$(MAKE) -C evdev precheck
	$(MAKE) -C win32mm precheck
	$(MAKE) -C portaudio precheck
	$(MAKE) -C libao precheck
	$(MAKE) -C wxwidgets precheck

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
	$(MAKE) -C evdev clean
	$(MAKE) -C win32mm clean
	$(MAKE) -C portaudio clean
	$(MAKE) -C libao clean
	$(MAKE) -C wxwidgets clean

forcelook:
	@true
