CROSS_PREFIX=
EXECUTABLE_SUFFIX = exe
OBJECT_SUFFIX = o
ARCHIVE_SUFFIX = a
FONT_SRC := unifontfull-5.1.20080820.hex

#Compilers.
CC := g++
HOSTCC = $(CC)
REALCC = $(CROSS_PREFIX)$(CC)

#Flags.
HOSTCCFLAGS = -std=gnu++0x
CFLAGS = -I$(BSNES_PATH) -Iinclude -Iavi -std=gnu++0x
LDFLAGS = -lboost_iostreams -lboost_filesystem -lboost_system -lz
PLATFORM_CFLAGS =
PLATFORM_LDFLAGS =

#Platform defs.
GRAPHICS = SDL
SOUND = SDL
JOYSTICK = SDL

#Core objects and what to build.
CORE_OBJECTS = $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/core/*.cpp)) \
	$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard avi/*.cpp)) \
	src/fonts/font.$(OBJECT_SUFFIX)
PROGRAMS = lsnes.$(EXECUTABLE_SUFFIX) movieinfo.$(EXECUTABLE_SUFFIX) lsnes-dumpavi.$(EXECUTABLE_SUFFIX) sdmp2sox.$(EXECUTABLE_SUFFIX)
all: $(PROGRAMS)

#Platform objects.
PLATFORM_OBJECTS=

#Lua.
ifndef LUA
CFLAGS += -DNO_LUA
else
CORE_OBJECTS += $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/lua/*.cpp))
CFLAGS += $(shell $(CROSS_PREFIX)pkg-config $(LUA) --cflags)
LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config $(LUA) --libs)
endif

#Threads
ifdef THREADS
ifeq ($(THREADS), YES)
CFLAGS += -DUSE_THREADS
else
ifeq ($(THREADS), NO)
CFLAGS += -DNO_THREADS
else
$(error "Bad value for THREADS (expected YES or NO)")
endif
endif
endif


#Some misc defines.
ifdef NO_TIME_INTERCEPT
CFLAGS += -DNO_TIME_INTERCEPT
else
LDFLAGS += -Wl,--wrap,time
endif
ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
endif

#Joystick.
ifeq ($(JOYSTICK), SDL)
ifneq ($(GRAPHICS), SDL)
$(error "SDL Joystick requires SDL graphics")
endif
PLATFORM_OBJECTS += src/plat-sdl/joystick.$(OBJECT_SUFFIX)
else
ifeq ($(JOYSTICK), DUMMY)
CFLAGS += -DSDL_NO_JOYSTICK
PLATFORM_OBJECTS += src/plat-dummy/joystick.$(OBJECT_SUFFIX)
else
ifeq ($(JOYSTICK), EVDEV)
CFLAGS += -DSDL_NO_JOYSTICK
PLATFORM_OBJECTS += $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/plat-evdev/*.cpp))
else
$(error "Unsupported joystick type")
endif
endif
endif

#Sound stuff.
ifeq ($(SOUND), SDL)
ifneq ($(GRAPHICS), SDL)
$(error "SDL Sound requires SDL graphics")
endif
PLATFORM_OBJECTS += src/plat-sdl/sound.$(OBJECT_SUFFIX)
else
ifeq ($(SOUND), PORTAUDIO)
PLATFORM_OBJECTS += src/plat-portaudio/sound.$(OBJECT_SUFFIX)
PLATFORM_CFLAGS += $(shell $(CROSS_PREFIX)pkg-config portaudio-2.0 --cflags)
PLATFORM_LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config portaudio-2.0 --libs)
else
ifeq ($(SOUND), DUMMY)
PLATFORM_OBJECTS += src/plat-dummy/sound.$(OBJECT_SUFFIX)
else
$(error "Unsupported sound type")
endif
endif
endif

#Graphics stuff.
ifeq ($(GRAPHICS), SDL)
PLATFORM_OBJECTS += src/plat-sdl/main.$(OBJECT_SUFFIX) src/plat-sdl/graphics.$(OBJECT_SUFFIX)
PLATFORM_CFLAGS += $(shell $(CROSS_PREFIX)sdl-config --cflags)
PLATFORM_LDFLAGS += $(shell $(CROSS_PREFIX)sdl-config --libs)
else
ifeq ($(GRAPHICS), WXWIDGETS)
PLATFORM_OBJECTS += $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/plat-wxwidgets/*.cpp))
PLATFORM_CFLAGS += $(shell $(CROSS_PREFIX)wx-config --cxxflags) $(shell $(CROSS_PREFIX)pkg-config libswscale --cflags)
PLATFORM_LDFLAGS += $(shell $(CROSS_PREFIX)wx-config --libs) $(shell $(CROSS_PREFIX)pkg-config libswscale --libs)
else
$(error "Unsupported graphics type")
endif
endif

CORE_CFLAGS=$(CFLAGS) $(USER_CFLAGS)
SUPPORT_CFLAGS=$(CORE_CFLAGS) $(PLATFORM_CFLAGS) $(USER_PLATFORM_CFLAGS)
CORE_LDFLAGS=$(BSNES_PATH)/out/libsnes.$(ARCHIVE_SUFFIX) $(LDFLAGS) $(USER_LDFLAGS)
SUPPORT_LDFLAGS=$(CORE_LDFLAGS) $(PLATFORM_LDFLAGS) $(USER_PLATFORM_LDFLAGS)
PLAT_DUMMY_OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/plat-dummy/*.cpp))
HOST_CFLAGS=$(HOSTCCFLAGS) $(USER_HOSTCCFLAGS)
HOST_LDFLAGS=$(HOSTLDFLAGS) $(USER_HOSTLDFLAGS)

.PRECIOUS: %.$(EXECUTABLE_SUFFIX) %.$(OBJECT_SUFFIX)

#Stuff compiled with core CFLAGS.
avi/%.$(OBJECT_SUFFIX): avi/%.cpp
	$(REALCC) -c -o $@ $< $(CORE_CFLAGS)
src/core/%.$(OBJECT_SUFFIX): src/core/%.cpp
	$(REALCC) -c -o $@ $< $(CORE_CFLAGS)
src/lua/%.$(OBJECT_SUFFIX): src/lua/%.cpp
	$(REALCC) -c -o $@ $< $(CORE_CFLAGS)
src/plat-dummy/%.$(OBJECT_SUFFIX): src/plat-dummy/%.cpp
	$(REALCC) -c -o $@ $< $(CORE_CFLAGS)
src/util/%.$(OBJECT_SUFFIX): src/util/%.cpp
	$(REALCC) -c -o $@ $< $(CORE_CFLAGS)

#Platform stuff to be compiled with support CFLAGS.
src/plat-evdev/%.$(OBJECT_SUFFIX): src/plat-evdev/%.cpp
	$(REALCC) -c -o $@ $< $(SUPPORT_CFLAGS)
src/plat-portaudio/%.$(OBJECT_SUFFIX): src/plat-portaudio/%.cpp
	$(REALCC) -c -o $@ $< $(SUPPORT_CFLAGS)
src/plat-sdl/%.$(OBJECT_SUFFIX): src/plat-sdl/%.cpp
	$(REALCC) -c -o $@ $< $(SUPPORT_CFLAGS)
src/plat-wxwidgets/%.$(OBJECT_SUFFIX): src/plat-wxwidgets/%.cpp
	$(REALCC) -c -o $@ $< $(SUPPORT_CFLAGS)

#lsnes main executable.
lsnes.$(EXECUTABLE_SUFFIX): $(CORE_OBJECTS) $(PLATFORM_OBJECTS)
	$(REALCC) -o $@ $^ $(SUPPORT_LDFLAGS)

#Other executables.
%.$(EXECUTABLE_SUFFIX): src/util/%.$(OBJECT_SUFFIX) $(CORE_OBJECTS) $(PLAT_DUMMY_OBJECTS)
	$(REALCC) -o $@ $^ $(CORE_LDFLAGS)

#Fonts.
src/fonts/parsehexfont.$(OBJECT_SUFFIX): src/fonts/parsehexfont.cpp
	$(HOSTCC) $(HOST_CFLAGS) -c -o $@ $^
src/fonts/parsehexfont.$(EXECUTABLE_SUFFIX): src/fonts/parsehexfont.$(OBJECT_SUFFIX)
	$(HOSTCC) $(HOST_LDFLAGS) -o $@ $^
src/fonts/font.$(OBJECT_SUFFIX): src/fonts/$(FONT_SRC) src/fonts/parsehexfont.$(EXECUTABLE_SUFFIX)
	src/fonts/parsehexfont.$(EXECUTABLE_SUFFIX) <src/fonts/$(FONT_SRC) >src/fonts/font.cpp
	$(REALCC) $(CORE_CFLAGS) -c -o $@ src/fonts/font.cpp
	$(HOSTCC) $(HOST_CFLAGS) -o src/fonts/verifyhexfont.$(EXECUTABLE_SUFFIX) src/fonts/verifyhexfont.cpp src/fonts/font.cpp
	src/fonts/verifyhexfont.$(EXECUTABLE_SUFFIX)
fonts/parsehexfont.$(EXECUTABLE_SUFFIX): fonts/parsehexfont.cpp
	$(HOSTCC) $(HOST_CFLAGS) -o $@ $^


clean:
	rm -f $(PROGRAMS) src/*.$(OBJECT_SUFFIX) src/*/*.$(OBJECT_SUFFIX)  avi/*.$(OBJECT_SUFFIX) src/fonts/font.o src/fonts/font.cpp
