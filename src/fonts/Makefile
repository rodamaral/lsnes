OBJECTS=font.$(OBJECT_SUFFIX) wrapper.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS)
	lua ../genfilelist.lua $^ >$@
	touch __all__.ldflags

.PRECIOUS: font.$(OBJECT_SUFFIX) font.cpp

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include -Wall

font.cpp: $(FONT_SRC)
	echo "extern const char* font_hex_data;" >font.cpp
	echo "const char* font_hex_data = " >>font.cpp
	sed -E -f fonttransform.sed <$^ >>font.cpp
	echo ";" >>font.cpp
	touch font.cpp.dep

font.cpp.dep:
	touch font.cpp.dep

precheck:
	../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../include -- *.cpp
	@true

forcelook:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) font.cpp __all__.ldflags __all__.files
