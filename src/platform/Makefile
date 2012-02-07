PLATFORMS=dummy evdev portaudio sdl wxwidgets
ALLOBJECT=__all__.$(OBJECT_SUFFIX)
ALLFLAGS=__all__.ldflags
PLATFORMS_OBJS=$(patsubst %,%/$(ALLOBJECT),$(PLATFORMS))
PLATFORMS_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(PLATFORMS))

$(ALLOBJECT): $(PLATFORMS_OBJS)
	ld -r -o $@ $^
	cat $(PLATFORMS_FLAGS) >$(ALLFLAGS)

dummy/$(ALLOBJECT): forcelook
	$(MAKE) -C dummy

evdev/$(ALLOBJECT): forcelook
	$(MAKE) -C evdev

portaudio/$(ALLOBJECT): forcelook
	$(MAKE) -C portaudio

sdl/$(ALLOBJECT): forcelook
	$(MAKE) -C sdl

wxwidgets/$(ALLOBJECT): forcelook
	$(MAKE) -C wxwidgets


.PRECIOUS: %.$(OBJECT_SUFFIX)

precheck:
	$(MAKE) -C dummy precheck
	$(MAKE) -C evdev precheck
	$(MAKE) -C portaudio precheck
	$(MAKE) -C sdl precheck
	$(MAKE) -C wxwidgets precheck

clean:
	rm -f *.ldflags
	$(MAKE) -C dummy clean
	$(MAKE) -C evdev clean
	$(MAKE) -C portaudio clean
	$(MAKE) -C sdl clean
	$(MAKE) -C wxwidgets clean

forcelook:
	@true
