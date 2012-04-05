ifndef LUA
CFLAGS += -DNO_LUA
OBJECTS = dummy.$(OBJECT_SUFFIX)
else
OBJECTS = $(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard src/lua/*.cpp))
LUA_CFLAGS += $(shell $(CROSS_PREFIX)pkg-config $(LUA) --cflags)
LUA_LDFLAGS += $(shell $(CROSS_PREFIX)pkg-config $(LUA) --libs)
OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) 
endif

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	echo $(LUA_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../include $(CFLAGS) $(LUA_CFLAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX)  *.ldflags
