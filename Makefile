EXECUTABLE_SUFFIX = exe
OBJECT_SUFFIX = o
ARCHIVE_SUFFIX = a
FONT_SRC := unifontfull-5.1.20080820.hex
CC := g++-4.5
HOSTCC = $(CC)


OBJECTS = $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard generic/*.cpp)) $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard avidump/*.cpp)) fonts/font.$(OBJECT_SUFFIX) 
GENERIC_LIBS = -ldl -lboost_iostreams -lboost_filesystem -lboost_system -lz
CFLAGS = $(USER_CFLAGS)
HOSTCCFLAGS = $(USER_HOSTCCFLAGS)
LDFLAGS = $(GENERIC_LIBS) $(USER_LDFLAGS)
PLATFORM = SDL
PLATFORM_CFLAGS = $(CFLAGS)
PLATFORM_LDFLAGS = $(LDFLAGS)

PROGRAMS = lsnes.$(EXECUTABLE_SUFFIX) movieinfo.$(EXECUTABLE_SUFFIX) lsnes-dumpavi.$(EXECUTABLE_SUFFIX)


#Lua.
ifdef NO_LUA
CFLAGS += -DNO_LUA
else
OBJECTS += $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard lua/*.cpp))
CFLAGS += $(shell pkg-config lua5.1 --cflags)
LDFLAGS += $(shell pkg-config lua5.1 --libs)
endif

#Some misc defines.
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

all: $(PROGRAMS)
.PRECIOUS: %.$(EXECUTABLE_SUFFIX) %.$(OBJECT_SUFFIX)


#Platform stuff.
ifeq ($(PLATFORM), SDL)
LSNES_MAIN = lsnes.$(OBJECT_SUFFIX)
PLATFORM_OBJECTS += $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard SDL/*.cpp))
PLATFORM_CFLAGS += $(shell sdl-config --cflags)
PLATFORM_LDFLAGS += $(shell sdl-config --libs)
ifdef TEST_WIN32
PLATFORM_LDFLAGS += -lSDLmain
endif
SDL/%.$(OBJECT_SUFFIX): SDL/%.cpp
	$(CC) -I. -Igeneric -g -std=gnu++0x -I$(BSNES_PATH) -c -o $@ $< $(CFLAGS) $(PLATFORM_CFLAGS)
lsnes.$(OBJECT_SUFFIX): lsnes.cpp
	$(CC) -I. -Igeneric -g -std=gnu++0x -I$(BSNES_PATH) -c -o $@ $< $(CFLAGS) $(PLATFORM_CFLAGS)
lsnes.$(EXECUTABLE_SUFFIX): lsnes.$(OBJECT_SUFFIX) $(OBJECTS) $(PLATFORM_OBJECTS)
	$(CC) -o $@ $^ $(BSNES_PATH)/out/libsnes.$(ARCHIVE_SUFFIX) $(LDFLAGS) $(PLATFORM_LDFLAGS)
else
lsnes.$(OBJECT_SUFFIX):
	echo "Unsupported platform" $(PLATFORM)
	false
endif


%.$(EXECUTABLE_SUFFIX): %.$(OBJECT_SUFFIX) $(OBJECTS) dummy/window-dummy.$(OBJECT_SUFFIX)
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
	rm -f $(PROGRAMS) $(patsubst %.$(EXECUTABLE_SUFFIX),%.$(OBJECT_SUFFIX),$(PROGRAMS)) SDL/*.$(OBJECT_SUFFIX) avidump/*.$(OBJECT_SUFFIX) generic/*.$(OBJECT_SUFFIX)
