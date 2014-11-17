ALLFILES=__all__.files
ALLFLAGS=__all__.ldflags

JSON_FILES=$(wildcard *.json)
JSON_SRC=$(patsubst %.json,%.cpp,$(JSON_FILES))
JSON_OBJECTS=$(patsubst %.json,%.$(OBJECT_SUFFIX),$(JSON_FILES))

__all__.files: $(JSON_OBJECTS)  inverselist.$(OBJECT_SUFFIX)
	lua ../genfilelist.lua $^ >$@
	touch $(ALLFLAGS)

mkstubs.exe: mkstubs.cpp ../library/json.cpp ../library/utf8.cpp ../library/string.cpp ../library/hex.cpp  ../library/eatarg.cpp ../library/int24.cpp
	$(HOSTCC) -g -std=gnu++0x -I../../include/library -o $@ $^  -lboost_regex$(HOST_BOOST_POSTFIX) -lboost_system$(HOST_BOOST_POSTFIX) -Wall

mkstubsi.exe: mkstubsi.cpp ../library/json.cpp ../library/utf8.cpp ../library/string.cpp ../library/hex.cpp  ../library/eatarg.cpp ../library/int24.cpp
	$(HOSTCC) -g -std=gnu++0x -I../../include/library -o $@ $^  -lboost_regex$(HOST_BOOST_POSTFIX) -lboost_system$(HOST_BOOST_POSTFIX) -Wall

inverselist.cpp: $(JSON_FILES) mkstubsi.exe
	./mkstubsi.exe $^

%.cpp: %.json mkstubs.exe
	./mkstubs.exe $<
	mv *.hpp ../../include/cmdhelp

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include -Wall


.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) $(JSON_SRC) inverselist.cpp __all__.ldflags __all__.files
	rm -f mkstubs.exe

forcelook:
	@true
