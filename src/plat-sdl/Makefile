ifeq ($(GRAPHICS), SDL)
OBJECTS = commandline.$(OBJECT_SUFFIX) drawprim.$(OBJECT_SUFFIX) graphicsfn.$(OBJECT_SUFFIX) keyboard.$(OBJECT_SUFFIX) main.$(OBJECT_SUFFIX) thread.$(OBJECT_SUFFIX) status.$(OBJECT_SUFFIX)
SDL_CFLAGS += $(shell $(CROSS_PREFIX)sdl-config --cflags)
SDL_LDFLAGS += $(shell $(CROSS_PREFIX)sdl-config --libs)
else
OBJECTS = dummy.$(OBJECT_SUFFIX)
endif

ifeq ($(SOUND), SDL)
ifneq ($(GRAPHICS), SDL)
$(error "SDL sound requires SDL graphics")
endif
OBJECTS += sound.$(OBJECT_SUFFIX)
endif

ifeq ($(JOYSTICK), SDL)
ifneq ($(GRAPHICS), SDL)
$(error "SDL joystick requires SDL graphics")
endif
OBJECTS += joystick.$(OBJECT_SUFFIX)
endif

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	echo $(SDL_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS) $(SDL_CFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
