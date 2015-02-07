ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags

JSON_FILES=$(wildcard *.json)
JSON_SRC=$(patsubst %.json,%.cpp,$(JSON_FILES))
JSON_OBJECTS=$(patsubst %.json,%.$(OBJECT_SUFFIX),$(JSON_FILES))

__all__.files: $(JSON_OBJECTS)  inverselist.$(OBJECT_SUFFIX)
	lua ../genfilelist.lua $^ >$@
	touch $(ALLFLAGS)

mkstubs$(DOT_EXECUTABLE_SUFFIX): mkstubs.cpp ../library/json.cpp ../library/utf8.cpp ../library/string.cpp ../library/hex.cpp  ../library/eatarg.cpp ../library/int24.cpp
	$(HOSTCC) -g -std=gnu++0x -I../../include/library -o $@ $^ $(HOSTHELPER_LDFLAGS) -Wall

mkstubsi$(DOT_EXECUTABLE_SUFFIX): mkstubsi.cpp ../library/json.cpp ../library/utf8.cpp ../library/string.cpp ../library/hex.cpp  ../library/eatarg.cpp ../library/int24.cpp
	$(HOSTCC) -g -std=gnu++0x -I../../include/library -o $@ $^  $(HOSTHELPER_LDFLAGS) -Wall

inverselist.cpp: $(JSON_FILES) mkstubsi$(DOT_EXECUTABLE_SUFFIX)
	./mkstubsi$(DOT_EXECUTABLE_SUFFIX) $^

%.cpp: %.json mkstubs$(DOT_EXECUTABLE_SUFFIX)
	./mkstubs$(DOT_EXECUTABLE_SUFFIX) $<
	mv $(patsubst %.json,%.hpp,$<) ../../include/cmdhelp

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include -Wall


.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

prebuild: __all__.files
	@true

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) $(JSON_SRC) inverselist.cpp __all__.ldflags __all__.files
	rm -f mkstubs$(DOT_EXECUTABLE_SUFFIX)
	rm -f mkstubsi$(DOT_EXECUTABLE_SUFFIX)

forcelook:
	@true
