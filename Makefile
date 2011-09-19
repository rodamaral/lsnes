FONT_SRC := unifontfull-5.1.20080820.hex
CC := g++-4.5
HOSTCC = $(CC)
OBJECTS = $(patsubst %.cpp,%.o,$(wildcard generic/*.cpp)) $(patsubst %.cpp,%.o,$(wildcard avidump/*.cpp)) fonts/font.o 
GENERIC_LIBS = -ldl -lboost_iostreams -lboost_filesystem -lboost_system -lz

PROGRAMS = lsnes.exe movieinfo.exe

CFLAGS = $(USER_CFLAGS)
HOSTCCFLAGS = $(USER_HOSTCCFLAGS)
LDFLAGS = $(GENERIC_LIBS) $(USER_LDFLAGS)

#Lua.
ifdef NO_LUA
CFLAGS += -DNO_LUA
else
OBJECTS += $(patsubst %.cpp,%.o,$(wildcard lua/*.cpp))
CFLAGS += $(shell pkg-config lua5.1 --cflags)
LDFLAGS += $(shell pkg-config lua5.1 --libs)
endif

#Platform stuff (SDL).
OBJECTS += $(patsubst %.cpp,%.o,$(wildcard SDL/*.cpp))
CFLAGS += $(shell sdl-config --cflags)
LDFLAGS += $(shell sdl-config --libs)


ifdef NO_THREADS
CFLAGS += -DNO_THREADS
endif

ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
endif


all: $(PROGRAMS)

.PRECIOUS: %.exe %.o

%.exe: %.o $(OBJECTS)
	$(CC) -o $@ $^ $(BSNES_PATH)/out/libsnes.a $(LDFLAGS)

%.o: %.cpp
	$(CC) -I. -Igeneric -g -std=c++0x -I$(BSNES_PATH) -c -o $@ $< $(CFLAGS)

fonts/font.o: fonts/$(FONT_SRC) fonts/parsehexfont.exe
	fonts/parsehexfont.exe <fonts/$(FONT_SRC) >fonts/font.cpp
	$(HOSTCC) -std=c++0x $(HOSTCCFLAGS) -c -o fonts/font.o fonts/font.cpp
	$(HOSTCC) -std=c++0x $(HOSTCCFLAGS) -o fonts/verifyhexfont.exe fonts/verifyhexfont.cpp fonts/font.o
	fonts/verifyhexfont.exe

fonts/parsehexfont.exe: fonts/parsehexfont.cpp
	$(HOSTCC) -std=c++0x $(HOSTCCFLAGS) -o $@ $^

clean:
	rm -f $(PROGRAMS) $(patsubst %.exe,%.o,$(PROGRAMS)) SDL/*.o avidump/*.o generic/*.o
