EXECUTABLE_SUFFIX = exe
OBJECT_SUFFIX = o
ARCHIVE_SUFFIX = a
FONT_SRC := unifontfull-5.1.20080820.hex
CC := g++-4.6
HOSTCC = $(CC)
LUAPACKAGE=lua5.1

OBJECTS = $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard generic/*.cpp)) $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard avidump/*.cpp)) fonts/font.$(OBJECT_SUFFIX)
ifndef NO_LIBDL
GENERIC_LIBS += -ldl
endif
GENERIC_LIBS += -lboost_iostreams -lboost_filesystem -lboost_system -lz
CFLAGS = $(USER_CFLAGS)
HOSTCCFLAGS = $(USER_HOSTCCFLAGS)
LDFLAGS = $(GENERIC_LIBS) $(USER_LDFLAGS)
GRAPHICS = SDL
SOUND = SDL
JOYSTICK = SDL
PLATFORM_CFLAGS = $(CFLAGS) $(PLATFORM_USER_CFLAGS)
PLATFORM_LDFLAGS = $(LDFLAGS) $(PLATFORM_USER_LDFLAGS)

PROGRAMS = lsnes.$(EXECUTABLE_SUFFIX) movieinfo.$(EXECUTABLE_SUFFIX) lsnes-dumpavi.$(EXECUTABLE_SUFFIX) sdmp2sox.$(EXECUTABLE_SUFFIX)
all: $(PROGRAMS)

#Lua.
ifdef NO_LUA
CFLAGS += -DNO_LUA
else
OBJECTS += $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard lua/*.cpp))
ifndef NO_LUA_SEARCH
CFLAGS += $(shell pkg-config $(LUAPACKAGE) --cflags)
LDFLAGS += $(shell pkg-config $(LUAPACKAGE) --libs)
endif
endif

#Some misc defines.
ifdef NO_TIME_INTERCEPT
CFLAGS += -DNO_TIME_INTERCEPT
else
LDFLAGS += -Wl,--wrap,time
endif
ifdef NO_THREADS
CFLAGS += -DNO_THREADS
endif
ifdef USE_THREADS
CFLAGS += -DUSE_THREADS
endif
ifdef TEST_WIN32
CFLAGS += -DTEST_WIN32_CODE
endif
ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
endif
ifeq ($(JOYSTICK), SDL)
ifneq ($(GRAPHICS), SDL)
$(error "SDL Joystick requires SDL graphics")
endif
PLATFORM_OBJECTS += platform/SDL/joystick-sdl.$(OBJECT_SUFFIX)
else
ifeq ($(JOYSTICK), DUMMY)
CFLAGS += -DSDL_NO_JOYSTICK
PLATFORM_OBJECTS += platform/dummy/joystick-dummy.$(OBJECT_SUFFIX)
else
ifeq ($(JOYSTICK), EVDEV)
CFLAGS += -DSDL_NO_JOYSTICK
PLATFORM_OBJECTS += platform/evdev/joystick-evdev.$(OBJECT_SUFFIX) platform/evdev/axes.$(OBJECT_SUFFIX) platform/evdev/buttons.$(OBJECT_SUFFIX)
else
$(error "Unsupported joystick type")
endif
endif
endif

ifeq ($(SOUND), SDL)
ifneq ($(GRAPHICS), SDL)
$(error "SDL Sound requires SDL graphics")
endif
PLATFORM_OBJECTS += platform/SDL/sound-sdl.$(OBJECT_SUFFIX)
else
ifeq ($(SOUND), PORTAUDIO)
PLATFORM_OBJECTS += platform/portaudio/sound-portaudio.$(OBJECT_SUFFIX)
PLATFORM_LDFLAGS += -lportaudio
else
ifeq ($(SOUND), DUMMY)
PLATFORM_OBJECTS += platform/dummy/sound-dummy.$(OBJECT_SUFFIX)
else
$(error "Unsupported sound type")
endif
endif
endif

ifeq ($(GRAPHICS), SDL)
LSNES_MAIN = lsnes.$(OBJECT_SUFFIX)
PLATFORM_OBJECTS += platform/SDL/main-sdl.$(OBJECT_SUFFIX) platform/SDL/window-sdl.$(OBJECT_SUFFIX)
ifndef NO_SDL_SEARCH
PLATFORM_CFLAGS += $(shell sdl-config --cflags)
PLATFORM_LDFLAGS += $(shell sdl-config --libs)
endif
ifdef TEST_WIN32
PLATFORM_LDFLAGS += -lSDLmain
endif
platform/SDL/%.$(OBJECT_SUFFIX): platform/SDL/%.cpp
	$(CC) -I. -Igeneric -g -std=gnu++0x -I$(BSNES_PATH) -c -o $@ $< $(CFLAGS) $(PLATFORM_CFLAGS)
else
$(error "Unsupported graphics type")
endif

.PRECIOUS: %.$(EXECUTABLE_SUFFIX) %.$(OBJECT_SUFFIX)


lsnes.$(EXECUTABLE_SUFFIX): $(OBJECTS) $(PLATFORM_OBJECTS)
	$(CC) -o $@ $^ $(BSNES_PATH)/out/libsnes.$(ARCHIVE_SUFFIX) $(PLATFORM_LDFLAGS)

%.$(EXECUTABLE_SUFFIX): %.$(OBJECT_SUFFIX) $(OBJECTS) $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard platform/dummy/*.cpp))
	$(CC) -o $@ $^ $(BSNES_PATH)/out/libsnes.$(ARCHIVE_SUFFIX) $(LDFLAGS)

%.$(OBJECT_SUFFIX): %.cpp
	$(CC) -I. -Igeneric -g -std=gnu++0x -I$(BSNES_PATH) -c -o $@ $< $(CFLAGS)

fonts/font.$(OBJECT_SUFFIX): fonts/$(FONT_SRC) fonts/parsehexfont.$(EXECUTABLE_SUFFIX)
	fonts/parsehexfont.$(EXECUTABLE_SUFFIX) <fonts/$(FONT_SRC) >fonts/font.cpp
	$(CC) -std=gnu++0x $(HOSTCCFLAGS) -c -o fonts/font.$(OBJECT_SUFFIX) fonts/font.cpp
	$(HOSTCC) -std=gnu++0x $(HOSTCCFLAGS) -o fonts/verifyhexfont.$(EXECUTABLE_SUFFIX) fonts/verifyhexfont.cpp fonts/font.cpp
	fonts/verifyhexfont.$(EXECUTABLE_SUFFIX)

fonts/parsehexfont.$(EXECUTABLE_SUFFIX): fonts/parsehexfont.cpp
	$(HOSTCC) -std=gnu++0x $(HOSTCCFLAGS) -o $@ $^

clean:
	rm -f $(PROGRAMS) $(patsubst %.$(EXECUTABLE_SUFFIX),%.$(OBJECT_SUFFIX),$(PROGRAMS)) platform/*/*.$(OBJECT_SUFFIX) avidump/*.$(OBJECT_SUFFIX) generic/*.$(OBJECT_SUFFIX) lua/*.$(OBJECT_SUFFIX) fonts/font.o fonts/font.cpp
