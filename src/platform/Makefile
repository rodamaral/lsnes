PLATFORMS=evdev portaudio wxwidgets win32mm libao macosx
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

macosx/$(ALLFILES): forcelook
	$(MAKE) -C macosx

.PRECIOUS: %.$(OBJECT_SUFFIX) *.files

precheck:
	$(MAKE) -C evdev precheck
	$(MAKE) -C win32mm precheck
	$(MAKE) -C portaudio precheck
	$(MAKE) -C libao precheck
	$(MAKE) -C wxwidgets precheck
	$(MAKE) -C macosx precheck

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.ldflags __all__.files
	$(MAKE) -C evdev clean
	$(MAKE) -C win32mm clean
	$(MAKE) -C portaudio clean
	$(MAKE) -C libao clean
	$(MAKE) -C wxwidgets clean
	$(MAKE) -C macosx clean

forcelook:
	@true
