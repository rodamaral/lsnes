__all_files__: util/__all_files__ \
	lsnes$(DOT_EXECUTABLE_SUFFIX) \
	$(patsubst %.cpp,%.util$(DOT_EXECUTABLE_SUFFIX),$(wildcard util/*.cpp))

DUMMY_LIBRARY=core lua fonts video dummy
PLATFORM_LIBRARY=core lua fonts video plat-dummy plat-evdev plat-portaudio plat-sdl plat-wxwidgets
ALLOBJECT=__all__.$(OBJECT_SUFFIX)
ALLFLAGS=__all__.ldflags
DUMMY_LIBRARY_OBJS=$(patsubst %,%/$(ALLOBJECT),$(DUMMY_LIBRARY))
PLATFORM_LIBRARY_OBJS=$(patsubst %,%/$(ALLOBJECT),$(PLATFORM_LIBRARY))
DUMMY_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(DUMMY_LIBRARY))
PLATFORM_LIBRARY_FLAGS=$(patsubst %,%/$(ALLFLAGS),$(PLATFORM_LIBRARY))


core/$(ALLOBJECT): forcelook
	$(MAKE) -C core

fonts/$(ALLOBJECT): forcelook
	$(MAKE) -C fonts

lua/$(ALLOBJECT): forcelook
	$(MAKE) -C lua

dummy/$(ALLOBJECT): forcelook
	$(MAKE) -C dummy

plat-dummy/$(ALLOBJECT): forcelook
	$(MAKE) -C plat-dummy

plat-evdev/$(ALLOBJECT): forcelook
	$(MAKE) -C plat-evdev

plat-portaudio/$(ALLOBJECT): forcelook
	$(MAKE) -C plat-portaudio

plat-sdl/$(ALLOBJECT): forcelook
	$(MAKE) -C plat-sdl

plat-wxwidgets/$(ALLOBJECT): forcelook
	$(MAKE) -C plat-wxwidgets

util/__all_files__: forcelook
	$(MAKE) -C util

util/%.$(OBJECT_SUFFIX): util/__all_files__
	@true;

video/$(ALLOBJECT): forcelook
	$(MAKE) -C video

.PRECIOUS: %.$(OBJECT_SUFFIX) util/%.$(OBJECT_SUFFIX)

%.util$(DOT_EXECUTABLE_SUFFIX): %.$(OBJECT_SUFFIX) $(DUMMY_LIBRARY_OBJS)
	$(REALCC) -o $@ $^ $(LDFLAGS) $(BSNES_PATH)/out/libsnes.a `cat $(DUMMY_LIBRARY_FLAGS)`

lsnes$(DOT_EXECUTABLE_SUFFIX): $(PLATFORM_LIBRARY_OBJS)
	$(REALCC) -o $@ $^ $(LDFLAGS) $(BSNES_PATH)/out/libsnes.a `cat $(PLATFORM_LIBRARY_FLAGS)`

precheck:
	$(MAKE) -C core precheck
	$(MAKE) -C lua precheck
	$(MAKE) -C plat-dummy precheck
	$(MAKE) -C plat-evdev precheck
	$(MAKE) -C plat-portaudio precheck
	$(MAKE) -C plat-sdl precheck
	$(MAKE) -C plat-wxwidgets precheck
	$(MAKE) -C util precheck
	$(MAKE) -C video precheck

clean:
	rm -f *.ldflags
	$(MAKE) -C core clean
	$(MAKE) -C lua clean
	$(MAKE) -C plat-dummy clean
	$(MAKE) -C plat-evdev clean
	$(MAKE) -C plat-portaudio clean
	$(MAKE) -C plat-sdl clean
	$(MAKE) -C plat-wxwidgets clean
	$(MAKE) -C util clean
	$(MAKE) -C video clean

forcelook:
	@true
