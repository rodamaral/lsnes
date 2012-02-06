OBJECTS=font.$(OBJECT_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	touch __all__.ldflags

.PRECIOUS: font.$(OBJECT_SUFFIX) font.cpp

font.$(OBJECT_SUFFIX): font.cpp
	$(REALCC) $(CORE_CFLAGS) -c -o $@ font.cpp

font.cpp : $(FONT_SRC)
	echo "extern const char* font_hex_data = " >font.cpp
	sed -E -f fonttransform.sed <$^ >>font.cpp
	echo ";" >>font.cpp

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) font.cpp  *.ldflags
